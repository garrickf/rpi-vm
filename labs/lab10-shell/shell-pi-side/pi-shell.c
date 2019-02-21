#include "rpi.h"
#include "pi-shell.h"

// read characters until we hit a newline.
static int readline(char *buf, int sz) {
	for(int i = 0; i < sz; i++) {
		if((buf[i] = uart_getc()) == '\n') {
			buf[i] = 0;
			return i;
		}
	}
	// just return partial read?
	panic("size too small\n");
}

static const char pi_done[] = "PI REBOOT!!!\n";
static const char cmd_done[] = "CMD-DONE\n";

void notmain() { 
	uart_init();
	int n;
	char buf[1024];

	while((n = readline(buf, sizeof buf))) {
		if (strncmp(buf, "echo ", 4) == 0) {
			printk("%s\n", buf);
        } else if (strncmp(buf, "reset", 5) == 0) {
        	printk("%s", pi_done);
			delay_ms(100);
			rpi_reboot();
		} else if (strncmp(buf, "run", 3) == 0) {
			load_code(); // In bootloader.c (in this lab!)
			printk("%s", cmd_done);
		} else {
			unimplemented();
		}
	}
	clean_reboot();
}
