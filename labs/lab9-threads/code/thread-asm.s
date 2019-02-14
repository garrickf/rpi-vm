@ empty stub routines.  use these, or make your own.

.globl test_csave
test_csave:
	sub r0, r0, #68		@ Go down stack by 68 bytes, we will fill this space
	str r0, [r0]
	str r1, [r0, #4]	@ Add offset: PC-relative addressing
	str r2, [r0, #8]
	str r3, [r0, #12]
	str r4, [r0, #16]
	str r5, [r0, #20]
	str r6, [r0, #24]
	str r7, [r0, #28]
	str r8, [r0, #32]
	str r9, [r0, #36]
	str r10, [r0, #40]
	str r11, [r0, #44]
	str r12, [r0, #48]
	str r13, [r0, #52]
	str r14, [r0, #56]
	str r15, [r0, #60]
	mrs r1, cpsr 		@ Load in CPSR to a gen-purpose register
	str r1, [r0, #64]
	bx lr         		@ Return

.globl test_csave_stmfd
test_csave_stmfd:
	sub r0, r0, #68
	stm r0, {r0-r15}  	@ Store multiple
	mrs r1, cpsr 		@ Load in CPSR to a gen-purpose register
	str r1, [r0, #64]
	bx lr         		@ Return

.globl rpi_get_cpsr
rpi_get_cpsr:
	mrs r0, cpsr
	bx lr

.globl rpi_cswitch
rpi_cswitch:
	@ For brain-surgery: layout of stack-stored registers is:
	@
	@	                decreasing address -->
	@	sp before stm                 sp after
	@ 	+----------+--------+----+----+------+
	@ 	| r14 (lr) | r12-r2 | r1 | r0 | cspr |
	@ 	+----------+--------+----+----+------+
	@	60        56       12    8    4      0
	@
	@ To do brain-surgery, go to the start of the stack, move down
	@ the amount of space you know will be taken up (sp after), 
	@ then use offsets to insert things into the "slots".
	stmfd sp!, {r0-r12, r14}	@ push onto a full-descending stack
	mrs r2, cpsr 				@ load cspr into a general-purpose register
	sub sp, sp, #4				@ decrement stack ptr
	str r2, [sp]				@ store cpsr

	str sp, [r0]				@ store where to pick up into the address pointed to by r0
	ldr sp, [r1]				@ load in the context we're switching to
	ldr r2, [sp]				@ this is the cpsr (think about going back up the stack)
	msr cpsr, r2 				@ set the cpsr
	add sp, sp, #4				@ increment stack ptr
	ldmfd sp!, {r0-r12, r14} 	@ load multiple from full-descending stack (it knows to inc. sp!)
	bx lr;						@ branch to link register

	; sub sp, sp, #60			@ Sub first
	; str r0, [sp, #0]
	; str r1, [sp, #4]		@ Add offset: PC-relative addressing
	; str r2, [sp, #8]
	; str r3, [sp, #12]
	; str r4, [sp, #16]
	; str r5, [sp, #20]
	; str r6, [sp, #24]
	; str r7, [sp, #28]
	; str r8, [sp, #32]
	; str r9, [sp, #36]
	; str r10, [sp, #40]
	; str r11, [sp, #44]
	; str r12, [sp, #48]
	; str r14, [sp, #52]
	
	; mrs r2, cpsr 			@ Load in CPSR to a gen-purpose register
	; str r2, [sp, #56]

	; str sp, [r0]			@ store lower bound, so when we load, we go back up

	; ldr sp, [r1]
	; ldr r2, [sp, #56]		@ This is the CPSR
	; msr cpsr, r2

	; # ldmdb r1, {r0-r12, r14} @ Load multiple
	; ldr r0, [sp, #0]
	; ldr r1, [sp, #4]		@ Add offset: PC-relative addressing
	; ldr r2, [sp, #8]
	; ldr r3, [sp, #12]
	; ldr r4, [sp, #16]
	; ldr r5, [sp, #20]
	; ldr r6, [sp, #24]
	; ldr r7, [sp, #28]
	; ldr r8, [sp, #32]
	; ldr r9, [sp, #36]
	; ldr r10, [sp, #40]
	; ldr r11, [sp, #44]
	; ldr r12, [sp, #48]
	; ldr r14, [sp, #52]
	; add sp, sp, #60
	; bx lr

@ [Make sure you can answer: why do we need to do this?]
@
@ use this to setup each thread for the first time.
@ setup the stack so that when cswitch runs it will:
@	- load address of <rpi_init_trampoline> into LR
@	- <code> into r1, 
@	- <arg> into r0
@ 
.globl rpi_init_trampoline
rpi_init_trampoline:
    blx r1
    bl rpi_exit @ can call c functions
