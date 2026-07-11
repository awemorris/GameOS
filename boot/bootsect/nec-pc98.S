/*
 * bootsect.S
 *
 * PC98 boot sector with tiny ELF32 PT_LOAD loader.
 *
 * Layout:
 *   LBA 0        : Boot sector / MBR
 *   LBA 1-65535  : Kernel ELF
 *   LBA 65536    : First partition
 *
 * Assumptions:
 *   - BIOS loads this sector at 0000:7c00, or equivalent physical 0x7c00.
 *   - BIOS passes boot drive in DL.
 *   - INT 13h Extensions AH=42h are available. No check is performed.
 *   - Kernel ELF starts at LBA 2.
 *   - ELF header and PHDR table fit in the first 512 bytes.
 *   - ELF is valid; no validation is performed.
 *   - BSS is not zero-filled here.
 *   - 386+.
 *   - PT_LOAD.p_offset is 512-byte aligned.
 *     4KB alignment is sufficient.
 *   - Kernel VMA has bit 31 set:
 *
 *       VMA 0x80100000 -> physical 0x00100000
 *
 *     Therefore this loader converts VMA to physical by clearing bit 31.
 *
 * This version preserves the MBR partition table area at 0x1be.
 */

.intel_syntax noprefix
.code16
.global _start

/*
 * LBA sector index of the start of the ELF kernel.
 */
.equ KERNEL_LBA, 1

/*
 * Base address
 */
.equ BOOT_BASE, 0x1fc00

/*
 * Separate buffers.
 *
 * HDRBUF keeps the ELF header and PHDR table alive.
 * SECBUF is reused for sector reads while loading PT_LOAD contents.
 */
.equ HDRBUF_SEG,     0x7000
.equ SECBUF_SEG,     0x7200

/*
 * ELF32 header offsets.
 */
.equ E_ENTRY,        24
.equ E_PHOFF,        28
.equ E_PHENTSIZE,    42
.equ E_PHNUM,        44

/*
 * ELF32 program header offsets.
 */
.equ PT_TYPE,         0
.equ PT_OFFSET,       4
.equ PT_VADDR,        8
.equ PT_FILESZ,      16
.equ PT_LOAD,         1

/*
 * Work variables outside the boot sector.
 *
 * BIOS loads this sector at 0x7c00..0x7dff.
 * We use 0x7e00 upward as scratch RAM.
 *
 * These are absolute physical addresses accessed with DS=0.
 */
.equ VAR_BASE,     0x7e00

.equ sec_no,       VAR_BASE + 0x00
.equ copy_len,     VAR_BASE + 0x02
.equ dst_linear,   VAR_BASE + 0x04
.equ remain,       VAR_BASE + 0x08
.equ file_off,     VAR_BASE + 0x0c

.equ elf_entry,    VAR_BASE + 0x12

.equ multiboot_flags,		VAR_BASE + 0x16
.equ multiboot_mem_lower,	VAR_BASE + 0x1a
.equ multiboot_mem_upper,	VAR_BASE + 0x1e

.equ BOOT_DAUA,    0x0584

_start:
	/*
	 * Establish a minimal real-mode environment.
	 *
	 * DS=0 is required because variables and descriptors are accessed
	 * through absolute physical addresses.
	 */
	cli

	/*
	 * lodsb in copy_secbuf_to_high needs DF=0;
	 * BIOS does not guarantee it.
	 */
	cld

	xor bx, bx
	mov ds, bx
	mov ss, bx
	mov sp, 0x7c00

	/*
	 * Make pseudo multiboot info.
	 */
	mov eax, 1
	mov [multiboot_flags], eax		/* MBINFO_FLAG_MEMORY */
	mov eax, 128 * 1024
	mov [multiboot_mem_upper], eax		/* Memory size above 1MB in KB */

	/*
 	 * Enable A20.
	 */
	xor al, al
	out 0xf2, al

	/*
	 * ES points to the header buffer.
	 *
	 * The ELF header and PHDRs remain in HDRBUF for the whole load.
	 */
	mov ax, HDRBUF_SEG
	mov es, ax

	/*
	 * Read the kernel top sector only once.
	 *
	 * This sector contains the ELF header and, by assumption, all PHDRs.
	 */
	mov ax, KERNEL_LBA	/* Currently 16-bit */
	mov bx, HDRBUF_SEG
	call read_lba_buf

	/*
	 * Save ELF entry.
	 *
	 * No ELF validation is done.
	 */
	mov eax, dword ptr es:[E_ENTRY]
	mov [elf_entry], eax

	/*
	 * Prepare PHDR iteration.
	 *
	 * CX = number of PHDRs
	 * BX = PHDR entry size
	 * SI = current PHDR offset inside HDRBUF
	 */
	mov cx, word ptr es:[E_PHNUM]
	mov bx, word ptr es:[E_PHENTSIZE]
	mov si, word ptr es:[E_PHOFF]

ph_loop:
	/*
	 * If all PHDRs have been processed, enter 32-bit mode.
	 */
	jcxz enter_pm32_and_jump

	/*
	 * Only PT_LOAD entries need disk loading.
	 */
	cmp dword ptr es:[si + PT_TYPE], PT_LOAD
	jne ph_next

	/*
	 * Load this PT_LOAD segment.
	 *
	 * ES:SI still points into HDRBUF.
	 */
	call load_one_segment

ph_next:
	/*
	 * Advance to the next PHDR.
	 */
	add si, bx
	loop ph_loop

/*
 * enter_pm32_and_jump
 *
 * Switch to flat 32-bit protected mode and jump to the kernel.
 */
enter_pm32_and_jump:
	cli

	/*
	 * Load the protected-mode GDT.
	 */
	lgdt cs:[gdtr]

	/*
	 * Set CR0.PE.
	 */
	mov eax, cr0
	or al, 1
	mov cr0, eax
	jmp real_to_prot_for_kernel
real_to_prot_for_kernel:

	/*
	 * Move to 32-bit mode and jump to pm32_entry.
	 */
	xor eax, eax
	mov ax, offset pm32_entry
	add eax, BOOT_BASE
	push 0x08
	push eax
	.byte 0x66
	retf

/*
 * load_one_segment
 *
 * Input:
 *   ES:SI = Elf32_Phdr in HDRBUF.
 *
 * Assumptions:
 *   - p_offset is 512-byte aligned.
 *   - p_vaddr has bit 31 set.
 *
 * BSS is ignored.
 */
load_one_segment:
	pushad
	push es

	/*
	* Destination physical address.
	*
	* VMA 0x801xxxxx becomes physical 0x001xxxxx.
	*/
	mov eax, dword ptr es:[si + PT_VADDR]
	btr eax, 31
	mov [dst_linear], eax

	/*
	* Number of file bytes to load.
	*/
	mov eax, dword ptr es:[si + PT_FILESZ]
	mov [remain], eax

	/*
	* Current file offset in the ELF image.
	*
	* This must start on a 512-byte boundary.
	*/
	mov eax, dword ptr es:[si + PT_OFFSET]
	mov [file_off], eax

seg_loop:
	/*
	 * Stop when this PT_LOAD's file bytes have been copied.
	 */
	cmp dword ptr [remain], 0
	je seg_done

	/*
	 * Convert file offset to disk LBA.
	 *
	 * Since p_offset is sector-aligned and we advance by full sectors
	 * except possibly the final partial copy, the source offset within
	 * SECBUF is always zero.
	 *
	 *   lba = KERNEL_LBA + (file_off >> 9)
	 */
	mov eax, dword ptr [file_off]
	shr eax, 9
	add ax, KERNEL_LBA

	/*
	 * Read this sector into SECBUF, not HDRBUF.
	 *
	 * Therefore the ELF header and PHDRs are not destroyed.
	 */
	mov bx, SECBUF_SEG
	call read_lba_buf

	/*
	 * copy_len = min(512, remain)
	 *
	 * Only the final sector of a segment may be partial.
	 */
	mov eax, 512
	cmp eax, dword ptr [remain]
	jbe 1f
	mov eax, dword ptr [remain]
1:
	mov [copy_len], ax

	/*
	 * Copy SECBUF:0000 to the high-memory destination.
	 */
	call copy_secbuf_to_high

	/*
	 * Advance file offset, destination address, and remaining byte count.
	 */
	movzx eax, word ptr [copy_len]
	add dword ptr [file_off], eax
	add dword ptr [dst_linear], eax
	sub dword ptr [remain], eax

	jmp seg_loop

seg_done:
	pop es
	popad
	ret

/*
 * copy_secbuf_to_high
 *
 * Copy:
 *   SECBUF_SEG:0000
 *      ->
 *   linear dst_linear
 *
 * GS is an unreal-mode flat 4GB data segment.
 */
copy_secbuf_to_high:
	pushad
	push ds
	push es

	/*
	 * Load the protected-mode GDT.
	 *  CS = 0x1fc0
	 */
	lgdt cs:[gdtr]

	/* Source address 0x72000 --> ESI */
	mov esi, SECBUF_SEG
	shl esi, 4

	/* Destination address --> EDI */
	mov edi, dword ptr [dst_linear]

	/* Copy bytes --> ECX */
	movzx ecx, word ptr [copy_len]

	cli

	/* Enter protected mode. */
	mov eax, cr0
	or al, 1
	mov cr0, eax
	jmp real_to_prot_tmp
real_to_prot_tmp:

	/* Load selectores. */
	mov ax, 0x10
	mov ds, ax
	mov es, ax

	/* Copy. */
//	.byte 0x67	/* 32-bit prefix */
//	rep movsb
copy_loop:
	mov al, [esi]
	mov [edi], al
	inc esi
	inc edi
	dec ecx
	jnz copy_loop

	/* Leave for real mdoe. */
	mov eax, cr0
	and al, 0xfe
	mov cr0, eax
	jmp prot_to_real_tmp
prot_to_real_tmp:

	sti

	pop es
	pop ds
	popad
	ret

/*
 * read_lba_buf
 *
 * Input:
 * AX = 16-bit LBA
 * BX = destination segment
 *
 * Reads one 512-byte sector to BX:0000 using PC-98 INT 1Bh.
 *
 * ES is preserved because the PHDR loop keeps ES pointing at HDRBUF.
 */
read_lba_buf:
    pusha
    push es

    /* Save the destination segment (BX) and LBA (AX) to the stack. */
	push bx
	push ax

    /* Get the geometry. */
    mov ah, 0x84
    mov al, [BOOT_DAUA]
    int 0x1b
	jnc 1f
    mov dh, 8               /* heads */
    mov dl, 17             /* sectors/track */
1:

	/* Sectors(DL) -> BX, Heads(DH) -> CX */
    movzx bx, dl            /* BX = sectors */
    movzx cx, dh            /* CX = heads   */

	/* Restore LBA. */
	pop ax

	/* LBA(AX) / Sectors(BX) */
    xor dx, dx
    div bx                  /* AX=LBA/sec, DX=sector */
    push dx

	/* LBA/Sectors(AX) / Heads(CX) */
    xor dx, dx
    div cx                  /* AX=cyl, DX=head */
    mov cx, ax              /* CX = cylinder */
    mov dh, dl              /* DH = head     */

	/* Restore Sectors to AX. */
    pop ax
    mov dl, al              /* DL = sector   */

    /* Restore destination sedment (BX). */
    pop bx                  /* BX = destination segment */
    mov es, bx              /* ES = destination segment */
    xor bp, bp              /* BP = offset 0 */

	/* Read. */
    mov ah, 0x06            /* READ DATA */
    mov al, [BOOT_DAUA]
    mov bx, 512             /* 転送バイト数 */
    int 0x1b
	jc read_error

	pop es
    popa
    ret
read_error:
	mov ax, 0xa000
	mov ds, ax
	xor di, di
	mov cl, 'E'
	mov [di], cl
error_spin:
	jmp error_spin

/*
 * 32-bit entry point. Jumps to ELF entry point.
 */
.code32
pm32_entry:
	/*
	 * Install flat data selectors.
	 */
	mov ax, 0x10
	mov ds, ax

	/*
	 * Multiboot info
	 *
	 * We only need multiboot_flags and multiboot_mem_upper.
	 * Other fields are garbage.
	 */
	mov eax, 0x2badb002
	mov ebx, multiboot_flags

	/*
	 * Jump to the kernel entry in 32-bit protected mode.
	 */
	jmp 0x08:0x100000

.code16

/*
 * GDT:
 *   0x00 null
 *   0x08 flat 32-bit code
 *   0x10 flat 32-bit data
 */
gdt:
	.quad 0

	/*
	 * 0x08: flat 32-bit code, base=0, limit=4GB.
	 */
	.word 0xffff    /* Limit 0:15 */
	.word 0x0000    /* Base 0:15 */
	.byte 0x00      /* Base 16:23 */
	.byte 0x9a      /* Access: Code, Execute/Read */
	.byte 0xcf      /* Flags(0xc0) + Limit 16:19(0x7) */
	.byte 0x00      /* Base 24:31 -> 0x00000000 */

	/*
	* 0x10: flat 32-bit data, base=0, limit=4GB.
	*/
	.word 0xffff    /* Limit 0:15 */
	.word 0x0000    /* Base 0:15 */
	.byte 0x00      /* Base 16:23 */
	.byte 0x92      /* Access: Data, Read/Write */
	.byte 0xcf      /* Flags(0xc0) + Limit 16:19(0x7) */
	.byte 0x00      /* Base 24:31 -> 0x00000000 */
gdt_end:

/*
 * GDTR
 */
gdtr:
	.word gdt_end - gdt - 1
	.long gdt + BOOT_BASE

.org 510
	.byte 0xaa
	.byte 0x55
	
