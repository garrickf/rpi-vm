/*
 * sonar, hc-sr04
 *
 * the comments on the sparkfun product page might be helpful.
 *
 * Pay attention to the voltages on:
 *	- Vcc
 *	- Vout.
 * 
 * When circuit is "open" it can be noisy --- is it high or low when open?
 * If high need to configure a pin to be a pullup, if low, pulldown to reject
 * noise.
 */
#include "rpi.h"

// Set constants for pins
const unsigned trigger = 26, // Green
	echo = 19, // Blue
	led = 20;

void notmain(void) {
    uart_init();

	printk("starting sonar!\n");

	// put your code here.  
	//
	// you'll need to:
	// 	1. init the device (pay attention to time delays here)
	//
	//	2. do a send (again, pay attention to any needed time 
	// 	delays)
	//
	//	3. measure how long it takes and compute round trip
	//	by converting that time to distance using the datasheet
	// 	formula
	//
	// 	4. use the code in gpioextra.h and then replace it with your
	//	own (using the broadcom pdf in the docs/ directory).
	// 
	// troubleshooting:
	//	1. readings can be noisy --- you may need to require multiple
	//	high (or low) readings before you decide to trust the 
	// 	signal.
	//
	// 	2. there are conflicting accounts of what value voltage you
	//	need for Vcc.
	//	
	// 	3. the initial 3 page data sheet you'll find sucks; look for
	// 	a longer one. 
	gpio_set_function(trigger, GPIO_FUNC_OUTPUT);
	gpio_set_function(led, GPIO_FUNC_OUTPUT);
	gpio_set_function(echo, GPIO_FUNC_INPUT);
	gpio_set_pulldown(echo); // Keep it down

	while (1) {
		printk("sending signal!\n");
		gpio_set_on(trigger);
		delay_us(10);
		gpio_set_off(trigger);

		while (1) { // Wait for first signal loop
			if (gpio_read(echo)) break;
		}
		// printk("found echo!\n");

		unsigned st = timer_get_time();
		unsigned end;
		while (1) { // Measure loop
			end = timer_get_time();
			if (!gpio_read(echo)) {
				printk("dist: %d thou.\n", (end - st) * 1000 / 148);
				break;
			}
			if ((end - st) > 38 * 1000) {
				printk("timeout!\n");
				break;
			}
		}
		delay_ms(1000);
	}

	printk("stopping sonar !\n");
	clean_reboot();
}
