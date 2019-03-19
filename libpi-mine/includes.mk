# We're using a modified libpi: full link here (replace with your own!)
LIBPI_PATH = /Users/garrick/code/cs140e/final-project/libpi-mine/

ARM = arm-none-eabi
CC = $(ARM)-gcc
LD  = $(ARM)-ld
AS  = $(ARM)-as
OD  = $(ARM)-objdump
OCP = $(ARM)-objcopy
LPI = $(LIBPI_PATH)/libpi.a
LPI_PIC = $(LIBPI_PATH)/libpi.PIC.a

CFLAGS = -O -Wall -nostdlib -nostartfiles -ffreestanding  -march=armv6 -std=gnu99 -I$(LIBPI_PATH) -I.
ASFLAGS = --warn --fatal-warnings -mcpu=arm1176jzf-s -march=armv6zk

ifdef USE_PIC
override CFLAGS += \
	    -frecord-gcc-switches\
	    -gdwarf-2\
	    -fdata-sections -ffunction-sections\
	    -Wall\
	    -Wl,--warn-common\
	    -Wl,--gc-sections\
	    -Wl,--emit-relocs\
	    -fPIC\
	    -msingle-pic-base\
	    -mpic-register=r9\
	    -mno-pic-data-is-text-relative

TARGET = libpi.PIC.a
endif
