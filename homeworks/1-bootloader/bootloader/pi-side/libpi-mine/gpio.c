#include "gpio.h"
#include "rpi.h"

#define GPFSEL0 0x20200000
#define GPFSEL1 0x20200004
#define GPFSEL2 0x20200008
#define GPFSEL3 0x2020000c
#define GPFSEL4 0x20200010
#define GPFSEL5 0x20200014

// Given a pin number, return the function select register to use
unsigned int gpio_pin_to_function_register(unsigned int pin) {
  unsigned int val = pin / 10;
  switch (val) {
  case 0:
    return GPFSEL0;
  case 1:
    return GPFSEL1;
  case 2:
    return GPFSEL2;
  case 3:
    return GPFSEL3;
  case 4:
    return GPFSEL4;
  case 5:
    return GPFSEL5;
  default:
    return 0;
  }
}

// Given a pin number, return the bit offset within its select
// register to use. E.g., if this returns 6 the pin uses bits 6-8.
unsigned int gpio_pin_to_function_offset(unsigned int pin) {
  unsigned int offset = (pin % 10) * 3;
  return offset;
}

unsigned int gpio_pin_valid(unsigned int pin) {
  return (pin >= GPIO_PIN_FIRST && pin <= GPIO_PIN_LAST);
}

unsigned int gpio_function_valid(unsigned int f) {
  return (f >= GPIO_FUNC_INPUT && f <= GPIO_FUNC_ALT5);
}

void gpio_set_function(unsigned int pin, unsigned int function) {
  if (!gpio_pin_valid(pin)) { return; }
  if (!gpio_function_valid(function)) { return; }

  unsigned int reg = gpio_pin_to_function_register(pin);
  unsigned int offset = gpio_pin_to_function_offset(pin);
  unsigned int val = GET32(reg);

  // Remove the original configuration bits
  unsigned int mask = ~(0x7 << offset);
  val = val & mask;

  // Add the new configuration bits
  val |= function << offset;

  // Set the register
  PUT32(reg, val);
}
