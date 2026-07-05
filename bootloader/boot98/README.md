# PC-98 IPL + second-stage loader skeleton

This is a starting point for a NEC PC-98 boot sector and second-stage ELF loader.
It assumes 512-byte sectors and BIOS INT 1Bh READ DATA. Edit geometry and DA/UA
handling for your target HDD/FDD format.

Build:

```sh
make CROSS=i386-elf-
cp /path/to/kernel.elf ./kernel.elf
make image.bin
```

Important tuning points:

- `BOOTLOADER_SECTORS` must match both `bootsect.S`, `bootloader.S`, and `Makefile`.
- `SECTORS_PER_TRACK`, `HEADS`, and `PC98_READ_AH` are deliberately constants.
- The loader assumes program headers fit in the first 512 bytes of the ELF.
- The handoff computes physical entry as `first_paddr + e_entry - first_vaddr`.
