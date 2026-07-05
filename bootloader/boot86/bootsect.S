/*
 * bootsect.S
 *
 * PC/AT boot sector with tiny ELF32 PT_LOAD loader.
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

.equ boot_drive,   VAR_BASE + 0x00
.equ copy_len,     VAR_BASE + 0x02

.equ dst_linear,   VAR_BASE + 0x04
.equ remain,       VAR_BASE + 0x08
.equ file_off,     VAR_BASE + 0x0c

.equ elf_entry,    VAR_BASE + 0x10


_start:
	/*
	 * Establish a minimal real-mode environment.
	 *
	 * DS=0 is required because variables and descriptors are accessed
	 * through absolute physical addresses.
	 */
	cli
	cld			/* lodsb in copy_secbuf_to_high needs DF=0;
				   BIOS does not guarantee it */
	xor ax, ax
	mov ds, ax
	mov ss, ax
	mov sp, 0x7c00

	/*
	 * Save the PC/AT BIOS boot drive.
	 *
	 * For HDD boot this is usually 0x80.
	 */
	mov [boot_drive], dl

	/*
	 * Prepare GS as an unreal-mode flat 4GB data segment.
	 *
	 * We stay in real mode for BIOS calls, but use GS to write above 1MB.
	 */
	call setup_unreal_gs

	/*
	 * ES points to the header buffer.
	 *
	 * The ELF header and PHDRs remain in HDRBUF for the whole load.
	 */
	mov ax, HDRBUF_SEG
	mov es, ax

	/*
	 * Read LBA 2 only once.
	 *
	 * This sector contains the ELF header and, by assumption, all PHDRs.
	 */
	mov ax, KERNEL_LBA
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
	lgdt [gdtr]

	/*
	* Set CR0.PE.
	*/
	mov eax, cr0
	or al, 1
	mov cr0, eax

	/*
	* Reload CS with the 32-bit code selector.
	*/
	jmp 0x08:pm32_entry

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
	push ds

	/*
	* Source is always the beginning of SECBUF.
	*/
	xor si, si

	/*
	* Destination is the current high-memory load address.
	*/
	mov edi, dword ptr [dst_linear]

	/*
	* Byte count is at most 512.
	*/
	mov cx, [copy_len]

	/*
	* DS:SI points to the sector buffer.
	*/
	mov ax, SECBUF_SEG
	mov ds, ax

1:
	/*
	* Byte-copy from low memory to high memory.
	*/
	lodsb
	mov byte ptr gs:[edi], al
	inc edi
	loop 1b

	/*
	* Restore DS=0.
	*/
	pop ds
	ret

/*
 * read_lba_buf
 *
 * Input:
 *   AX = 16-bit LBA
 *   BX = destination segment
 *
 * Reads one 512-byte sector to BX:0000 using INT 13h Extensions AH=42h.
 *
 * ES is preserved because the PHDR loop keeps ES pointing at HDRBUF.
 */
read_lba_buf:
	/*
	* Patch the DAP with the requested LBA and destination segment.
	*/
	mov [dap_lba], ax
	mov [dap_seg], bx

	/*
	* BIOS extended read.
	*
	*   AH    = 42h
	*   DL    = boot drive
	*   DS:SI = DAP
	*/
	mov ah, 0x42
	mov dl, [boot_drive]
	mov si, offset dap

	/*
	* Keep ES stable for PHDR access.
	*/
	push es
	int 0x13
	pop es

	jc fatal
	ret
fatal:
	/*
	* Minimal fatal path.
	*/
	jmp fatal

/*
 * setup_unreal_gs
 *
 * After return:
 *   GS hidden base  = 0
 *   GS hidden limit = 4GB
 *
 * This compact version briefly enters protected mode, loads GS with the
 * flat data selector, clears PE, and continues in real mode.
 */
setup_unreal_gs:
	cli

	/*
	* Load GDT containing flat code/data descriptors.
	*/
	lgdt [gdtr]

	/*
	* Enter protected mode temporarily.
	*/
	mov eax, cr0
	or al, 1
	mov cr0, eax

	/*
	* Load GS with the flat data selector.
	*/
	mov ax, 0x10
	mov gs, ax

	/*
	* Return to real mode while keeping GS's hidden descriptor cache.
	*/
	mov eax, cr0
	and al, 0xfe
	mov cr0, eax

	sti
	ret

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
	mov es, ax
	mov ss, ax

	/*
	* Temporary protected-mode stack below VGA memory.
	*/
	mov esp, 0x0009f000

	/*
	* Convert high-half ELF entry VMA to physical address.
	*
	* Example:
	*   0x80101234 -> 0x00101234
	*/
	mov eax, dword ptr [elf_entry]
	btr eax, 31

	/*
	* Jump to the kernel entry in 32-bit protected mode.
	*/
	jmp eax

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
	.word 0xffff
	.word 0x0000
	.byte 0x00
	.byte 0x9a
	.byte 0xcf
	.byte 0x00

	/*
	* 0x10: flat 32-bit data, base=0, limit=4GB.
	*/
	.word 0xffff
	.word 0x0000
	.byte 0x00
	.byte 0x92
	.byte 0xcf
	.byte 0x00
gdt_end:

/*
 * GDTR
 */
gdtr:
	.word gdt_end - gdt - 1
	.long gdt

/*
 * INT 13h Extensions Disk Address Packet.
 *
 * Destination segment is patched at runtime:
 *   HDRBUF_SEG for the ELF header sector
 *   SECBUF_SEG for PT_LOAD data sectors
 */
dap:
	.byte 0x10
	.byte 0x00
	.word 1
	.word 0x0000
dap_seg:
	.word 0x0000
dap_lba:
	.quad 0

/*
 * MBR partition table.
 *
 * Currently all entries are empty.
 */
.org 0x1be
mbr:
	/* Entry 0 */
	.long 0
	.long 0
	.long 0
	.long 0

	/* Entry 1 */
	.long 0
	.long 0
	.long 0
	.long 0

	/* Entry 2 */
	.long 0
	.long 0
	.long 0
	.long 0

	/* Entry 3 */
	.long 0
	.long 0
	.long 0
	.long 0

/*
 * MBR signature.
 */
.org 510
mbr_signature:
	.word 0xaa55
