CROSS   ?= i686-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
ASFLAGS := -m32 -ffreestanding -fno-pic -fno-pie -nostdlib
MKDIR   := mkdir -p

#
# IPL (boot sector)
#

obj/bootsect.bin: obj/bootsect.elf
	$(OBJCOPY) -O binary $< $@
	@test $$(stat -c%s $@) -eq 512  # Sanity-check: exactly one sector.

obj/bootsect.elf: obj/bootsect.o
	$(LD) -m elf_i386 -T bootsect.ld -o $@ obj/bootsect.o

obj/bootsect.o: bootsect.S
	$(MKDIR) obj
	$(CC) $(ASFLAGS) -c -o $@ $<

#
# Phony
#

clean:
	rm -rf obj
