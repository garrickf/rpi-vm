#include "cpsr-util-asm.h"

.globl cpsr_read
cpsr_read:
	mrs r0, cpsr    @ Yank value of cpsr
	bx lr

.globl cpsr_set_mode
cpsr_set_mode:
	msr cpsr, r0    @ Write r0 (arg1) to the cpsr
	bx lr