/*
 * my-uart: Personal implementation of uart.c
 * Garrick Fernandez (garrick)
 * ---
 * Exports three important functions for UART 
 * (universal asynchronous receiver-transmitter) communication:
 * init, getc, and putc. The test programs in lab4-uart hook into
 * the library compiled with this source in order to transmit data
 * back to the UNIX side. In the bootloader, putc and getc are 
 * utilized by higher-level helpers to sends chunks of the communcations
 * protocol needed to boot the program.
 */

#include "rpi.h"
#include "uart.h"
#include "gpio.h"
#include "mem-barrier.h"

//#define get32 GET32
//#define put32 PUT32 
// NOTE: use get32 and put32 from rpi.h

// Defines for the start of the AUX and UART registers
#define AUX_BASE 0x20210000
#define AUX_ENABLES_OFST 0x5004
static volatile unsigned *auxenb = (volatile unsigned *)(AUX_BASE + AUX_ENABLES_OFST); // TODO: void *, static (so doesnt escape the namespace (nm))

/*
 * Auxillary peripherals structure, taken from Dawson's newsgroup post.
 * If you cast a memory address as a pointer to a struct, you can
 * navigate its fields in analogous fashion to the registers on the rpi. 
 * Nifty! See pg. 8-9 for more details.
 */
struct aux_periphs {
  volatile unsigned
  /* <aux_mu_> regs */
  io, // see pg. 11
  ier,
  #define CLEAR_TX_FIFO (1 << 1)
  #define CLEAR_RX_FIFO (1 << 2)
  #define CLEAR_FIFOs (CLEAR_TX_FIFO|CLEAR_RX_FIFO)
  // dwelch does not write the low bit?
  # define IIR_RESET ((0b11 << 6) | 1)
  iir,

  lcr,
  mcr,
  lsr,
  msr,
  scratch,

  #define RX_ENABLE (1 << 0)
  #define TX_ENABLE (1 << 1)
  cntl,

  stat,
  baud;
};

// Usage: uart->{{ field name }}
static struct aux_periphs * const uart = (void*)0x20215040; // See pg. 8 for mem address

// Combine the read/write memory barriers to remove ambiguity in the code.
void dev_barrier(void) {
	dmb();
	dsb();
}

/*
 * init_gpio
 * ---
 * Sets up the GPIO pins for talking. To do so, uses functions in gpio.h
 * (gpio_set_function) to set the GPIO pins 14 and 15 to TXD1 and RXD1 (see pg. 102).
 */
void init_gpio() {
  gpio_set_function(14, GPIO_FUNC_ALT5);
  gpio_set_function(15, GPIO_FUNC_ALT5);
}

/*
 * clear_txrx
 * ---
 * Disables the receiver and transmitter by zeroing out bits 1-0 on the
 * AUX_MU_CNTL_REG register (see pg. 17). This lets us do housekeeping without any
 * spurious communication happening.
 */
void clear_txrx() {
  unsigned value = get32(&uart->cntl);
  value = value & ~(0b11); // Zero out two lower bits
  put32(&uart->cntl, value);
}

void set_txrx() {
  unsigned value = get32(&uart->cntl);
  value = value | 0b11; // Flip two lower bits
  put32(&uart->cntl, value);
}

// Sets the baudrate of the rpi to be 115200
#define BAUDRATE 270 // Closest we can get!
void set_baudrate() {
  unsigned baudrate = BAUDRATE; // Raw calculation: see pg. 11 (system clock: 250MHz)
  put32(&uart->baud, baudrate);
}

// Sets the bitmode to be 8 bits in the LCR register (see pg. 14)
void set_bitmode() {
  unsigned value = get32(&uart->lcr);
  value = value | 0b11;
  put32(&uart->lcr, value);
}

/*
 * clear_fifo_queues
 * ---
 * The receive and transmit FIFO's ought to be cleared in case they
 * have any garbage in them. To do this, we write 0b110 to bits 2:1 of
 * the IIR register (see pg. 13).
 */
void clear_fifo_queues() {
  unsigned reg = get32(&uart->iir);
  reg |= 0b110; // OR in 1's in bits 2:1
  put32(&uart->iir, reg);
}

void uart_init(void) {
  dev_barrier(); // Paranoid; in case called multiple times 
  init_gpio();
  dev_barrier(); // Swap to AUX peripheral
    
  // Write to the AUXENB register's lowest bit to enable the UART (pg. 9)
  put32(auxenb, get32(auxenb) | 1);
  
  dev_barrier();

  // Disable TX/RX while we work
  clear_txrx();

  // Do some initialization work on the UART registers
  set_baudrate();
  set_bitmode();
  clear_fifo_queues();

  // Reenable TX/RX
  set_txrx();

  dev_barrier();
}

// XXX: Check stat register
#define TX_EMPTY (1 << 5)
#define RX_READY 1
/*
 * uart_getc
 * ---
 * Uses the LSR register to check bit 0 (reciever ready) before
 * taking an 8-bit symbol from the IO register. Se pgs. 15 and 11
 * for the register info. Uses a busy loop.
 */
int uart_getc(void) {
	while (!(get32(&uart->lsr) & RX_READY)) {} // While receiver empty (pg. 15)
  return get32(&uart->io) & 0xff;
}

/*
 * uart_putc
 * ---
 * Uses the LSR register to check bit 5 (transmitter empty) before
 * sending a single 8-bit symbol of data via the IO register. See pgs.
 * 15 and 11 for the register info.
 */
void uart_putc(unsigned c) {
  while (!(get32(&uart->lsr) & TX_EMPTY)) {} // While transmitter full (pg. 15)
  put32(&uart->io, c & 0xff);
}
