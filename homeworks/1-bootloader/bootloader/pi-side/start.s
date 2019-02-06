@ based on dwelch's start for bootloader.

.globl _start
_start:
    b skip
.space 0x200000-0x8004,0
skip:
    mov sp,#0x08000000
    bl notmain
hang: b rpi_reboot
# Note: ^^ had to change to rpi_reboot if using libpi base

.globl BRANCHTO
BRANCHTO:
    bx r0
