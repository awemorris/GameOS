CROSS   ?= i686-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
ASFLAGS := -m32 -ffreestanding -fno-pic -fno-pie -nostdlib
MKDIR   := mkdir -p

# Must match BOOTLOADER_SECTORS in the .S files.
BOOTLOADER_SECTORS := 32

#
# Final image = IPL + padded loader + kernel ELF.
#

obj/greenboot98.bin: obj/bootsect.bin obj/bootloader.pad obj/parttab.bin
	cat obj/bootsect.bin obj/bootloader.pad > $@

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
# Second Stage (boot loader)
#

obj/bootloader.pad: obj/bootloader.bin
	cp $< $@
	truncate -s $$(( $(BOOTLOADER_SECTORS) * 512 )) $@

obj/bootloader.bin: obj/bootloader.elf
	$(OBJCOPY) -O binary $< $@

obj/bootloader.elf: obj/bootloader.o
	$(LD) -m elf_i386 -T bootloader.ld -o $@ obj/bootloader.o

obj/bootloader.o: bootloader.S
	$(MKDIR) obj
	$(CC) $(ASFLAGS) -c -o $@ $<

#
# Padding
#

obj/parttab.bin:
	dd if=/dev/zero of=$@ bs=512 count=1 status=none


#
# Phony
#

clean:
	rm -f *.o *.elf *.bin *.pad image.bin
