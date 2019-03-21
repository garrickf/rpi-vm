/*
 * File: constants for mapping memory in the rpi
 * ---
 * Sets the definitive regions for the rpi kernel (some are for testing, others
 * are for the full fledged version). We want some of the kernel's segments to have an
 * established mapping so that we always know where to find them.
 * 
 * Current mapping (In virtual memory):
 * ---------------------------------------------------
 * ???              More user space stuff
 * 0x408000:        ARMBASE <user code>
 * 
 * 0x400000:        Start of user space...
 *                  System stack start (grows down)
 * ---------
 * 0x140000:        Heap start (grows up)          
 * 0x130000:        Stack start for interrupts (64KB)
 * 0x120000:        <Bottom of stack>
 *                  Stack start for syscalls (64KB)
 * 0x110000:        <Bottom of stack, with leeway>
 * ---------
 * 0x108000:        <End kernel code>
 *                  ^^^^^^^^^^^^^^^^^
 * 0x  8000:        Kernel code (1MB max, let's hope)
 * 0x     0:        Interrupt table
 * ---------------------------------------------------
 */

// Fiddle this to change constants (good for interrupt handlers, which jump to those constants)
#define RUN_ADVANCED 1

#define ADDRESSES_PER_MB    0x100000
#define ADDRESSES_PER_64KB  0x10000
#define ADDRESSES_PER_4KB   0x1000

// Where the kernel and other executables ought to start (in VM)
#define KERNEL_BASE         0x8000
#define ARMBASE             0x408000


/* With sm/lg pages, can be more fine with how memory is laid out. Expect that
 * syscall and swi stacks won't be bigger than 64KB, and make the system stack exist in
 * high memory and grow towards the heap. */
#define SWI_STACK_ADDR_FINE 0x120000
#define INT_STACK_ADDR_FINE 0x130000
#define SYS_STACK_ADDR_FINE 0x400000
#define USR_SPACE_START     0x400000
#define SYS_HEAP_START      0x140000

/* Initial locations to host interrupt handler stacks without sm/lg pages.
 * Each mapped section is currently 1MB (100 000 hex addresses) big, since
 * the VM lets us map in 1MB chunks. Stacks grow downward! */
#if RUN_ADVANCED == 0
#define SYS_STACK_ADDR      0x100000 
#define SWI_STACK_ADDR      0x200000
#define INT_STACK_ADDR      0x300000
#define MAX_ADDR            0x400000
// The heap is set to start after the kernel's special stacks
#define MAX_STACK_ADDR      INT_STACK_ADDR
#define INITIAL_STACK_PTR   0x8000
#else
#define SYS_STACK_ADDR      SYS_STACK_ADDR_FINE 
#define SWI_STACK_ADDR      SWI_STACK_ADDR_FINE
#define INT_STACK_ADDR      INT_STACK_ADDR_FINE
#define MAX_ADDR            USR_SPACE_START
// The heap is set to start after the kernel's special stacks
#define MAX_STACK_ADDR      SYS_STACK_ADDR_FINE
#define INITIAL_STACK_PTR   SYS_STACK_ADDR_FINE
#endif
