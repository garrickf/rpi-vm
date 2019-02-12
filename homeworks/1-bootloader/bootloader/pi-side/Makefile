# Makefile using libpi path, my-uart and my-gpio.c impl. Cited from newsgroup
CC = arm-none-eabi-gcc
CFLAGS = -I$(LIBPI_PATH) -I. -Wall -Og -nostdlib -nostartfiles -ffreestanding
SRC = bootloader.c my-uart.c my-gpio.c
OBJS = $(SRC:.c=.o)

all: kernel.img

start.o: start.s
	arm-none-eabi-as start.s -o start.o

kernel.img: memmap start.o $(OBJS)
	make -C $(LIBPI_PATH)
	arm-none-eabi-ld start.o $(OBJS) $(LIBPI_PATH)/libpi.a -T memmap -o bootloader.elf
	arm-none-eabi-objdump -D bootloader.elf > bootloader.list
	arm-none-eabi-objcopy bootloader.elf -O binary kernel.img

clean:
	rm -f *.o *.bin *.elf *.list *.img *~ Makefile.bak
	make -C libpi.small clean

depend:
	makedepend -I$(LIBPI)/.. *.[ch] 

# DO NOT DELETE 