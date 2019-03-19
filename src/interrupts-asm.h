#ifndef __INTERRUPT_ASM_H__
#define __INTERRUPT_ASM_H__

/* from A2-2 */
#define USER_MODE       0b10000
#define FIQ_MODE        0b10001
#define IRQ_MODE        0b10010
#define SUPER_MODE      0b10011
#define ABORT_MODE      0b10111
#define UNDEF_MODE      0b11011
#define SYS_MODE        0b11111

/* Functions defined in asm */
unsigned cpsr_read(void);

/* Helper defined in driver.c */
void cpsr_print_mode(unsigned cpsr_r);

#endif
