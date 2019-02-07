/*
 * gpio.c: My own gpio.c
 * Garrick Fernandez (garrick)
 * 
 * Interface authors: Pat Hanrahan, Philip Levis, & Julie Zelenski
 * ---
 * TODO: flesh out, make your own!
 */

#include "gpio.h"
#include "rpi.h"

// Magic address: See pg. 6 (bus address 0x7Ennnnnn available at 0x20nnnnnn)
// and pg. 90 (the function select registers begin at 0x7E20nnnn)
#define GPIO_BASE 0x20200000
// More for SET, CLR, and LEV (level), pg. 90
#define GPIO_SET_BASE 0x2020001C
#define GPIO_CLR_BASE 0x20200028
#define GPIO_LEV_BASE 0x20200034
// More for PUD and PUDCLK, pg. 91
#define GPIO_PUD 0x20200094
#define GPIO_PUDCLK_BASE 0x20200098

/*
 * gpio_pin_to_fsel_reg
 * ---
 * Name-translation helper, given GPIO pin, returns function select register 
 * associated with the pin. See pg. 90 of the Broadcom manual; the FSEL registers 
 * start at 0x7E20 0000 and continue on by 0x4's. Each register supports 10 pins (pg. 92).
 * So, if we want FSEL9 (GPIO 9), we look at the 9/10 = FSEL0 register.
 */
static unsigned gpio_pin_to_fsel_reg(unsigned pin) {
  return (GPIO_BASE + 0x04 * (pin / 10));
}

/*
 * gpio_pin_to_fsel_offset
 * ---
 * Name-translation helper, given GPIO pin, returns function select register 
 * ~offset~ associated with the pin. See pg. 92 of the Broadcom manual; three bits are devoted 
 * to a single GPIO pin, so we multiply the raw offset (mod) by 3. So, if we want FSEL9 (GPIO 9),
 * we take bits 27-29 of the FSEL0 register.
 */
static unsigned gpio_pin_to_fsel_offset(unsigned pin) {
  return pin % 10 * 3;
}

/*
 * gpio_is_valid_pin
 * ---
 * Validation helper, given GPIO pin, returns if it is a valid pin.
 * See pg. 89 of the manual: "There are 54 GPIO pins...." We use the values
 * enumerated in gpio.h to do this check.
 */
static unsigned gpio_is_valid_pin(unsigned pin) {
  return (pin >= GPIO_PIN_FIRST && pin <= GPIO_PIN_LAST);
}

/*
 * gpio_is_valid_function
 * ---
 * Validation helper. Given GPIO pin, returns if it is a valid pin.
 * See pg. 92 of the manual for the types of functionality we can select.
 * We use the values enumerated in gpio.h to do this check. Notice that
 * GPIO_FUNC_ALT3 is the max value; accoding to pg. 92, the binary 0b111 is 
 * written for alternate function 3, so the enumerated value makes that 
 * easier for us!
 */
static unsigned gpio_is_valid_function(unsigned fn) {
  return (fn >= GPIO_FUNC_INPUT && fn <= GPIO_FUNC_ALT3);
}

/*
 * gpio_init
 * ---
 * Interface function, initializes GPIO (for this assignment, according to
 * the header file provided in libpi.small, this does nothing).
 */
void gpio_init() {
  return;
}

/*
 * gpio_set_function
 * ---
 * Interface function, given pin and function (see .h for enumerated types),
 * sets pin to be that function via lookup and writing with rpi.h's PUT32.
 * See pg. 92 for the fields of a FSEL register and the bit patterns we write to them.
 * If any input is invalid, this function does nothing.
 */
void gpio_set_function(unsigned int pin, unsigned int function) {
  if (!gpio_is_valid_pin(pin)) return;
  if (!gpio_is_valid_function(function)) return;

  unsigned fsel_reg = gpio_pin_to_fsel_reg(pin);
  unsigned offset = gpio_pin_to_fsel_offset(pin);
  unsigned value = GET32(fsel_reg);

  // Keep every bit except the three bits we wish to alter.
  unsigned mask = ~(0b111 << offset);
  value &= mask;
  value |= (function << offset);
  PUT32(fsel_reg, value);
}

/*
 * gpio_set_function
 * ---
 * Interface function, given pin and function, gets function associated
 * with pin. See pg. 92 for the fields of a FSEL register and the bit 
 * patterns we write to them.
 * If pin is invalid, returns GPIO_INVALID_REQUEST.
 */
unsigned int gpio_get_function(unsigned int pin) {
  if (!gpio_is_valid_pin(pin)) return GPIO_INVALID_REQUEST;

  unsigned fsel_reg = gpio_pin_to_fsel_reg(pin);
  unsigned offset = gpio_pin_to_fsel_offset(pin);
  unsigned value = GET32(fsel_reg);

  // Keep every bit except the three bits we wish to look at.
  unsigned mask = (0b111 << offset);
  value &= mask;
  return value >> offset;
}

// Convenience functions for setting a pin to GPIO_FUNC_INPUT or
// GPIO_FUNC_OUTPUT. The implementation calls gpio_set_function.
// See .h file for more details.
void gpio_set_input(unsigned int pin) { gpio_set_function(pin, GPIO_FUNC_INPUT); }
void gpio_set_output(unsigned int pin) { gpio_set_function(pin, GPIO_FUNC_OUTPUT); }

/*
 * gpio_pin_to_param_reg
 * ---
 * Name-translation helper, given GPIO pin, returns parameter register 
 * associated with the pin. See pg. 95-96 of the Broadcom manual.
 * The first register supports 32 pins, and the second supports the remainder.
 * So, if we want GPIO 9, we look at the 9/32 = 0th register.
 */
static unsigned gpio_pin_to_param_reg(unsigned pin, unsigned base) {
  return (base + 0x04 * (pin / 32));
}

/*
 * gpio_write
 * ---
 * Assuming pin is already in output mode, set pin to be 1 (high) 
 * or 0 (low). Pg. 95 of the manual lays out how the set and clear 
 * registers are involved in this: you can just write the bit to them
 * (zeroes have no effect) because the set register is only for setting.
 * It's like a parameter register than one that actually stores status;
 * this design gets rid of the need for read-modify-write operations.
 *
 * If pin is invalid, does nothing.
 */
void gpio_write(unsigned int pin, unsigned int val) {
  if (!gpio_is_valid_pin(pin)) return;

  unsigned reg;
  if (val) reg = gpio_pin_to_param_reg(pin, GPIO_SET_BASE);
  else reg = gpio_pin_to_param_reg(pin, GPIO_CLR_BASE);

  PUT32(reg, (1 << pin % 32)); // mod 32 yields the offset
}

/*
 * gpio_write
 * ---
 * Get the current level for the pin by accessing the pin level registers.
 * See pg. 96 for more information; we bitwise-AND the information we're
 * interested in getting. Similar to the SET and CLR regs, LEV supports 32
 * 1-bit slots per register, up to the maximum number of pins. If pin is invalid, 
 * returns GPIO_INVALID_REQUEST.
 */
unsigned int gpio_read(unsigned int pin) {
  if (!gpio_is_valid_pin(pin)) return GPIO_INVALID_REQUEST;

  unsigned reg = gpio_pin_to_param_reg(pin, GPIO_LEV_BASE);
  unsigned value = GET32(reg);
  if (value & (1 << pin % 32)) return 1;
  return 0;
}

// Some labs use set_on and set_off, so they're provided here. 
// Convenient for setting a pin on/off, call gpio_write under the hood.
// See .h file in cs140e-win19/libpi for more details.
int gpio_set_off(unsigned pin) { 
  gpio_write(pin, 0);
  return 0;
}

int gpio_set_on(unsigned pin) {
  gpio_write(pin, 1);
  return 0;
}

static unsigned gpio_is_valid_pud(unsigned pud) {
  return (pud >= GPIO_PUD_DISABLE && pud <= GPIO_PUD_PULLUP);
}

// Have this be the common routine, probably
void gpio_set_pud(unsigned pin, unsigned pud) {
  if (!gpio_is_valid_pin(pin)) return;
  if (!gpio_is_valid_pud(pud)) return;

  PUT32(GPIO_PUD, pud); // Set req. control signal
  delay_us(150); // Wait 150us; conservative, but sure
  unsigned pudclk_reg = gpio_pin_to_param_reg(pin, GPIO_PUDCLK_BASE);
  PUT32(pudclk_reg, (1 << pin % 32));
  delay_us(150); // Wait 150us
  PUT32(GPIO_PUD, GPIO_PUD_DISABLE); // Remove control signal
  PUT32(pudclk_reg, 0); // Remove clock
}

void gpio_set_pullup(unsigned pin) {
  gpio_set_pud(pin, GPIO_PUD_PULLUP);
}

void gpio_set_pulldown(unsigned pin) {
  gpio_set_pud(pin, GPIO_PUD_PULLDOWN);
}