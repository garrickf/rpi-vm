/*
	Example of how to use interrupts to interleave two tasks: setting
	a LED smoothly to different percentage of brightness and periodically
	reading sonar distance.
	
	Without interrupts have to decide how to PWM led while you are 
	doing sonar.  Can easily get jagged results (flickering) if you
	don't do it often enough.  Manually putting in code to interleave
	is a pain.

	Current method to fix:
		1. non-interrupt code periodically measures distance
		and computes the off/on decisions in a vector 

		2. interrupt handler cycles through vector doing off/on based
		on it's values.

	You could do the other way: sonar measurements in the int handler,
	pwm in the main code.  Q: tradeoffs?
*/
#include "rpi.h"
#include "timer-interrupt.h"
#include "pwm.h"

// use whatever pins you want.
const unsigned trigger = 26, // Green
	echo = 19, // Blue
	led = 20;

// useful timeout.
int timeout_read(int pin, int v, unsigned timeout) {
	unsigned start = timer_get_time();
	while(1) {
		if(gpio_read(pin) != v)
			return 1;
		if(timer_get_time() - start > timeout)
			return 0;
	}
}

#define K_NUM_ENTRIES 10
static unsigned choices[K_NUM_ENTRIES];
static unsigned choiceIdx = 0;

void int_handler(unsigned pc) {
	/*
	   Can have GPU interrupts even though we just enabled timer: 
	   check for timer, ignore if not.

	   p 114:  bits set in reg 8,9
	   These bits indicates if there are bits set in the pending
	   1/2 registers. The pending 1/2 registers hold ALL interrupts
	   0..63 from the GPU side. Some of these 64 interrupts are
	   also connected to the basic pending register. Any bit set
	   in pending register 1/2 which is NOT connected to the basic
	   pending register causes bit 8 or 9 to set. Status bits 8 and
	   9 should be seen as "There are some interrupts pending which
	   you don't know about. They are in pending register 1 /2."
	 */
	volatile rpi_irq_controller_t *r = RPI_GetIRQController();
	if(r->IRQ_basic_pending & RPI_BASIC_ARM_TIMER_IRQ) {

		// do very little in the interrupt handler: just flip
	 	// led off or on.
	 	if(choices[choiceIdx % K_NUM_ENTRIES]) gpio_set_on(led);
	 	else gpio_set_off(led);
		choiceIdx++;

		/* 
		 * Clear the ARM Timer interrupt - it's the only interrupt 
		 * we have enabled, so we want don't have to work out which 
		 * interrupt source caused us to interrupt 
		 */
		RPI_GetArmTimer()->IRQClear = 1;
	}
}

// put N [off,on] decisions in pwm_choices.   error should be 
// minimal.
// 	- <pwm_choices> should be able to hold N entries.
 
// void pwm_compute(unsigned *pwm_choices, unsigned on, unsigned n);
// void pwm_print(unsigned *pwm_choices, unsigned on, unsigned n);

void notmain() {
    uart_init();
    install_int_handlers();

	timer_interrupt_init(0x4);

    gpio_set_output(led);
    gpio_set_output(trigger);
    gpio_set_input(echo);
    gpio_set_pullup(echo);


	// do last!
  	system_enable_interrupts();  	// Q: comment out: what happens?

	while (1) {
		// printk("sending signal!\n");
		gpio_set_on(trigger);
		delay_us(10);
		gpio_set_off(trigger);

		while (1) { // Wait for first signal loop
			if (gpio_read(echo)) break;
		}
		// printk("found echo!\n");

		unsigned st = timer_get_time();
		unsigned end;
		unsigned dist = 1;
		unsigned scaled = 1;
		while (1) { // Measure loop
			end = timer_get_time();
			if (!gpio_read(echo)) {
				// printk("dist: %d thou.\n", (end - st) * 1000 / 148);
				dist = (end - st) * 1000 / 148; // Thousandths of an inch
				scaled = dist / (1000);
				if (scaled > K_NUM_ENTRIES) scaled = K_NUM_ENTRIES;
				printk("scaled: %d\n", scaled);
				break;
			}
			if ((end - st) > 38 * 1000) {
				// printk("timeout!\n");
				break;
			}
		}
		pwm_compute(choices, K_NUM_ENTRIES - scaled, K_NUM_ENTRIES); // Recompute
		// pwm_print(choices, dist, K_NUM_ENTRIES); // Recompute
		delay_ms(100);
	}

	// printk("stopping sonar !\n");
}
