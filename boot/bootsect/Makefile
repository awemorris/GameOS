CROSS   ?= i686-linux-gnu-
CC      := $(CROSS)gcc
LD      := $(CROSS)ld
OBJCOPY := $(CROSS)objcopy
ASFLAGS := -m32 -ffreestanding -fno-pic -fno-pie -nostdlib
MKDIR   := mkdir -p

all: bootsect-ibm.bin bootsect-nec.bin

#
# IBM PC/AT boot sector code
#

bootsect-ibm.bin: obj-ibm/ibm-pcat.elf
	$(OBJCOPY) -O binary $< $@
	@test $$(stat -c%s $@) -eq 512 # Sanity-check: exactly one sector.

obj-ibm/ibm-pcat.elf: obj-ibm/ibm-pcat.o
	$(LD) -m elf_i386 -T ibm-pcat.lds -o $@ obj-ibm/ibm-pcat.o

obj-ibm/ibm-pcat.o: ibm-pcat.S
	$(MKDIR) obj-ibm
	$(CC) $(ASFLAGS) -c -o $@ $<


#
# NEC P-9800 boot sector code
#

bootsect-nec.bin: obj-nec/nec-pc98.elf
	$(OBJCOPY) -O binary $< $@
	@test $$(stat -c%s $@) -eq 512 # Sanity-check: exactly one sector.

obj-nec/nec-pc98.elf: obj-nec/nec-pc98.o
	$(LD) -m elf_i386 -T nec-pc98.lds -o $@ obj-nec/nec-pc98.o

obj-nec/nec-pc98.o: nec-pc98.S
	$(MKDIR) obj-nec
	$(CC) $(ASFLAGS) -c -o $@ $<

#
# Phony
#

clean:
	rm -rf obj-ibm obj-nec
