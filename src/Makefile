# Makefile for project, kitbashed together from previous labs :)
# See: http://scottmcpeak.com/autodepend/autodepend.html for more info

# Define the name of the executable and the object files that need to linked to create it.
NAME = pi-vm
OBJS = driver.o vm-asm.o cp15-arm.o mmu.o bvec.o interrupts-c.o interrupts-asm.o cpsr-util-asm.o

# We're using a modified libpi: full link here (replace with your own!)
LIBPI_PATH = /Users/garrick/code/cs140e/final-project/libpi-mine/

# Pull in useful set of defines
include $(LIBPI_PATH)/includes.mk

# Compile flags
CPP_ASFLAGS = -nostdlib -nostartfiles -ffreestanding -Wa,--warn -Wa,--fatal-warnings -Wa,-mcpu=arm1176jzf-s -Wa,-march=armv6zk
CFLAGS += -Wstack-usage=512 -Werror

# -marm_prefer_ldrd_strd

CFLAGS += -Wno-unused-function

# all: libpi $(NAME).bin mktags run
all: libpi $(NAME).bin run

run:
	my-install $(NAME).bin

# is there a better way to do this?
libpi:
	@make -C $(LIBPI_PATH)

# Why are we generating ctags?
mktags:
	@rm -f tags
	ctags *.[chsS] $(LIBPI_PATH)/*.[chsS]

$(NAME).bin: memmap $(OBJS) vm-start.o 
	$(LD) vm-start.o $(OBJS) -T memmap -o $(NAME).elf  $(LPI)
	$(OD) -D $(NAME).elf > $(NAME).list
	$(OCP) $(NAME).elf -O binary $(NAME).bin

# pull in dependency info for *existing* .o files
-include $(OBJS:.o=.d)

# compile and generate dependency info;
# more complicated dependency computation, so all prereqs listed
# will also become command-less, prereq-less targets
#   sed:    strip the target (everything before colon)
#   sed:    remove any continuation backslashes
#   fmt -1: list words one per line
#   sed:    strip leading spaces
#   sed:    add trailing colons
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c -o $*.o
	$(CC) -MM $(CFLAGS) $*.c > $*.d
	@mv -f $*.d $*.d.tmp
	@sed -e 's|.*:|$*.o:|' < $*.d.tmp > $*.d
	@sed -e 's/.*://' -e 's/\\$$//' < $*.d.tmp | fmt -1 | \
	  sed -e 's/^ *//' -e 's/$$/:/' >> $*.d
	@rm -f $*.d.tmp

.s.o: 
	$(AS) $(ASFLAGS) $< -o $@

.S.o: 
	$(CC) -c $(CPP_ASFLAGS) $< -o $@

# Remove all compilation products
clean:
	rm -f replay *.o *.d *.bin *.elf *.list *.img Makefile.bak *~ tags
