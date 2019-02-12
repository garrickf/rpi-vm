/*
 * gpio.c: My own gpio.c
 * Garrick Fernandez (garrick)
 * 
 * Original interface authors: Pat Hanrahan, Philip Levis, Julie Zelenski, Dawson Engler
 * ---
 * Implements the gpio.h interface as seen in cs140e-win19/libpi. There's a few
 * discrepancies between that shared interface and the one specified in libpi.small in 
 * bootloader, so as much as possible, I tried to create something compatible between 
 * the two. Citations: lab1 (for initial GPIO stuff), Broadcom ARM Manual, cs107e-complex-gpio
 * in lab3, whose gpio.h file (which is identical to the one in bootloader/pi-side/libpt.small)
 * inspired some of the enumerated values I'm using for convenience.
 *
 * The major difference of the shared interface and libpi.small/cs107e is that many of the functions
 * return some error if something goes wrong, instead of doing nothing. This can be useful
 * for callers that want to see if something's up.
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

// Cited from gpio.h in libpi.small; enumerated types for our convenience.
// gpio.h in cs140e-win19/libpi has more defined for us as well!
enum {
    GPIO_PIN_FIRST = 0,
    GPIO_PIN0 = 0,
    GPIO_PIN1 = 1,
    GPIO_PIN2 = 2,
    GPIO_PIN3 = 3,
    GPIO_PIN4 = 4,
    GPIO_PIN5 = 5,
    GPIO_PIN6 = 6,
    GPIO_PIN7 = 7,
    GPIO_PIN8 = 8,
    GPIO_PIN9 = 9,
    GPIO_PIN10 = 10,
    GPIO_PIN11 = 11,
    GPIO_PIN12 = 12,
    GPIO_PIN13 = 13,
    GPIO_PIN14 = 14,
    GPIO_PIN15 = 15,
    GPIO_PIN16 = 16,
    GPIO_PIN17 = 17,
    GPIO_PIN18 = 18,
    GPIO_PIN19 = 19,
    GPIO_PIN20 = 20,
    GPIO_PIN21 = 21,
    GPIO_PIN22 = 22,
    GPIO_PIN23 = 23,
    GPIO_PIN24 = 24,
    GPIO_PIN25 = 25,
    GPIO_PIN26 = 26,
    GPIO_PIN27 = 27,
    GPIO_PIN28 = 28,
    GPIO_PIN29 = 29,
    GPIO_PIN30 = 30,
    GPIO_PIN31 = 31,
    GPIO_PIN32 = 32,
    GPIO_PIN33 = 33,
    GPIO_PIN34 = 34,
    GPIO_PIN35 = 35,
    GPIO_PIN36 = 36,
    GPIO_PIN37 = 37,
    GPIO_PIN38 = 38,
    GPIO_PIN39 = 39,
    GPIO_PIN40 = 40,
    GPIO_PIN41 = 41,
    GPIO_PIN42 = 42,
    GPIO_PIN43 = 43,
    GPIO_PIN44 = 44,
    GPIO_PIN45 = 45,
    GPIO_PIN46 = 46,
    GPIO_PIN47 = 47,
    GPIO_PIN48 = 48,
    GPIO_PIN49 = 49,
    GPIO_PIN50 = 50,
    GPIO_PIN51 = 51,
    GPIO_PIN52 = 52,
    GPIO_PIN53 = 53,
    GPIO_PIN_LAST =  53
};

// Cited from gpio.h in libpi.small; return value for invalid request
#define GPIO_INVALID_REQUEST -1

// Note: gpio_init from libpi.small is omitted here, as it's not part 
// of the shared libpi's gpio.h; it does nothing, anyway.

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
 * enumerated above to do this check.
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
 * GPIO_FUNC_ALT3 is the max value; according to pg. 92, the binary 0b111 is 
 * written for alternate function 3, so the enumerated value makes that 
 * easier for us!
 */
static unsigned gpio_is_valid_function(unsigned fn) {
  return (fn >= GPIO_FUNC_INPUT && fn <= GPIO_FUNC_ALT3);
}

/*
 * gpio_set_function
 * ---
 * Interface function, given pin and function (see .h for enumerated types),
 * sets pin to be that function via lookup and writing with rpi.h's PUT32.
 * See pg. 92 for the fields of a FSEL register and the bit patterns we write to them.
 * If any input is invalid, this function returns GPIO_INVALID_REQUEST.
 */
int gpio_set_function(unsigned int pin, unsigned int function) {
  if (!gpio_is_valid_pin(pin)) return GPIO_INVALID_REQUEST;
  if (!gpio_is_valid_function(function)) return GPIO_INVALID_REQUEST;

  unsigned fsel_reg = gpio_pin_to_fsel_reg(pin);
  unsigned offset = gpio_pin_to_fsel_offset(pin);
  unsigned value = GET32(fsel_reg);

  // Keep every bit except the three bits we wish to alter.
  unsigned mask = ~(0b111 << offset);
  value &= mask;
  value |= (function << offset);
  PUT32(fsel_reg, value);
  return 0;
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
int gpio_set_input(unsigned int pin) { return gpio_set_function(pin, GPIO_FUNC_INPUT); }
int gpio_set_output(unsigned int pin) { return gpio_set_function(pin, GPIO_FUNC_OUTPUT); }

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
 * If pin is invalid, returns GPIO_INVALID_REQUEST.
 */
int gpio_write(unsigned int pin, unsigned int val) {
  if (!gpio_is_valid_pin(pin)) return GPIO_INVALID_REQUEST;

  unsigned reg;
  if (val) reg = gpio_pin_to_param_reg(pin, GPIO_SET_BASE);
  else reg = gpio_pin_to_param_reg(pin, GPIO_CLR_BASE);

  PUT32(reg, (1 << pin % 32)); // mod 32 yields the offset
  return 0;
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
// Convenient for setting a pin on/off, calls gpio_write under the hood.
// See .h file in cs140e-win19/libpi for more details.
int gpio_set_off(unsigned pin) { return gpio_write(pin, 0); }
int gpio_set_on(unsigned pin) { return gpio_write(pin, 1); }

static unsigned gpio_is_valid_pud(unsigned pud) {
  return (pud >= GPIO_PUD_DISABLE && pud <= GPIO_PUD_PULLUP);
}

/*
 * gpio_set_pud
 * ---
 * Sets the pid to be the given pud (pullup, pulldown, or disable). Uses
 * two registers to handle this: one (GPPUD) chooses the function that will be set 
 * when writing to the other (GPPUDCLK). See pg. 100-101 for the documentation
 * of the routine. We delay by 150us, which should be an upper bound on 150 cycles,
 * the minimum time we need for the control signals to take effect.
 *
 * If pin or pud is invalid, returns GPIO_INVALID_REQUEST.
 */
int gpio_set_pud(unsigned pin, unsigned pud) {
  if (!gpio_is_valid_pin(pin)) return GPIO_INVALID_REQUEST;
  if (!gpio_is_valid_pud(pud)) return GPIO_INVALID_REQUEST;

  PUT32(GPIO_PUD, pud);                 // Set required control signal
  delay_us(150);                        // Wait 150us; conservative, but sure
  unsigned pudclk_reg = gpio_pin_to_param_reg(pin, GPIO_PUDCLK_BASE);
  PUT32(pudclk_reg, (1 << pin % 32));   // Write to PUDCLK to modify GPIO pin
  delay_us(150);                        // Wait 150us
  PUT32(GPIO_PUD, GPIO_PUD_DISABLE);    // Remove control signal
  PUT32(pudclk_reg, 0);                 // Remove clock
  return 0;
}

// Convenience functions for setting a pin to GPIO_PUD_PULLUP or
// GPIO_PUD_PULLDOWN. The implementation calls gpio_set_pud.
// See .h file for more details.
int gpio_set_pullup(unsigned pin) { return gpio_set_pud(pin, GPIO_PUD_PULLUP); }
int gpio_set_pulldown(unsigned pin) { return gpio_set_pud(pin, GPIO_PUD_PULLDOWN); }
