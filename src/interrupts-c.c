/* 
 * interrupts-c.c: some interrupt support code.  Default handlers, 
 * interrupt installation.
 */
#include "rpi.h"
#include "rpi-interrupts.h"
#include "interrupts-asm.h"
#include "mmu.h"
#include "memmap-constants.h"

#define DEBUG_HANDLE_DATA_ABORTS 1
#define DEBUG_PRINT_DATA_ABORTS 1

#define UNHANDLED(msg,r) \
	panic("ERROR: unhandled exception <%s> at PC=%x\n", msg,r)

void interrupt_vector(unsigned pc) {
	UNHANDLED("general interrupt", pc);
}

void fast_interrupt_vector(unsigned pc) {
	UNHANDLED("fast", pc);
}

void software_interrupt_vector(unsigned pc) {
	UNHANDLED("soft interrupt", pc);
}

void reset_vector(unsigned pc) {
	UNHANDLED("reset vector", pc);
}

void undefined_instruction_vector(unsigned pc) {
	UNHANDLED("undefined instruction", pc);
}

void prefetch_abort_vector(unsigned pc) {
	UNHANDLED("prefetch abort", pc);
}

void data_abort_vector(unsigned pc) {
    // cpsr_print_mode(cpsr_read()); // Will be in abort mode
#if DEBUG_PRINT_DATA_ABORTS == 1
    printDataAbort(pc);
#endif

#if DEBUG_HANDLE_DATA_ABORTS == 1
    unsigned faultval = get_data_fault_status_reg();
    if (fault_status_has_valid_far(faultval)) {
        unsigned address = get_fault_address_reg();
        if (address == 0x0) { // null ptr exception
            printk("Illegal access of 0x0. Aborting...");
            clean_reboot();
        }

        if (address < SYS_STACK_ADDR_FINE && address > (unsigned)kmalloc_heap_end()) {
            // Within system stack bounds and above heap, initiate a page miss
            // Could make more fine grained
            handle_page_miss(address);
        } else if (address < (unsigned)kmalloc_heap_start() || address > (unsigned)kmalloc_heap_end()) {
            printk("Outside of heap <range 0x%x to 0x%x>, fatal error. Quitting...\n", 
                (unsigned)kmalloc_heap_start(), 
                (unsigned)kmalloc_heap_end());
            clean_reboot(); // TODO: fail gracefully?
        }
    }
#endif
}

static int int_intialized_p = 0;

// wait: which was +8
enum {
    RESET_INC = -1,     // cannot return from reset
    UNDEFINED_INC = 4,  // 
    SWI_INC = 4,        // address of instruction after SWI
    PREFETCH_INC = 4,        // aborted instruction + 4
};

// call before int_init() to override.
void int_set_handler(int t, interrupt_t handler) {
    interrupt_t *src = (void*)&_interrupt_table;
    demand(t >= RESET_INT && t < FIQ_INT && t != INVALID, invalid type);
    src[t] = handler;

    demand(!int_intialized_p, must be called before copying vectors);
}

/*
 * Copy in interrupt vector table and FIQ handler _table and _table_end
 * are symbols defined in the interrupt assembly file, at the beginning
 * and end of the table and its embedded constants.
 */
static void install_handlers(void) {
        unsigned *dst = (void*)RPI_VECTOR_START,
                 *src = &_interrupt_table,
                 n = &_interrupt_table_end - src;
        for(int i = 0; i < n; i++)
                dst[i] = src[i];
}

#include "cp15-arm.h"

void interrupts_init(void) {
    // BCM2835 manual, section 7.5: turn off all GPIO interrupts.
    PUT32(INTERRUPT_DISABLE_1, 0xffffffff);
    PUT32(INTERRUPT_DISABLE_2, 0xffffffff);
    system_disable_interrupts();
    cp15_barrier();

    // setup the interrupt vectors.
    install_handlers();
    int_intialized_p = 1;
}

// Helpers for extracting data from the data fault status register
// (p. B4-43)
int WIF_WRITE(unsigned faultval) { return faultval & 0b1 << 11; }
int WIF_READ(unsigned faultval) { return !WIF_WRITE(faultval); }
int WFAULT_DOMAIN(unsigned faultval) { return ((faultval & 0b1111 << 4) >> 4); }
int WFAULT_STATUS(unsigned faultval) { 
    return (faultval & 0b1111) // First four bits of the status
        | ((faultval & 0b1 << 10) >> 6); // The fifth bit (go to position 10, then 4)
}

// Helper for turning the fault status into a string error (p. B4-20).
char *fault_status_to_str(unsigned faultval) {
    switch (WFAULT_STATUS(faultval)) {
        /* Fault sources arranged by highest priority and grouped by type. */
        case 0b00001: return "Alignment issue";

        case 0b00000: return "PMSA - TLB miss (MPU)";
        case 0b00100: return "Instruction cache maintenance operation fault";

        case 0b01100: return "1st level external abort on translation";
        case 0b01110: return "2nd level external abort on translation";

        case 0b00101: return "Section translation";
        case 0b00111: return "Page translation";

        case 0b01001: return "Section domain fault";
        case 0b01011: return "Page domain fault";

        case 0b01101: return "Section permission fault";
        case 0b01111: return "Page permission fault";

        case 0b01000: return "Precise external abort";       
        case 0b10100: return "TLB lock";
        case 0b11010: return "Coprocessor data abort";
        case 0b10110: return "Imprecise data abort";
        case 0b11000: return "Parity error exception";
        case 0b00010: return "Debug event";
    }
    return "Unknown status";
}

// Given fault status register, return whether or not we can trust the domain value.
int fault_status_has_valid_domain(unsigned faultval) {
    switch (WFAULT_STATUS(faultval)) {
        case 0b01110: // 2nd level external abort on translation
        case 0b00111: // Page translation
        case 0b01001: // Section domain fault
        case 0b01011: // Page domain fault
        case 0b01101: // Section permission fault
        case 0b01111: // Page permission fault
        case 0b00010: // Debug event
            return 1;
    }
    return 0;
}

// Given fault status register, return whether or not we can trust the FAR (faulting address register).
int fault_status_has_valid_far(unsigned faultval) {
    switch (WFAULT_STATUS(faultval)) {
        case 0b10100: // TLB lock
        case 0b11010: // Coprocessor data abort
        case 0b10110: // Imprecise data abort
        case 0b11000: // Parity error exception
        case 0b00010: // Debug event
            return 0;
    }
    return 1;
}

// Print a binary value in nDigits digits with leading zeroes if needed
void printBinary(unsigned val, unsigned nDigits) {
    for (int i = nDigits - 1; i >= 0; i--) {
        printk("%b", (val & 0b1 << i) >> i);
    }
}

// Helper for dumping information about a data abort
void printDataAbort(unsigned pc) {
    unsigned faultval = get_data_fault_status_reg();
    
    printk("------ Data abort! ------\n", pc);
    printk("pc:\t 0x%x\n", pc);

    if (WIF_READ(faultval)) printk("Type:\t Read access\n");
    else printk("Type:\t Write access\n");

    /* Faulting address */
    if (fault_status_has_valid_far(faultval)) {
        printk("Address: 0x%x\n", get_fault_address_reg());
    } else {
        printk("Address: 0x%x (invalid!)\n", get_fault_address_reg());
    }

    /* Domain */
    if (fault_status_has_valid_domain(faultval)) {
        printk("Domain:\t %d\n", WFAULT_DOMAIN(faultval));
    } else {
        printk("Domain:\t %d (invalid!)\n", WFAULT_DOMAIN(faultval));
    }
    
    /* Status */
    printk("Status:\t 0b");
    printBinary(WFAULT_STATUS(faultval), 5);
    printk(" [%s]\n", fault_status_to_str(faultval));
    // printk("Status:\t 0b%5b [%s]\n", WFAULT_STATUS(faultval), fault_status_to_str(faultval));

    printk("------- End debug -------\n", pc);
}