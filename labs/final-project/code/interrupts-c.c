/* 
 * interrupts-c.c: some interrupt support code.  Default handlers, 
 * interrupt installation.
 */
#include "rpi.h"
#include "rpi-interrupts.h"

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
	// UNHANDLED("data abort", pc); // panic will kill it?
    unsigned faultval = get_data_fault_status_reg();
    
    if (WIF_READ(faultval)) printk("This access was a read access.\n");
    else printk("This was a write access.\n");

    printk("Domain\t %d\n", WFAULT_DOMAIN(faultval));
    printk("Status\t %b [%s]\n", WFAULT_STATUS(faultval), fault_status_to_str(WFAULT_STATUS(faultval)));
    printk("Address\t %x\n", get_fault_address_reg());

    printk("ERROR: unhandled exception <data abort> at PC=%x\n", pc); // Should trudge on
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
int WIF_WRITE(unsigned faultval) { return faultval & (0b1 << 11); }
int WIF_READ(unsigned faultval) { return !WIF_WRITE(faultval); }
int WFAULT_DOMAIN(unsigned faultval) { return (faultval & (0b1111 << 4) >> 4 ); }
int WFAULT_STATUS(unsigned faultval) { 
    return (faultval & 0b111) // First three bits of the status
        | (faultval & (0b1 << 10) >> 6); // The fourth bit
}

char *fault_status_to_str(int fault_status) {
    switch (fault_status) {
        case 0b00001: return "Alignment issue";
        case 0b00101: return "Section translation | domain invalid | FAR valid";
        // TODO: fill out the other cases
    }
    return "Unknown status";
}