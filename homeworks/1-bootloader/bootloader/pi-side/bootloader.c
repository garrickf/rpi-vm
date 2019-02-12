/* 
 * bootloader.c - Simple Bootloader
 * Garrick Fernandez (garrick)
 * ---
 * "A very simple bootloader; more robust than xmodem (that code seems to 
 * have bugs in terms of recovery with inopportune timeouts)." - Dawson
 * 
 * Leverages basic utility functions to maintain a communication ping-pong
 * protocol beween us and the UNIX side until all data has been transferred,
 * at which point we jump to the executable and run it.
 */

#define __SIMPLE_IMPL__
#include "../shared-code/simple-boot.h" // For error codes
// #include "libpi.small/rpi.h"            // For PUT32, uart_ functions
#include "rpi.h" // When using base libpi directory

/*
 * send_byte
 * ---
 * Provided function that ingests the rpi.h interface to send 
 * a single byte to the UNIX side.
 */
static void send_byte(unsigned char uc) {
  uart_putc(uc);
}

/*
 * get_byte
 * ---
 * Provided function that ingests the rpi.h interface to send
 * a single byte to the UNIX side.
 */
static unsigned char get_byte(void) { 
  return uart_getc();
}

/*
 * get_uint
 * ---
 * Provided function that gets an unsigned (4 byte) chunk of 
 * data. Uses '|=' to avoid compiler optimizations that shuffle
 * instructions when we want to construct a correct 4-byte chunk.
 */
static unsigned get_uint(void) {
	unsigned u = get_byte();
  u |= get_byte() << 8;
  u |= get_byte() << 16;
  u |= get_byte() << 24;
  return u;
}

/*
 * put_uint
 * ---
 * As above, provided function that puts a 4-byte chunk of data
 * in transit to the UNIX side.
 */
static void put_uint(unsigned u) {
  send_byte((u >> 0)  & 0xff);
  send_byte((u >> 8)  & 0xff);
  send_byte((u >> 16) & 0xff);
  send_byte((u >> 24) & 0xff);
}

/*
 * die
 * ---
 * Provided helper, sends an error code (see shared-code/simple-boot.h)
 * and reboots the rpi.
 */
static void die(int code) {
  put_uint(code);
  rpi_reboot();
}

//  Steps:
//	1. wait for SOH, size, cksum from unix side.
//	2. echo SOH, checksum(size), cksum back.
// 	3. wait for ACK.
//	4. read the bytes, one at a time, copy them to ARMBASE.
//	5. verify checksum.
//	6. send ACK back.
//	7. wait 500ms 
//	8. jump to ARMBASE.

/*
 * notmain: Bootloader
 * ---
 * The main bootloader routine.
 */
extern char __bss_start__; // For calculating size of notmain. The code should just be stored in the text segment.
                           // See linker file (memmap) for more info.

void notmain(void) {
	uart_init(); // This hooks into our UART implementation!

	// XXX: cs107e has this delay; doesn't seem to be required if 
	// you drain the uart.
	delay_ms(500);

	/* My implementation begins here: */
  // Wait for SOH byte
  if (get_uint() != SOH) die(BAD_START);

  unsigned nBytes = get_uint();
  unsigned nBytesHash = crc32(&nBytes, sizeof(unsigned));
  unsigned fileHash = get_uint();
  
  if ((unsigned)&__bss_start__ <= ARMBASE + nBytes) die(TOO_BIG);

  put_uint(SOH);
  put_uint(nBytesHash);
  put_uint(fileHash);

  if (get_uint() != ACK) die(NAK);

  // Begin receipt of binary data
  unsigned offset;
  for (offset = 0; offset < nBytes; offset += sizeof(unsigned)) {
    unsigned chunk = get_uint();
    PUT32(ARMBASE + offset, chunk); // Copy starting at ARMBASE
  }
  // Assert end of transmission, otherwise bad end
  if (get_uint() != EOT) die(BAD_END); 

  if (crc32((unsigned char *)ARMBASE, nBytes) == fileHash) put_uint(ACK);
  else die(BAD_CKSUM); // Bad checksum
  /* End of my implementation. */

	// XXX: appears we need these delays or the unix side gets confused.
	// I believe it's b/c the code we call re-initializes the uart; could
	// disable that to make it a bit more clean.
	delay_ms(500);
	// run what client sent.
  BRANCHTO(ARMBASE);
	// should not get back here, but just in case.
	rpi_reboot();
}
