/*
 * rpi-vm: Implementing VM on an rpi
 * ---
 * Author: Garrick Fernandez
 * Sources: ARMv6 reference manual, CS140E (Dawson and Holly), dwelch67/bare-metal-pi
 */
#include "rpi.h"
#include "cp15-arm.h"
#include "mmu.h"
#include "interrupts-asm.h"
#include "rpi-interrupts.h"
#include "memmap-constants.h"

/****************************************************************************************
 * helper code to help set up address space environment.
 */
#include "bvec.h"

// The environment struct seems to encode the information for an environment.
typedef struct env {
    uint32_t pid,
             domain,
             asid;

    // the domain register.
    uint32_t domain_reg;
    fld_t *pt;
} env_t;

static bvec_t dom_v, asid_v, env_v;
static uint32_t pid_cnt;

#define MAX_ENV 8
static env_t envs[MAX_ENV];

void env_init(void) {
    dom_v = bvec_mk(1,16);
    asid_v = bvec_mk(1,64);
    env_v = bvec_mk(0,MAX_ENV);
}

env_t *env_alloc(void) {
    env_t *e = &envs[bvec_alloc(&env_v)];

    e->pt = mmu_pt_alloc(4096);
    e->pid = ++pid_cnt;
    e->domain = bvec_alloc(&dom_v);
    e->asid = bvec_alloc(&asid_v);

    // default: can override.
    e->domain_reg = 0b01 << e->domain*2; // Determine the register to go to; client (accesses checked)
    printk("env domain (1-16): %d\nenv domain reg fill: %b", e->domain, e->domain_reg);
    return e;
}

void env_free(env_t *e) {
    unsigned n = e - &envs[0];
    demand(n < MAX_ENV, freeing unallocated pointer!);

    bvec_free(&dom_v, e->domain);
    bvec_free(&asid_v, e->asid);
    bvec_free(&env_v, n);

    // not sure how to free pt.  ugh.
}

// GF: seems to have appropriate domain switching here...why our domain reg, then ~0UL? accounting for idea that we're not handling domains yet?
void env_switch_to(env_t *e) {
    cp15_domain_ctrl_wr(e->domain_reg);
    // cp15_domain_ctrl_wr(~0UL); // Should trigger a secion domain fault: check writing the reg with mmu on in manual, as well as surfacing correct error code

    cp15_set_procid_ttbr0(e->pid << 8 | e->asid, e->pt); // Ch. B2

    unsigned pid,asid;
    mmu_get_curpid(&pid, &asid);
    printk("pid=%d, expect=%d\n", pid, e->pid);
    printk("asid=%d, expect=%d\n", asid, e->asid);

    assert(pid == e->pid);
    assert(asid == e->asid);
    // mmu_asid_print();

    mmu_enable();
}


/*************************************************************************************
 * your code
 */

unsigned cpsr_read(void); // Defined in asm

unsigned cpsr_read_c(void) { 
    return cpsr_read() & 0b11111; 
}
int mmu_is_on(void) {
    return cp15_ctrl_reg1_rd().MMU_enabled;
}
void cpsr_print_mode(unsigned cpsr_r) {
    switch(cpsr_r & 0b11111) {
    case USER_MODE: printk("user mode\n"); break;
    case FIQ_MODE: printk("fiq mode\n"); break;
    case IRQ_MODE: printk("irq mode\n"); break;
    case SUPER_MODE: printk("supervisor mode\n"); break;
    case ABORT_MODE: printk("abort mode\n"); break;
    case UNDEF_MODE: printk("undef mode\n"); break;
    case SYS_MODE: printk("sys mode\n"); break;
    default: panic("invalid cpsr: %b\n", cpsr_r);
    }
}

// you will call this with the pc of the SWI instruction, and the saved registers
// in saved_regs. r0 at offset 0, r1 at offset 1, etc.
void handle_swi(uint8_t sysno, uint32_t pc, uint32_t *saved_regs) {
    printk("sysno=%d\n", sysno);
    printk("\tcpsr =%x\n", cpsr_read());
    assert(cpsr_read_c() == SUPER_MODE);

    // check that the stack is in-bounds.
    int i;
    assert(&i < (int*)SWI_STACK_ADDR);
    assert(&i > (int*)SYS_STACK_ADDR);

    printk("\treturn=%x stack=%x\n", pc, saved_regs);
    printk("arg[0]=%d, arg[1]=%d, arg[2]=%d, arg[3]=%d\n", 
                saved_regs[0], 
                saved_regs[1], 
                saved_regs[2], 
                saved_regs[3]);

    saved_regs[0] = 13;
    return;
}

/****************************************************************************************
 * part0, implement:
 *  - unsigned cpsr_read(void);
 *  - swi_setup_stack: set the stack pointer in SUPER mode to SWI_STACK_ADDR
 *  - swi_asm:  issue a SWI exception.
 *  - exception handling: write an SWI exception handler based on lab7-interrupts/timer-int
 */

// you need to define these.
unsigned cpsr_read(void);
int swi_asm1(int arg0, int arg1, int arg2, int arg3);
void swi_setup_stack(unsigned stack_addr);
int swi_asm2(int arg0, int arg1, int arg2, int arg3); // A new syscall appears!
int swi_asm3();

// don't modify this: it should run fine when everything works.
void int_part0(void) {
    interrupts_init();
    swi_setup_stack(SWI_STACK_ADDR);

    printk("about to do a SWI\n");
    int res = swi_asm1(1,2,3,4);
    printk("done with SWI, result = %d\n", res);
}

/*******************************************************************************
 * part1
 */

/*
 * for this: run with virtual memory.  actually should work out of the box.
 * then start tuning.  
 *  - write a tight loop that calls a no-op syscall.
 *  - start making it very fast.
 * you shouldn't have to touch much of this code.
 */
void int_part1(void) {
    interrupts_init();
    env_init();
    mmu_init();
    assert(cpsr_read_c() == SYS_MODE);

    env_t *e = env_alloc();

    // map the sections you need. (GF)
    mmu_map_section(e->pt, 0x0, 0x0)->domain = e->domain; // 0x0 is where our code is
    mmu_map_section(e->pt, SWI_STACK_ADDR, SWI_STACK_ADDR)->domain = e->domain; // SWI_STACK_ADDR is where the SWI stack is
    mmu_map_section(e->pt, SYS_STACK_ADDR, SYS_STACK_ADDR)->domain = e->domain;
    mmu_map_section(e->pt, INT_STACK_ADDR, INT_STACK_ADDR)->domain = e->domain;

    // gpio
    mmu_map_section(e->pt, 0x20000000, 0x20000000)->domain = e->domain;
    mmu_map_section(e->pt, 0x20200000, 0x20200000)->domain = e->domain;

    env_switch_to(e);
    assert(cpsr_read_c() == SYS_MODE);
    assert(mmu_is_on());

    int test1 = *((int *)0x99999999999); // Test deref of unmapped region, remove the "- 4" to go straight to the boundary
    printk("heyo: %d\n", test1);
    // Cool!
    
    swi_setup_stack(SWI_STACK_ADDR);

    uint32_t cpsr_before = cpsr_read_c();
    uint32_t reg1_before = cp15_ctrl_reg1_rd_u32();
    printk("about to call swi\n");
    printk("swi_asm = %d\n", swi_asm1(1,2,3,4));
    assert(cpsr_before == cpsr_read_c());
    assert(reg1_before == cp15_ctrl_reg1_rd_u32());

    printk("\n **about to call swi2\n");
    printk("swi_asm = %d\n", swi_asm2(1,2,3,4));
    assert(cpsr_before == cpsr_read_c());
    assert(reg1_before == cp15_ctrl_reg1_rd_u32());

    // have to disable mmu before reboot.  probably should build in.
    mmu_disable();
    clean_reboot();
}

void int_part2(void) {
    printk("PART 2\n");
    interrupts_init();
    env_init();
    mmu_init();
    assert(cpsr_read_c() == SYS_MODE);

    env_t *e = env_alloc();

    // map the sections you need. (GF)
    fld_t * p = mmu_map_section(e->pt, 0x0, 0x0);
    p->domain = e->domain;
    fld_cache_on(p);
    fld_writeback_on(p);
    
    p = mmu_map_section(e->pt, SWI_STACK_ADDR, SWI_STACK_ADDR);
    p->domain = e->domain;
    fld_cache_on(p);
    fld_writeback_on(p);

    p = mmu_map_section(e->pt, SYS_STACK_ADDR, SYS_STACK_ADDR);
    p->domain = e->domain;
    fld_cache_on(p);
    fld_writeback_on(p);
    
    p = mmu_map_section(e->pt, INT_STACK_ADDR, INT_STACK_ADDR);
    p->domain = e->domain;
    fld_cache_on(p);
    fld_writeback_on(p);

    // gpio
    mmu_map_section(e->pt, 0x20000000, 0x20000000)->domain = e->domain;
    mmu_map_section(e->pt, 0x20200000, 0x20200000)->domain = e->domain;

    env_switch_to(e);
    assert(cpsr_read_c() == SYS_MODE);
    assert(mmu_is_on());
    
    swi_setup_stack(SWI_STACK_ADDR);

    // uint32_t cpsr_before = cpsr_read_c();
    // uint32_t reg1_before = cp15_ctrl_reg1_rd_u32();

    printk("no caching...\n");
    unsigned start = timer_get_time();
    for (int i = 0; i < 10000; i++) {
        // printk("about to call swi\n");
        // printk("swi_asm = %d\n", swi_asm1(1,2,3,4));
        swi_asm3();
        // assert(cpsr_before == cpsr_read_c());
        // assert(reg1_before == cp15_ctrl_reg1_rd_u32());
    }
    unsigned end = timer_get_time();
    printk("end - start: %u\n", end - start);

    printk("with caching...\n");
    mmu_all_cache_on();
    // cpsr_before = cpsr_read_c(); // NEED to reset the cpsr_before and reg1_before to make it work
    // reg1_before = cp15_ctrl_reg1_rd_u32();
    start = timer_get_time();
    for (int i = 0; i < 10000; i++) {
        // printk("about to call swi\n");
        // printk("swi_asm = %d\n", swi_asm1(1,2,3,4));
        swi_asm3();
        // assert(cpsr_before == cpsr_read_c());
        // assert(reg1_before == cp15_ctrl_reg1_rd_u32());
    }
    end = timer_get_time();
    printk("w/ cache on: end - start: %u\n", end - start);

    // have to disable mmu before reboot.  probably should build in.
    mmu_disable();
    clean_reboot();
}

// VM tests to run.
void vm_tests() {
    printk("============================\n");
    printk("=== Virtual Memory Tests ===\n");
    printk("============================\n");
    env_init();
    mmu_init();
    assert(cpsr_read_c() == SYS_MODE);
    env_t *e = env_alloc();

// Define a section to decide which test to run.
#define VM_PART1 0
#define VM_PART2 1
#define VM_PART3 0
#define VM_PART4 0
#define VM_PART5 0

#if VM_PART1 == 1
    printk("\n*** Test 1 ***\n\n");
    printk("> Should be able to turn on and off VM.\n");

    *((char *)0x400) = 42;

    // Just map our section
    mmu_map_section(e->pt, 0x0, 0x0)->domain = e->domain;

    env_switch_to(e); // calls mmu_enable();
    assert(mmu_is_on());
    assert(*((char *)0x400) == 42);
    mmu_disable();
    assert(!mmu_is_on());

    printk("> End of test!\n");
#endif

#if VM_PART2 == 1
    printk("\n*** Test 2 ***\n\n");
    printk("> Should be able to turn on VM, map a section, then access.\n");

    unsigned part2_base = 0x100000;
    *((char *)0x400) = 42;
    *((char *)(part2_base + 0x400)) = 137;

    // Just map our section
    mmu_map_section(e->pt, 0x0, 0x0)->domain = e->domain;
    // Need to map GPIO for communication
    mmu_map_section(e->pt, 0x20000000, 0x20000000)->domain = e->domain;
    mmu_map_section(e->pt, 0x20200000, 0x20200000)->domain = e->domain;
    // Need to map interrupt stack to jump to handler code. If you comment out, hangs at the interrupt.
    // The stack grows downwards, so we allocate the section below it.
    mmu_map_section(e->pt, INT_STACK_ADDR - ADDRESSES_PER_MB, 
        INT_STACK_ADDR - ADDRESSES_PER_MB)->domain = e->domain;
    // kmalloc() allocates the page table here; should also be acessible in VM!
    mmu_map_section(e->pt, MAX_STACK_ADDR, MAX_STACK_ADDR)->domain = e->domain;

    env_switch_to(e); // calls mmu_enable();
    assert(mmu_is_on());
    printk("> MMU turned on successfully.\n");

    assert(*((char *)0x400) == 42);
    // Should fault when uncommented
    char c = *((char *)part2_base + 0x400);
    printk("Accessing data... <%d>\n", c);

    // A write access (should be)
    PUT32(part2_base + 0x400, 12);

    printk("> Mapping a section with VM enabled.\n");
    fld_t *fld = mmu_map_section(e->pt, part2_base, part2_base);
    fld->domain = e->domain;
    // fld->AP = 0b00; // Generate section permission fault
    c = *((char *)part2_base + 0x400);
    printk("Accessing data... <%d>\n", c);
    assert(*((char *)(part2_base + 0x400)) == 137);
    
    mmu_disable();
    assert(!mmu_is_on());

    printk("> End of test!\n");
#endif

#if VM_PART3 == 1
    printk("\n*** Test 3 ***\n\n");
    printk("> Should be able to allocate small pages.\n");

    unsigned part3_base = 0x100000;
    *((char *)(part3_base + 0x400)) = 125;

    // Just map our section
    mmu_map_section(e->pt, 0x0, 0x0)->domain = e->domain;
    // Need to map GPIO for communication
    mmu_map_section(e->pt, 0x20000000, 0x20000000)->domain = e->domain;
    // mmu_map_section(e->pt, 0x20100000, 0x20100000)->domain = e->domain; // stderr seems to feed through here
    mmu_map_section(e->pt, 0x20200000, 0x20200000)->domain = e->domain;
    // Need to map interrupt stack to jump to handler code. If you comment out, hangs at the interrupt.
    // The stack grows downwards, so we allocate the section below it.
    mmu_map_section(e->pt, INT_STACK_ADDR - ADDRESSES_PER_MB, 
        INT_STACK_ADDR - ADDRESSES_PER_MB)->domain = e->domain;
    // kmalloc() allocates the page table here; should also be acessible in VM!
    mmu_map_section(e->pt, MAX_STACK_ADDR, MAX_STACK_ADDR)->domain = e->domain;

    env_switch_to(e); // calls mmu_enable();
    assert(mmu_is_on());
    printk("> MMU turned on successfully.\n");

    // Should fault when uncommented
    char c = *((char *)part3_base + 0x400);
    printk("Accessing data... <%d>\n", c);

    printk("> Mapping a small page with VM enabled.\n");
    mmu_map_sm_page(e->pt, part3_base, part3_base); // TODO: Add domain

    c = *((char *)part3_base + 0x400);
    // c = *((char *)part3_base + 0x1000 - 4); // This is right on the boundary of the small page
    // c = *((char *)part3_base + 0x1000); // Should cause a fault for small page...
    printk("Accessing data... <%d>\n", c);
    assert(*((char *)(part3_base + 0x400)) == 125);
    
    mmu_disable();
    assert(!mmu_is_on());

    printk("> End of test!\n");
#endif

#if VM_PART4 == 1
    printk("\n*** Test 4 ***\n\n");
    printk("> Should be able to allocate large pages.\n");

    unsigned part4_base = 0x100000;
    *((char *)(part4_base + 0x400)) = 66;
    *((char *)part4_base + 0x10000 - 4) = 137;

    // Just map our section
    mmu_map_section(e->pt, 0x0, 0x0)->domain = e->domain;
    // Need to map GPIO for communication
    mmu_map_section(e->pt, 0x20000000, 0x20000000)->domain = e->domain;
    // mmu_map_section(e->pt, 0x20100000, 0x20100000)->domain = e->domain; // stderr seems to feed through here
    mmu_map_section(e->pt, 0x20200000, 0x20200000)->domain = e->domain;
    // Need to map interrupt stack to jump to handler code. If you comment out, hangs at the interrupt.
    // The stack grows downwards, so we allocate the section below it.
    mmu_map_section(e->pt, INT_STACK_ADDR - ADDRESSES_PER_MB, 
        INT_STACK_ADDR - ADDRESSES_PER_MB)->domain = e->domain;
    // kmalloc() allocates the page table here; should also be acessible in VM!
    mmu_map_section(e->pt, MAX_STACK_ADDR, MAX_STACK_ADDR)->domain = e->domain;

    printk("> Mapping a large page before turning on VM.\n");
    mmu_map_lg_page(e->pt, part4_base, part4_base); // TODO: Add domain, protection bits

    env_switch_to(e); // calls mmu_enable();
    assert(mmu_is_on());
    printk("> MMU turned on successfully.\n");

    // Should fault when uncommented
    char c = *((char *)part4_base + 0x400);
    printk("Accessing data... <%d>\n", c);

    printk("Address of something on the stack... <0x%x>\n", &c); // Where is the stack?
    c = *((char *)&c);
    printk("Accessing data... <%d>\n", c);

    // Does mapping under VM mean that TLB is invalidated?
    // printk("> Mapping a large page with VM enabled.\n");
    // mmu_map_lg_page(e->pt, part4_base, part4_base); // TODO: Add domain, protection bits

    // cp15_domain_ctrl_wr(~0UL); // Seems okay to write to CP15 while VM is on!

    c = *((char *)part4_base + 0x400);
    c = *((char *)part4_base + 0x10000 - 4); // This is right on the boundary of the large page
    // c = *((char *)part4_base + 0x10000); // Should cause a fault for large page...
    printk("Accessing data... <%d>\n", c);
    assert(*((char *)(part4_base + 0x400)) == 66);
    assert(*((char *)(part4_base + 0x10000 - 4)) == 137);
    
    mmu_disable();
    assert(!mmu_is_on());

    printk("> End of test!\n");
#endif

// Test 5
#if VM_PART5 == 1
    printk("\n*** Test 5 ***\n\n");
    printk("> Dereferencing nullptr should yield an error.\n");

    unsigned part5_base = 0x100000;
    *((char *)(part5_base + 0x400)) = 21;
    
    mmu_map_section(e->pt, 0x0, 0x0)->domain = e->domain;
    // Need to map GPIO for communication
    mmu_map_section(e->pt, 0x20000000, 0x20000000)->domain = e->domain;
    // mmu_map_section(e->pt, 0x20100000, 0x20100000)->domain = e->domain; // stderr seems to feed through here
    mmu_map_section(e->pt, 0x20200000, 0x20200000)->domain = e->domain;
    // Need to map interrupt stack to jump to handler code. If you comment out, hangs at the interrupt.
    // The stack grows downwards, so we allocate the section below it.
    mmu_map_section(e->pt, INT_STACK_ADDR - ADDRESSES_PER_MB, 
        INT_STACK_ADDR - ADDRESSES_PER_MB)->domain = e->domain;
    // kmalloc() allocates the page table here; should also be acessible in VM!
    mmu_map_section(e->pt, MAX_STACK_ADDR, MAX_STACK_ADDR)->domain = e->domain;

    env_switch_to(e); // calls mmu_enable();
    assert(mmu_is_on());
    printk("> MMU turned on successfully.\n");

    // Should fault when uncommented, should go to interrupt table at PA 0x0
    char c = *((char *)part5_base + 0x400);
    printk("Accessing data... <%d>\n", c);

    printk("> Mapping a small page with VM enabled.\n");
    mmu_map_lg_page(e->pt, part5_base, part5_base); // TODO: Add domain

    c = *((char *)part5_base + 0x400);
    printk("Accessing data... <%d>\n", c);
    assert(*((char *)(part5_base + 0x400)) == 21);

    // TODO: Swap domain to client

    cpsr_print_mode(cpsr_read_c()); // We ought to be in sys mode

    // What if we dereference nullptr itself? (We may be in system mode)
    c = *((char *)0x0);
    printk("Accessing data... <%d>\n", c);
    
    mmu_disable();
    assert(!mmu_is_on());

    printk("> End of test!\n");
#endif

    env_free(e);
}

void syscall_tests() {
    return;
}

// Main entry point for program
void notmain() {
    // Initialize UART, enable interrupts
    uart_init();
    interrupts_init();

    // start the heap after the max stack address
    kmalloc_set_start(MAX_STACK_ADDR);

    // implement swi interrupts without vm
    // int_part1();

    cpsr_print_mode(cpsr_read());

    vm_tests();
    syscall_tests();

    clean_reboot();
}
