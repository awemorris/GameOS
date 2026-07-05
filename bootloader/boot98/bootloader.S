/*
 * bootloader.S
 * PC-98 HDD-only second-stage bootloader.
 * GNU as, Intel syntax. Assembled as .code16, with a .code32 tail
 * that runs after the switch to 32-bit protected mode.
 *
 * Entry conditions (set up by bootsect.S):
 *   - CS:IP = 0x0800:0x0000 (linear 0x00008000)
 *   - AL    = DA/UA of the boot device (same value the IPL received)
 *   - DS/ES/SS undefined; we reinitialise them here.
 *
 * Responsibilities:
 *   1. Cache DA/UA into `boot_daua` for our own INT 1Bh calls.
 *   2. Read the first sector of the kernel ELF into a low buffer,
 *      parse the ELF32 header, walk PT_LOAD program headers.
 *   3. For each PT_LOAD, read file bytes into a low-memory bounce
 *      buffer (LOWBUF_SEG:0000), then copy each chunk to its final
 *      >= 1MB destination using a temporary 16-bit protected-mode
 *      REP MOVSB with big-limit data segments (himemory.c style).
 *   4. Zero-fill the .bss portion (memsz - filesz) using the same path.
 *   5. Switch to flat 32-bit protected mode and jump to the kernel's
 *      physical entry point, computed from ELF e_entry adjusted by
 *      the first PT_LOAD's (paddr - vaddr) delta.
 *
 * Simplifications (matches elfload.c "手抜き" spirit):
 *   - ELF32 little-endian only, EM_386, ET_EXEC.
 *   - Program header table is assumed to lie within the first 512 bytes
 *     of the ELF (i.e. same sector as the ELF header).
 *   - Only PT_LOAD is honoured; other segment types are ignored.
 *   - No paging is enabled; kernel starts in flat 32-bit mode.
 *   - No relocation is performed.
 */

.intel_syntax noprefix
.code16
.global _start

/* -------------------------------------------------------------------------
 * Compile-time configuration
 * ------------------------------------------------------------------------- */

/* Must match bootsect.S and the Makefile. */
.equ BOOTLOADER_SECTORS, 31
.equ PARTTAB_LBA,        1
.equ LOADER_LBA,         2
.equ KERNEL_LBA,         (LOADER_LBA + BOOTLOADER_SECTORS)

/* HDD geometry: must match bootsect.S. */
.equ HDD_SECTORS_PER_TRACK, 17
.equ HDD_HEADS,             8
.equ PC98_HDD_READ_AH,      0x56
.equ HDD_DTL_512,           2

/* Low-memory bounce buffer used to receive one 512B sector from BIOS
 * before we copy it to high memory (>= 1MB).
 *   LOWBUF_SEG : real-mode segment
 *   LOWBUF_LIN : same location expressed as a 32-bit linear address
 */
.equ LOWBUF_SEG, 0x7000
.equ LOWBUF_LIN, 0x00070000

/* If a PT_LOAD's p_paddr is below 1MB (0x00100000), we redirect it
 * to this fallback so the kernel still lands in high memory.
 */
.equ LOAD_FALLBACK, 0x00100000

/* ELF32 header / program header field offsets (see elf.h). */
.equ E_ENTRY,     24     /* Elf32_Ehdr.e_entry */
.equ E_PHOFF,     28     /* Elf32_Ehdr.e_phoff */
.equ E_PHENTSIZE, 42     /* Elf32_Ehdr.e_phentsize */
.equ E_PHNUM,     44     /* Elf32_Ehdr.e_phnum */
.equ PT_TYPE,      0     /* Elf32_Phdr.p_type */
.equ PT_OFFSET,    4     /* Elf32_Phdr.p_offset (file offset) */
.equ PT_VADDR,     8     /* Elf32_Phdr.p_vaddr  (virtual address) */
.equ PT_PADDR,    12     /* Elf32_Phdr.p_paddr  (physical address) */
.equ PT_FILESZ,   16     /* Elf32_Phdr.p_filesz */
.equ PT_MEMSZ,    20     /* Elf32_Phdr.p_memsz  */
.equ PT_LOAD,      1     /* p_type == PT_LOAD */

/* -------------------------------------------------------------------------
 * Entry point (real mode)
 * ------------------------------------------------------------------------- */
_start:
    cli
    xor ax, ax
    mov ds, ax
    mov ss, ax
    mov sp, 0x7c00        /* Reuse the same stack as the IPL. */
    sti

    /* Cache DA/UA passed from the IPL. */
    mov [boot_daua], al

    /* ----- Read sector 0 of the kernel image (contains ELF hdr + PHDRs). */
    mov ax, LOWBUF_SEG
    mov es, ax
    xor bp, bp
    mov si, KERNEL_LBA
    call hdd_read_lba_512_esbp
    jc fatal

    /* Set DS = ES so we can address the just-loaded ELF header at [0]. */
    push es
    pop  ds

    /* ----- Validate ELF magic and class. */
    cmp dword ptr [0], 0x464c457f  /* "\x7FELF" little-endian */
    jne fatal
    cmp byte  ptr [4], 1           /* EI_CLASS = ELFCLASS32 */
    jne fatal
    cmp byte  ptr [5], 1           /* EI_DATA  = ELFDATA2LSB */
    jne fatal

    /* Save e_entry for later use during the PM32 jump. */
    mov eax, dword ptr [E_ENTRY]
    mov dword ptr [elf_entry], eax

    /* Prepare PHDR walk:
     *   CX = phnum   (loop count)
     *   BX = phentsize (stride to next PHDR)
     *   SI = current PHDR pointer, initialised from e_phoff.
     *
     * NOTE: this assumes e_phoff < 512 so the PHDRs are already in
     * our low buffer. That matches what elfload.c-style images produce.
     */
    movzx cx, word ptr [E_PHNUM]
    movzx bx, word ptr [E_PHENTSIZE]
    mov   si, word ptr [E_PHOFF]

    /* Track the lowest-vaddr PT_LOAD, so we can translate e_entry
     * (virtual) into a physical address at handoff time.
     */
    mov dword ptr [first_vaddr], 0xffffffff
    mov dword ptr [first_paddr], 0

/* ---- PHDR loop -------------------------------------------------------- */
ph_loop:
    push cx                       /* Save loop counter (CX may be clobbered). */
    push bx                       /* Save stride (BX may be clobbered).       */

    cmp dword ptr [si + PT_TYPE], PT_LOAD
    jne ph_next                   /* Skip non-loadable segments. */

    /* If this PT_LOAD has the smallest vaddr so far, remember it. */
    mov eax, dword ptr [si + PT_VADDR]
    cmp eax, dword ptr [first_vaddr]
    jae have_first
    mov dword ptr [first_vaddr], eax

    /* Take its physical address, forcing >= 1MB. */
    mov edx, dword ptr [si + PT_PADDR]
    cmp edx, 0x00100000
    jae 1f
    mov edx, LOAD_FALLBACK
1:
    mov dword ptr [first_paddr], edx

have_first:
    call load_one_segment         /* Load this PT_LOAD into high memory. */

ph_next:
    pop bx
    add si, bx                    /* Advance to next PHDR (stride = phentsize). */
    pop cx
    loop ph_loop

    /* All segments in place: enter PM32 and jump to the kernel. */
    call enter_pm32_and_jump

/* Fatal error: halt. */
fatal:
    cli
1:  hlt
    jmp 1b

/* -------------------------------------------------------------------------
 * load_one_segment
 *
 *   Copies one PT_LOAD segment from disk to its high-memory destination,
 *   using LOWBUF as a 512-byte bounce buffer.
 *
 *   Preconditions:
 *     DS = LOWBUF_SEG   (so [si+..] indexes the ELF/PHDR image)
 *     SI = pointer to the current PHDR within the low buffer
 * ------------------------------------------------------------------------- */
load_one_segment:
    pushad                        /* Save all 32-bit regs. 386+ only. */

    /* Destination physical address, forced to >= 1MB. */
    mov eax, dword ptr [si + PT_PADDR]
    cmp eax, 0x00100000
    jae 1f
    mov eax, LOAD_FALLBACK
1:
    mov dword ptr [dst_linear], eax

    /* remain  = number of file bytes still to copy */
    mov eax, dword ptr [si + PT_FILESZ]
    mov dword ptr [remain], eax

    /* file_off = current absolute offset within the ELF file */
    mov eax, dword ptr [si + PT_OFFSET]
    mov dword ptr [file_off], eax

read_loop:
    cmp dword ptr [remain], 0
    je  zero_bss                  /* Done with file bytes -> handle .bss */

    /* Convert file_off -> (sector index, in-sector offset). */
    mov eax, dword ptr [file_off]
    xor edx, edx
    mov ecx, 512
    div ecx                       /* EAX = sector, EDX = byte-in-sector */
    add ax, KERNEL_LBA            /* Turn into disk LBA. */
    mov si, ax

    /* Read one 512B sector into LOWBUF. */
    mov ax, LOWBUF_SEG
    mov es, ax
    xor bp, bp
    call hdd_read_lba_512_esbp
    jc  fatal

    /* copy_len = min(512 - edx, remain) */
    mov eax, 512
    sub eax, edx
    cmp eax, dword ptr [remain]
    jbe 1f
    mov eax, dword ptr [remain]
1:
    mov dword ptr [copy_len], eax

    /* Source linear = LOWBUF_LIN + in-sector offset. */
    mov eax, LOWBUF_LIN
    add eax, edx
    mov dword ptr [src_linear], eax

    /* Perform low->high copy via temporary 16-bit protected mode. */
    call copy_high

    /* Advance counters. */
    mov eax, dword ptr [copy_len]
    add dword ptr [file_off],   eax
    add dword ptr [dst_linear], eax
    sub dword ptr [remain],     eax
    jmp read_loop

zero_bss:
    /* Compute .bss size = memsz - filesz.
     * If <= 0, nothing to do.
     */
    mov eax, dword ptr [si + PT_MEMSZ]
    sub eax, dword ptr [si + PT_FILESZ]
    jbe done_seg
    mov dword ptr [remain], eax

    /* Zero the low buffer once, then copy zeros in 512B chunks. */
    mov ax, LOWBUF_SEG
    mov es, ax
    xor di, di
    xor ax, ax
    mov cx, 256
    rep stosw                     /* 256 words = 512 bytes of zeros */

zero_loop:
    cmp dword ptr [remain], 0
    je  done_seg
    mov eax, 512
    cmp eax, dword ptr [remain]
    jbe 1f
    mov eax, dword ptr [remain]
1:
    mov dword ptr [copy_len],   eax
    mov dword ptr [src_linear], LOWBUF_LIN
    call copy_high
    mov eax, dword ptr [copy_len]
    add dword ptr [dst_linear], eax
    sub dword ptr [remain],     eax
    jmp zero_loop

done_seg:
    popad
    ret

/* -------------------------------------------------------------------------
 * hdd_read_lba_512_esbp
 *
 * Same contract and math as in bootsect.S; duplicated here so the
 * second-stage doesn't have to link against the boot sector.
 *
 *   Input : SI = LBA, ES:BP = buffer
 *   Output: CF from BIOS
 *   Clobbers: AX, BX, CX, DX
 * ------------------------------------------------------------------------- */
hdd_read_lba_512_esbp:
    push ds
    push si
    push di

    xor ax, ax
    mov ds, ax                   /* DS = 0 to reach [boot_daua], [sector_no] */

    mov ax, si
    xor dx, dx
    mov bx, HDD_SECTORS_PER_TRACK
    div bx                        /* AX = temp, DX = sector-1 */
    mov  [sector_no], dl
    inc  byte ptr [sector_no]     /* Sector numbers are 1-based. */

    xor dx, dx
    mov bx, HDD_HEADS
    div bx                        /* AX = cyl, DX = head */

    mov cx, ax                    /* CX = cylinder */
    mov dh, dl                    /* DH = head    */
    mov dl, [sector_no]           /* DL = sector  */

    mov ah, PC98_HDD_READ_AH
    mov al, [boot_daua]           /* Reload DA/UA into AL every call. */
    mov bx, HDD_DTL_512

    int 0x1b

    pop di
    pop si
    pop ds
    ret

/* -------------------------------------------------------------------------
 * copy_high
 *
 *   Copy copy_len bytes from linear src_linear to linear dst_linear
 *   using a short excursion into 16-bit protected mode. This mirrors
 *   the technique used in himemory.c (linear_memcpy):
 *
 *     - Build a 3-entry GDT with two data descriptors whose bases
 *       are src_linear and dst_linear respectively, both with a
 *       64KB limit (fine because copy_len <= 512 in our caller).
 *     - Disable interrupts, LGDT, set CR0.PE=1.
 *     - Far-jump to a 16-bit code selector (still in real-mode-like
 *       addressing but with protected-mode segment bases).
 *     - REP MOVSB from selector 0x08:0000 to selector 0x10:0000.
 *     - Clear CR0.PE=0, far-jump back to real mode, re-enable IRQs.
 * ------------------------------------------------------------------------- */
copy_high:
    pushad
    cli
    call make_copy_gdt            /* Patch GDT bases with src/dst_linear. */
    lgdt [gdtr]

    mov eax, cr0
    or  eax, 1                    /* CR0.PE = 1 : enter protected mode */
    mov cr0, eax
    jmp 0x18:pm16_copy            /* Selector 0x18 = 16-bit code descriptor */

pm16_copy:
    /* DS = source data descriptor, ES = destination data descriptor. */
    mov ax, 0x08
    mov ds, ax
    mov ax, 0x10
    mov es, ax

    xor si, si
    xor di, di
    mov cx, word ptr [copy_len]   /* copy_len is <= 512, fits in CX. */
    cld
    rep movsb                     /* Bulk transfer. */

    /* Leave protected mode. */
    mov eax, cr0
    and eax, 0xfffffffe
    mov cr0, eax
    jmp 0x0000:real_after_copy    /* Reload CS with real-mode value 0. */

real_after_copy:
    xor ax, ax
    mov ds, ax                    /* Restore DS = 0 for our variables. */
    sti
    popad
    ret

/* Fill the base fields of gdt_src / gdt_dst from src_linear / dst_linear. */
make_copy_gdt:
    pushad
    mov eax, dword ptr [src_linear]
    mov bx,  gdt_src
    call fill_desc_base
    mov eax, dword ptr [dst_linear]
    mov bx,  gdt_dst
    call fill_desc_base
    popad
    ret

/* Write EAX (a 32-bit linear base) into a GDT descriptor at DS:BX.
 * Descriptor layout:
 *   +0..+1 : limit  0..15
 *   +2..+3 : base   0..15
 *   +4     : base   16..23
 *   +5     : access byte
 *   +6     : limit  16..19 + flags
 *   +7     : base   24..31
 */
fill_desc_base:
    mov [bx + 2], al
    shr eax, 8
    mov [bx + 3], al
    shr eax, 8
    mov [bx + 4], al
    shr eax, 8
    mov [bx + 7], al
    ret

/* -------------------------------------------------------------------------
 * enter_pm32_and_jump
 *
 *   Switch to flat 32-bit protected mode and jump to the kernel's
 *   physical entry point.
 *
 *     physical_entry = first_paddr + (elf_entry - first_vaddr)
 *
 *   which handles the common case where the ELF is linked at a high
 *   virtual address (e.g. 0x80100000) but physically loaded at 1MB
 *   (see sections.lds.txt: VMA 0x80100000, AT 0x00100000).
 * ------------------------------------------------------------------------- */
enter_pm32_and_jump:
    cli
    lgdt [pm32_gdtr]
    mov eax, cr0
    or  eax, 1
    mov cr0, eax
    jmp 0x08:pm32_entry            /* 0x08 = flat 32-bit code selector */

.code32
pm32_entry:
    /* Reload data segments with the flat data selector. */
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x0009f000            /* Stack just below A000:0000. */

    /* Compute physical entry = first_paddr + (elf_entry - first_vaddr). */
    mov eax, dword ptr [elf_entry]
    sub eax, dword ptr [first_vaddr]
    add eax, dword ptr [first_paddr]
    jmp eax

/* -------------------------------------------------------------------------
 * Data section (still reachable in real mode via DS=0)
 * ------------------------------------------------------------------------- */
.code16
.align 8

/* 16-bit copy GDT: null, data-src (0x08), data-dst (0x10), code16 (0x18). */
gdt:
    .quad 0
gdt_src:
    .word 0xffff; .word 0; .byte 0; .byte 0x92; .byte 0x00; .byte 0
gdt_dst:
    .word 0xffff; .word 0; .byte 0; .byte 0x92; .byte 0x00; .byte 0
gdt_code16:
    .word 0xffff; .word 0; .byte 0; .byte 0x9a; .byte 0x00; .byte 0

gdtr:
    .word gdtr - gdt - 1
    .long gdt

/* 32-bit flat GDT used for the final handoff. */
.align 8
pm32_gdt:
    .quad 0
    /* 0x08: flat 32-bit code, base=0, limit=4GB, DPL=0 */
    .word 0xffff; .word 0; .byte 0; .byte 0x9a; .byte 0xcf; .byte 0
    /* 0x10: flat 32-bit data, base=0, limit=4GB, DPL=0 */
    .word 0xffff; .word 0; .byte 0; .byte 0x92; .byte 0xcf; .byte 0
pm32_gdtr:
    .word pm32_gdtr - pm32_gdt - 1
    .long pm32_gdt

/* Cached DA/UA (see file header). */
boot_daua:    .byte 0
/* Spill for sector number in LBA->CHS conversion. */
sector_no:    .byte 0

/* Copy engine state. */
src_linear:   .long 0
dst_linear:   .long 0
copy_len:     .long 0

/* Segment load state. */
remain:       .long 0
file_off:     .long 0

/* ELF handoff state. */
elf_entry:    .long 0
first_vaddr:  .long 0
first_paddr:  .long 0
