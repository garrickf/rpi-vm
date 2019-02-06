// simple hello world to test that your new makefile arrangement works.
#include "rpi.h"

void notmain(void) {
	uart_init();
	printk("hello world\n");
	clean_reboot();
}
