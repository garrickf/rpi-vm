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
//	return *(unsigned)b
// however, the compiler doesn't have to align b to what unsigned 
// requires, so this can go awry.  easier to just do the simple way.
// we do with |= to force get_byte to get called in the right order 
// 	(get_byte(fd) | get_byte(fd) << 8 ...) 
// isn't guaranteed to be called in that order b/c | is not a seq point.
static unsigned get_uint(int fd) {
        unsigned u = get_byte(fd);
        u |= get_byte(fd) << 8;
        u |= get_byte(fd) << 16;
        u |= get_byte(fd) << 24;
        return u;
}

void put_uint(int fd, unsigned u) {
	// mask not necessary.
        send_byte(fd, (u >> 0)  & 0xff);
        send_byte(fd, (u >> 8)  & 0xff);
        send_byte(fd, (u >> 16) & 0xff);
        send_byte(fd, (u >> 24) & 0xff);
}

// simple utility function to check that a u32 read from the 
// file descriptor matches <v>.
void expect(const char *msg, int fd, unsigned v) {
	unsigned x = get_uint(fd);
	if(x != v) 
		panic("%s: expected %x, got %x\n", msg, v,x);
}

// unix-side bootloader: send the bytes, using the protocol.
// read/write using put_uint() get_unint().
void simple_boot(int fd, const unsigned char * buf, unsigned n) { 
	// HW1: send SOH (start-of-header)
  put_uint(fd, SOH);
  // expect("receive SOH byte", fd, SOH);
  printf("simple_boot: sending size %u\n", n);
  put_uint(fd, n); // Send nBytes
  unsigned nBytesHash = crc32(&n, sizeof(unsigned));
  // expect("recieve numBytes hash", fd, nBytesHash); // TODO: send chksum (check buffer, nBytes size. is nBytes + 1 for null term char? include padding?)
  unsigned fileHash = crc32(buf, n);
  put_uint(fd, fileHash); // Send filehash

  // Wait for reply
  expect("receive echoed SOH", fd, SOH);
  expect("receive crc32 checksum of nBytes", fd, nBytesHash);
  expect("receive echoed file checksum", fd, fileHash);

  // TODO: send buffer over 4 bytes at a time

  for (unsigned i = 1; i < 10; i++) {
		put_uint(fd, i);
		fprintf(stderr, "sending %d...\n", i);
		expect("ping-pong", fd, i+1);
	}
  // Send a reboot signal
  put_uint(fd, 0x12345678);
	exit(0);
}
