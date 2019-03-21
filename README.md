# rpi-vm

## How to run
To complie and run, type `make` in the `src` directory. The Makefile assumes you have your bootloader linked in your `.bashrc` config file; if not, Dawson's 104E lab code should set you on the right track.

## File summary
Here's the structure of the directories:
- `docs` contains useful documentation, of which the most important is the ARM reference manual
- `firmware` has files you'll need to place on your SD card in order to get set up with the bootloader
- `homeworks` links to my version of the CS140E bootloader and kernel; if you have your own, great! If not, refer 
to Dawson's 140E repo to get a solution up and running to install binaries onto the rpi.
- `libpi-mine` is a slightly-modified `libpi` for the sake of the project.
- `src` contains the source code

Some files of interest:
- `driver.c` is the main point of entry for the program. Most of the VM tests are run out of here.
- `interrupts-asm.S` has important assembly code for routing interrupts (such as data aborts) to the right handler, which we implement in C code elsewhere. Why? See `A2-16` for a more thorough explanation: the interrupt handler table has enough space for 4 bytes of handler per exception, so we put `ldr` instructions that move the program counter, `pc`,
to a small trampoline of assembly that saves state, then initiates a more robust handler.
- `mmu.c` and `mmu.h` defines the structure of page tables and how they are manipulated in memory. The ARM hardware 
traverses the page tables we build out for it, so it's important to adhere to the structure specified in the ARM manual.
- `cpsr-util` defines a small set of assembly functions operating on the CPSR we're using in our tests.

## Changing tests and flags
To change tests, check out the `#define` macros in the `driver.c` file. You can pick and choose which tests to run for VM.

`#define DEBUG_PRINT_DESCRIPTORS 1`

## VM functionality

### Triggering data aborts

### Small pages

### Large pages

### Triggering a segmentation fault (dereferencing nullptr)
The ARM is a little tricky because the interrupts table is stored at address `0x0` on physical memory.
This means that we need to wrap a little more protection around it in order to prevent bad reads and writes
from a process that isn't our kernel. `VM_PART5` has a small test that attempts to dereference and read the
interrupts table. There are a few things going on in the test that are different from the others we've made:

First, we're mapping the interrupt table in a particular manner:
```c
mmu_map_sm_page(e->pt, 0x0, 0x0, e->domain, F_NO_USR_ACCESS);
```
The `F_NO_USR_ACCESS = 0b01` flag is passed along and alters the `AP` (access permission) bits in the small page
to disallow access when the processor is in user mode. `B4-8` and `B4-9` have more information on how the `AP` 
and additional `APX` bit control access.

Next, we're demoting our kernel from system mode to usr mode, emulating a move out from the kernel into userspace
code. Since we're in a privileged mode, we can still write to the mode bits of the _Current Program Status Register_ 
(CPSR) to demote ourselves (see `A2-11` to `A2-14` for more on the CPSR):

```c
cpsr_set_mode(USER_MODE); //       <--- This call writes to the CPSR
cpsr_print_mode(cpsr_read_c()); // <--- We ought to be in user mode
printk("> Dereferencing nullptr\n");
char nptr = *((char *)0x0);
printk("Accessing data... <%d>\n", nptr);
```

This change can't be reversed without initiating some state change back into privileged mode (say, through
a system call trap).

Once we dereference, we'll trigger a sub-page or page permission fault (see `B4-15` for the flow chart and `B4-20`
for the chart-chart), which spools up our data abort handler. In the handler, we can check to see if we tried 
dereferencing nullptr:

```c
unsigned address = get_fault_address_reg();
if (address == 0x0) { // null ptr exception
    printk("Illegal access of 0x0. Aborting...");
    clean_reboot();
}
```

To run this test, we're mapping memory a little differently than in the previous tests: mainly, we need to map the interrupt table with different permissions than the kernel code. To do this, you'll need to 
run around and flip a few flags. The first one is to `#define RUN_ADVANCED 1` in `memmap-constants.h` so that the code will map the kernal to the correct addresses. Also, make sure you `#define DEBUG_HANDLE_DATA_ABORTS 1` in `interrupts-c.c` to ensure you're hitting the check for `0x0`.

### Allocating extra stack space
