/*
 * interrupt-asm.s
 *
 * Code for interrupt handling.  Refer to armisa.pdf in docs/ for what
 * the opcodes mean.
 */

#include "arm-coprocessor-insts.h"
#include "interrupts-asm.h"

/*
 * Enable/disable interrupts.
 *
 * CPSR = current program status register
 *        upper bits are different carry flags.
 *        lower 8:
 *           7 6 5 4 3 2 1 0
 *          +-+-+-+---------+
 *          |I|F|T|   Mode  |
 *          +-+-+-+---------+
 *
 *  I : disables IRQ when = 1.
 *  F : disables FIQ when = 1.
 *  T : = 0 indicates ARM execution, = 1 is thumb execution.
 *      Mode = current mode.
 */
.globl system_enable_interrupts
system_enable_interrupts:
    mrs r0,cpsr                 @ move process status register (PSR) to r0
    bic r0,r0,#(1<<7)		@ clear 7th bit.
    msr cpsr_c,r0		@ move r0 back to PSR
    bx lr		        @ return.

.globl system_disable_interrupts
system_disable_interrupts:
    mrs r0,cpsr		       
    orr r0,r0,#(1<<7)	       @ set 7th bit: or in 0b100 0000
    msr cpsr_c,r0
    bx lr


@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@
@ put your code below.

@ Ref. from lab on threads (lab9)
.globl cpsr_read
cpsr_read:
	mrs r0, cpsr
	bx lr

.globl swi_asm1
swi_asm1:
    swi 0x1     @ start syscall, that's it
    bx lr

.globl swi_asm2
swi_asm2:
    swi 0x2     @ start syscall, that's it
    bx lr

.globl swi_asm3
swi_asm3:
    swi 0x3     @ start syscall, that's it
    bx lr

@ swi_setup_stack: set the stack pointer in SUPER mode to SWI_STACK_ADDR
@ r0: SWI_STACK_ADDR
.globl swi_setup_stack
swi_setup_stack:
    mrs r1, cpsr    @ save cpsr so we don't trash it later
    mov r2, #SUPER_MODE
    msr cpsr_c, r2  @ enter supervisor mode to get a different stack pointer

    mov sp, r0

    msr cpsr, r1    @ return cpsr to default
    bx lr

@ From timer-interrupt lab: a table for looking up the interrupt type (what to do)
@ Literally, if you jump to 0x8, you go to the third instruction (a software interrupt!)
.globl _interrupt_table
.globl _interrupt_table_end
_interrupt_table:
  ldr pc, _reset_asm
  ldr pc, _undefined_instruction_asm
  ldr pc, _software_interrupt_asm
  ldr pc, _prefetch_abort_asm
  ldr pc, _data_abort_asm
  ldr pc, _reset_asm
  ldr pc, _interrupt_asm
fast_interrupt_asm:
  sub   lr, lr, #4 @First instr of FIQ handler
  push  {lr}
  push  {r0-r12}
  mov   r0, lr              @ Pass old pc
  bl    fast_interrupt_vector    @ C function
  pop   {r0-r12}
  ldm   sp!, {pc}^

_reset_asm:                   .word reset_asm
_undefined_instruction_asm:   .word undefined_instruction_asm
_software_interrupt_asm:      .word software_interrupt_asm
_prefetch_abort_asm:          .word prefetch_abort_asm
_data_abort_asm:              .word data_abort_asm
_interrupt_asm:               .word interrupt_asm
_interrupt_table_end: 

@ only handler that should run since we only enable general interrupts
@ a handler for swi interrupts
software_interrupt_asm:  
  @ push {r0, lr}
  push  {r0-r12,lr}     @ XXX: pushing too many registers: only need caller
  @ vpush {s0-s15}	    @ uncomment if want to save caller-saved fp regs

  ldr r0, [lr, #-4]
  and r0, r0, #0xFF     @ and-mask the lower 8 bits
  mov r1, lr            @ move old program counter (lr) into r1
  mov r2, sp            @ move in stack pointer

  bl    handle_swi      @ c code

  @ vpop {s0-s15}           @ pop caller saved fp regs
  pop   {r0-r12,lr} 	    @ pop integer registers TODO: software_interrupt_asm for different nums of arguments
  @ pop {r0, lr}

  @ sub lr, lr, #4    @ Don't subtract, you want to go to the next instruction (lr points to that) WHY DID I DO THAT
  @ return from interrupt handler: will re-enable general ints.
  movs    pc, lr       @ moves the link register into the pc and implicitly
                        @ loads the PC with the result, then copies the 
                        @ SPSR to the CPSR.
                        @ We don't want to update the link reg, so we just literally shove the lr into pc

reset_asm:
  sub   lr, lr, #4
  mov   sp, #INT_STACK_ADDR  @ spec: Have all other interrupts load INT_STACK_ADDR as the stack pointer and call the appropriate handler in interrupts-c.c.
  bl    reset_vector
undefined_instruction_asm:
  sub   lr, lr, #4
  mov   sp, #INT_STACK_ADDR
  bl    undefined_instruction_vector
prefetch_abort_asm:
  sub   lr, lr, #4
  mov   sp, #INT_STACK_ADDR
  bl    prefetch_abort_vector
data_abort_asm:
  mov   sp, #INT_STACK_ADDR
  push  {r0, lr}             @ want to push all the caller saved regs and maybe (frame ptr), no need for s0-s4
  bl    data_abort_vector
  pop   {r0, lr}
  sub   lr, lr, #4
  movs  pc, lr              @ Continue on as if nothing happened: see: failure oblivious coding
interrupt_asm:
  sub   lr, lr, #4
  mov   sp, #INT_STACK_ADDR
  bl    interrupt_vector

.globl get_data_fault_status_reg
get_data_fault_status_reg:
  @ Read CP15 register 5 (c5) to r0, opcode2 = 0 for the data FSR (p. B4-43)
  mrc p15, 0, r0, c5, c0, 0 
  bx lr 

.globl get_fault_address_reg
get_fault_address_reg:
  @ Read CP15 register 5 (c6) to r0, opcode2 = 0 for the data FSR (p. B4-44)
  mrc p15, 0, r0, c6, c0, 0 
  bx lr 