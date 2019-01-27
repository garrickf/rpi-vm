#include "rpi.h"
#include "uart.h"
#include "gpio.h"
#include "mem-barrier.h"

// NOTE: use get32 and put32 from rpi.h

// Defines for the start of the AUX and UART registers
#define AUX_BASE 0x20210000
#define AUX_ENABLES_OFST 0x5004
volatile unsigned *aux_enables = (volatile unsigned *)(AUX_BASE + AUX_ENABLES_OFST); // TODO: void *, static (so doesnt escape the namespace (nm))

// 2.1, p8
struct aux_periphs {
        volatile unsigned
                /* <aux_mu_> regs */
                io,             // p11
                ier,

#               define CLEAR_TX_FIFO    (1 << 1)
#               define CLEAR_RX_FIFO    (1 << 2)
#               define CLEAR_FIFOs      (CLEAR_TX_FIFO|CLEAR_RX_FIFO)
                // dwelch does not write the low bit?
#               define IIR_RESET        ((0b11 << 6) | 1)
                iir,

                lcr,
                mcr,
                lsr,
                msr,
                scratch,

#               define RX_ENABLE (1 << 0)
#               define TX_ENABLE (1 << 1)
                cntl,

                stat,
                baud;
};

static struct aux_periphs * const uart = (void*)0x20215040;
// Usage: uart->{{ field name }}

// use this if you need memory barriers.
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
 * aux_enable
 * ---
 * Sets the mini UART enable bit, bit 0 of the AUXENB register (see pg. 9).
 */
void aux_enable() { // TODO: get the bits first
  put32(aux_enables, 0b1);
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
#define BAUDRATE 270
void set_baudrate() {
  unsigned baudrate = BAUDRATE; // Better to do raw calculation
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
  aux_enable();
  dev_barrier();

  // Disable TX/RX while we work
  clear_txrx();

  // Do some initialization work on the UART registers
  set_baudrate();
  set_bitmode();
  clear_fifo_queues();
  // TODO: anything else?  

  // Reenable TX/RX
  set_txrx();

  dev_barrier();
}

// Check stat register
#define TX_EMPTY 5
#define RX_READY 0
int rxReady() {
  unsigned reg = get32(&uart->lsr);
  return (reg & (0b1 << RX_READY)) != 0;
}

int txEmpty() {
  unsigned reg = get32(&uart->lsr);
  return (reg & (0b1 << TX_EMPTY)) != 0;
}

// Busy wait for receive FIFO to hold 1 symbol (for us, an 8 bit string).
int uart_getc(void) {
	while (!rxReady()) {} 
  unsigned reg = get32(&uart->io);
  return reg & 0xff; // Get 8 LS bits
}

void uart_putc(unsigned c) {
  while (!txEmpty()) {}
  unsigned value = c & 0xff; // Take 8 LS bits
  put32(&uart->io, value); // TODO: take out middle man
}
