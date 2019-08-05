# rpi-vm
`rpi-vm` builds out functionality from the virtual memory labs this quarter. From the projects doc:

```
Develop the virtual memory system into one that is full featured, able to handle the different page sizes, the different protection bits, etc. Have it correctly catch when code references beyond the end of the heap, dereferences null, and needs to dynamically grow the stack.
```

The features added were:
- Second-level descriptors and building pages tables for second-level fetches and walks
- Data abort exception handling code for when non-mapped VM addresses are accessed
- Debugging print utilities to inspect the state of the page table and fault registers on data abort

### Resources
This project benefitted a lot from the ARMv6 technical reference manual (see `docs/`), and some sample code from `dwelch`'s [implementation of VM](https://github.com/dwelch67/raspberrypi/tree/master/mmu) on the `rpi`. That implementation, however, lacks higher-level structs and large page creation, which we'll be adding here.

## How to run
To compile and run, type `make` in the `src` directory. The Makefile assumes you have your bootloader (`my-install`) linked in your `.bashrc` config file; if not, Dawson's 104E lab code should set you on the right track.

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

There are some flags set in some of the files that are useful to toggle:
- `#define DEBUG_PRINT_DESCRIPTORS 1` in `mmu.c` will make tests print out information about page table entries when they are made
- `#define DEBUG_HANDLE_DATA_ABORTS 1` in `interrupts-c.c` will enable most of the code on the data abort handler for fault detection.
- `#define DEBUG_PRINT_DATA_ABORTS 1` in `interrupts-c.c` will make tests print information on a data abort.
- `#define RUN_ADVANCED 1` in `memmap-constants.h` rearranges the way kernel code is laid out in physical memory, which is needed to run tests `VM_PART5` and `VM_PART6`.

## Intro to VM
First, a primer!

VM on the `rpi` consists of building and maintaining an in-memory structure of page entries that the ARM hardware walks in order to find the corresponding physical address. As you can imagine, this can be pretty slow, so there's an additional specialized hardware cache called the _translation lookaside buffer_ (TLB) that stores page table entries that were dredged up by previous page table walks.

In the page table itself, there are two main layers of descriptors, called first-level descriptors and second-level descriptors in the manual. You may have also heard of page directory entries (PDE's) and page table entries (PTE's), and we use the terms somewhat interchangably throughout. First-level descriptors can consist of 1MB sections or coarse page tables which further subdivide a given 1MB section. Second level descriptors are followed by hopping from a coarse page table descriptor, to a coarse page table, to a page table entry, and the flavors we're working with are small (4KB) pages and large (64KB) pages. The hardware chops up a modified virtual address (MVA) into different keys/indexes it uses to traverse the page tables. If we understand how this hardware walk works, we can emulate it to modify the table!

In addition, sections and pages have extra bits in their descriptors that encode special information about the virtual address mapping: who can access that memory, if it can be executed, etc. We can encode information into these bits, and the hardware will automatically check them as it does a translation—say we hit a PTE whose `domain` bits are different than the working set of domains we have access to—then the hardware will issue a "domain fault" and jump to a data abort handler. (There's also a prefetch abort handler for instruction fetches, but we'll be working with data accesses for now.) A data abort handler allows us to react meaningfully to all sort of page table faults: for example, if we've tried to access memory outside of our designated zone, we should do the `rpi` equivalent of a segfault!

The trickiest part of VM is making sure you're adhering to the structure laid out by the hardware, and mapping the regions your code (and memory!) will be in once you turn on VM, or you might hop to segments that don't have mappings. What's worse, if you forget to map your interrupt stack, you won't even be able to trigger your debugging data abort handler!

## Design
First and second-level descriptors are 32-bit entries, but instead of doing bit manipulation, we define structs that have bitfields we can set on the fly. Here's one for the generic second-level descriptor:

```
typedef struct second_level_descriptor {
    unsigned
        tag0:   1,  // The first bit is 1 in large pages and XN in small pages
        tag1:   1,  // The second bit is 0 in large pages and 1 in small pages (use this to distinguish)
        _data:   30;
} sld_t;
```

If we have a `sld_t *` to a blob of memory we know is a large page, we can cast it to a `lg_page_desc_t *` and get the more descriptive fields we're looking to use. The code juggles specific types locally and returns generalized types where possible; the additional type-checking is useful when doing layers of fetches.

The `mmu.c` file is subdivided into functions managing the creation of raw descriptors; public interface functions which map a VA to a PA with a given page size, flags, and domain; translation functions that use the VA to derive keys and indexes for lookup; and debug code.

### Small pages, large pages
The suite of tests `VM_PART1` to `VM_PART4` test basic turn MMU on/off, section allocation, small page allocation, and large page allocation. Unfortunately, I broke tests 2-4, but most of the small/large/section allocation can be seen in the other two tests, where I use them to more rigorously map the kernel.

Some tricky things to watch out for: for small pages, the `XN` bit is shoved into the 0th bit (see `B4-31`), where we'd expect the tag to be. The code maneuvers around that by fragmenting the tag field and intorducing constants that would be better for checking that field.

### Triggering data aborts
The data abort handler can be seen in `interrupts-c.c` and `interrupts-asm.S`; you can alter which fault occurs by uncommenting code in `VM_PART6`. In the data abort handler, we want to do a few things:
- Get useful information about the abort.
- Trigger a page miss if we should grow VM.
- Otherwise, panic (we've dereferenced `NULL` or are way out in no-man's land).

To get information, we can read the _data fault status register_ (DFSR) and _fault address register_ (FAR) to get infomation on the type of abort, domain, and even address that was being accessed at the time of the abort. See `B4-19` for more info and `B4-20` for a nice table that made it into code. To see if something is outside the heap, I modified `kmalloc` a bit (since we are using it further out from where the linker put `__heap_start__`) to return the last set starting point of the heap, so we can check heap bounds pretty easily. (As we introduce user processes, you can wrap heap start and end into little process control blocks!) 

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
To allocate more stack, we use a naive check: if we're below where we mapped the stack and above the current heap end, we should allocate a page for it:

```c
if (address < SYS_STACK_ADDR_FINE && address > (unsigned)kmalloc_heap_end()) {
    // Within system stack bounds and above heap, initiate a page miss
    // Could make more fine grained
    handle_page_miss(address);
}
```

The page miss handler is located in `driver.c` to have access to the current page table entry; it just maps a large page. Later, we could make this a global or common structure shared between code:

```c
void handle_page_miss(unsigned address) {
    // Do a one-to-one mapping
    assert(curr_env);
    printk("Allocating a new page for the stack...\n");
    mmu_map_lg_page(curr_env->pt, address - ADDRESSES_PER_4KB, 
        address - ADDRESSES_PER_4KB, curr_env->domain, 0);
    printk("All done!\n");
}
```

Something else that could be added is moving the instruction pointer back after the data abort to reexecute the instruction which caused the page miss: according to page `A2-21`,we can go back by #8 (to re-execute after fixing the reason for the abort) or by #4 (if the aborted instruction does not need to be re-executed). Right now, we always `sub lr, lr, #4`, which is a little failure-oblivious :)

## Tricky bits and next steps
Some of the trickier bugs in the assignment were early on, when debugging bad page table walks and setting bitfields properly to trigger the data aborts you were expecting. This rigorous testing gave me more trust in the structure of the page table, but it was also frustrating when things didn't work, with no clear indication of what was wrong. 

Above all, implementing VM took a lot of time just thinking for me, and designing things like how I wanted to switch between structs after doing fetches, design the interface for encoding flags into descriptors, and deciding how to lay out kernel memory to leave open the possibility for userspace, loding binaries, and threads—these were interesting questions I wanted to answer well. You may notice in some of the interfaces references to how things work in some great UNIX functions (`waitpid` and `status` and `flags`, generalized `sock_addr` structs between `IPv4` and `IPv6`); the hope was that with a solid foundation it would be easier to scale and add in threading support in a non-hacky manner. If only there was more time!

In terms of cool new things to do, it would be neat to:
- Add user processes (could use domains to quickly turn on/off permissions to kernel data, or replace the page table pointer and flush TLB's in a more heavyweight fashion)
- Reincorporate syscalls via `swi` to promote ourselves back to kernel mode and transition between kernel and user code
- Incorporating threads and weaving them into VM
- Flipping kernel + bootloader memory mappings so that the kernel can boot into high address space, allowing us to load and run normal binaries at `ARMBASE`
- Managing page mapping when physical memory gets clogged up—perhaps another kernel data structure?
