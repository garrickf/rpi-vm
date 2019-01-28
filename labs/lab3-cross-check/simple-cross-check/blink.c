#include "rpi.h"

// see broadcomm documents for magic addresses.
#define GPIO_BASE 0x20200000UL
volatile unsigned *gpio_set0  = (volatile unsigned *)(GPIO_BASE + 0x1C);
volatile unsigned *gpio_clr0  = (volatile unsigned *)(GPIO_BASE + 0x28);

// XXX: Lab 3: rewritten using get and put32
// int functions should pass -1 if bogus is passed in
int gpio_set_output(unsigned pin) {
  if (pin >= 32) return -1;
	volatile unsigned *gpio_fsel;
	// Calculate correct function select to use. The function selects
	// operate on different pins, and those pins are themselves spread out
	// over 32-bit spaces. Setting a pin to be output consists of
	// setting the appropriate bits in these spaces. We can use a little math 
	// to figure out where in memory to go to set bits.
	gpio_fsel = (volatile unsigned *)(GPIO_BASE + 0x04 * (pin / 10));
	unsigned value = get32(gpio_fsel);
	
	// Keep every bit except the three bits we wish to alter.
	unsigned mask = ~(0b111 << (pin % 10 * 3));
	value &= mask;

	value |= (0b001 << (pin % 10 * 3));
	put32(gpio_fsel, value);
  return 0;
}

// Translates the GPIO pin number to be activated to a bit number, 
// which is put into the GPIO Pin Output Set Registers (GPSETn).
int gpio_set_on(unsigned pin) {
  if (pin >= 32) return -1;
	unsigned param = 0b1 << pin;
	put32(gpio_set0, param);
  return 0;
}

// Translates the GPIO pin number to be activated to a bit number, 
// which is put into the GPIO Pin Output Clear Registers (GPCLRn).
int gpio_set_off(unsigned pin) {
  if (pin >= 32) return -1;
	unsigned param = 0b1 << pin;
	put32(gpio_clr0, param);
  return 0;
}

// countdown 'ticks' cycles; the asm probably isn't necessary.
//void delay(unsigned ticks) {
//	while(ticks-- > 0)
//		asm("add r1, r1, #0");
//}

void notmain (void) {
  int led = 20;

  gpio_set_output(led);
  while(1) {
    gpio_set_on(led);
    delay(10000000);
    // delay(100); // Delay experiment
    gpio_set_off(led);
    delay(1000000);
    // delay(1000); // Delay experiment
  }
//  return 0;

// gpio_set_output(20);
// gpio_set_output(21);
 //        while(1) {
 //                gpio_set_on(20);
 //                gpio_set_off(21);
 //                delay(10000000);
 //                gpio_set_off(20);
 //                gpio_set_on(21);
 //                delay(1000000);
 //        }
	// return 0;
}

int gpio_broken_example(unsigned pin) {
  if(pin >= 32)
    return -1;
  volatile unsigned char *u = (void*)(0x80000000);
  u += pin*4;
  put32(u, get32(u)+1);
  return 0;
}
