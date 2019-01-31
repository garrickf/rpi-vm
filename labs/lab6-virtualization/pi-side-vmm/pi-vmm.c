#include "rpi.h"
#include "pi-vmm-ops.h"

// note: printf will not work unless you do something special.
#undef printf
#define printf  cannot_print

// just from your bootloader.
static void send_byte(unsigned char uc) {
        uart_putc(uc);
}
static unsigned char get_byte(void) {
        return uart_getc();
}

unsigned get_uint(void) {
        unsigned u = get_byte();
        u |= get_byte() << 8;
        u |= get_byte() << 16;
        u |= get_byte() << 24;
        return u;
}

void put_uint(unsigned u) {
        send_byte((u >> 0)  & 0xff);
        send_byte((u >> 8)  & 0xff);
        send_byte((u >> 16) & 0xff);
        send_byte((u >> 24) & 0xff);
}

#define PUT_COMMAND 0
#define GET_COMMAND 1
#define REBOOT_COMMAND 2
// probably should put reply's and CRC32
int notmain ( void ) {
	uart_init();
	delay_ms(100);

        put_uint(OP_READY);
        unsigned addr;
        while(1) {
	       unsigned command = get_uint();
               if (command == OP_WRITE32) {
                        addr = get_uint();
                        unsigned val = get_uint();
                        PUT32(addr, val); // Use lower level on rpi
               } else if (command == OP_READ32) {
                        addr = get_uint();
                        unsigned val = GET32(addr); // Use lower level on rpi
                        put_uint(val);
               } else if (command == OP_DONE) {
                        reboot();
               }
	}
}
