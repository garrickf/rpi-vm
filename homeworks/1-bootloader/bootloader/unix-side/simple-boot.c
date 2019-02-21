/*
 * simple-boot.c
 * Garrick Fernandez (garrick)
 * ---
 * Exports the simple bootloader routine, which we need!
 */

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "demand.h"
#include "trace.h" // For tracing, lab 5

#define __SIMPLE_IMPL__
#include "../shared-code/simple-boot.h"

static void send_byte(int fd, unsigned char b) {
	if(write(fd, &b, 1) < 0)
		panic("write failed in send_byte\n");
}

static unsigned char get_byte(int fd) {
	unsigned char b;
	int n;
	if((n = read(fd, &b, 1)) != 1)
		panic("read failed in get_byte: expected 1 byte, got %d\n",n);
	return b;
}

// NOTE: the other way to do is to assign these to a char array b and 
//    return *(unsigned)b
// however, the compiler doesn't have to align b to what unsigned 
// requires, so this can go awry.  easier to just do the simple way.
// we do with |= to force get_byte to get called in the right order 
// 	  (get_byte(fd) | get_byte(fd) << 8 ...) 
// isn't guaranteed to be called in that order b/c | is not a seq point.
unsigned get_uint(int fd) {
  unsigned u = get_byte(fd);
  u |= get_byte(fd) << 8;
  u |= get_byte(fd) << 16;
  u |= get_byte(fd) << 24;
  trace_read32(u); // Lab 5 DEBUG
  return u;
}

void put_uint(int fd, unsigned u) {
  trace_write32(u); // Lab 5 DEBUG
  // mask not necessary.
  send_byte(fd, (u >> 0)  & 0xff);
  send_byte(fd, (u >> 8)  & 0xff);
  send_byte(fd, (u >> 16) & 0xff);
  send_byte(fd, (u >> 24) & 0xff);
}

/* 
 * expect
 * ---
 * Simple utility function to check that a u32 read from the 
 * file descriptor matches v. Interprets error codes if they're
 * there.
 */
void expect(const char *msg, int fd, unsigned v) {
	unsigned x = get_uint(fd);
	if (x != v) {
    char *error = "not an error code - invalid data echoed?";
    switch (x) {
      case BAD_CKSUM:
        error = "bad checksum";
        break;
      case BAD_START:
        error = "bad start of transmission";
        break;
      case BAD_END:
        error = "bad end of transmission";
        break;
      case TOO_BIG:
        error = "payload too big";
        break;
      case NAK:
        error = "no acknowledgement/error in transmission";
        break;
    }
		panic("%s: expected %x, got %x (%s)\n", msg, v, x, error);
  }
}

/*
 * simple_boot: UNIX-side Bootloader
 * ---
 * Send bytes using protocol (see handout and pi-side/bootloader.c). 
 *   1) Send SOH, nBytes, cksum of file
 *   2) Wait for echoed data, send ACK
 *   3) Send data (in buf), then EOT
 *   4) Wait for ACK from rpi, end
 * Reads and writes are done using put_uint() and get_uint().
 */
void simple_boot(int fd, const unsigned char * buf, unsigned n) {
  put_uint(fd, SOH);
  put_uint(fd, n); // nBytes
  unsigned fileHash = crc32(buf, n);
  put_uint(fd, fileHash); // Send filehash

  // Wait for reply
  expect("receive echoed SOH", fd, SOH);
  unsigned nBytesHash = crc32(&n, sizeof(unsigned));
  expect("receive crc32 checksum of nBytes", fd, nBytesHash);
  expect("receive echoed file checksum", fd, fileHash);
  put_uint(fd, ACK); // Send ACK (acknowledgement)

  // Send buffer over, 4 bytes at a time
  for (unsigned offset = 0; offset < n; offset += sizeof(unsigned)) {
    unsigned *chunk = (unsigned *)(buf + offset);
    put_uint(fd, *chunk);
    // printf("simple_boot: sending %u\n", *chunk); // DEBUG
  }
  put_uint(fd, EOT); // End-of-transmission

  expect("receive acknowlegement of transmission", fd, ACK);
}
