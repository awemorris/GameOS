/*
 * PC-98 512-byte boot sector, GNU as, i386-elf-gcc -m32.
 * Assumptions:
 *   - BIOS loads this sector at 0000:7c00 and passes PC-98 DA/UA in AL.
 *   - 512-byte sectors, CHS geometry below.
 *   - Loads BOOTLOADER_SECTORS sectors starting at LBA 1 to 0800:0000.
 */

.intel_syntax noprefix

.code16

.global _start

.equ LOAD_SEG, 0x0800
.equ BOOTLOADER_SECTORS, 31
.equ SECTORS_PER_TRACK, 17
.equ HEADS, 2
.equ PC98_READ_AH, 0x56       /* INT 1Bh READ DATA, seek+MFM, no multi-track */

_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov [boot_daua], al

    /* -----------------------------------------------------------------
     * Read the second-stage bootloader.
     *
     * NEW LAYOUT:
     *   LBA 0        : this IPL
     *   LBA 1        : partition table (reserved, not read here)
     *   LBA 2..1+N   : second-stage bootloader
     *
     * We start reading from LBA 2, skipping the partition table.
     * ----------------------------------------------------------------- */
    mov ax, LOAD_SEG
    mov es, ax
    xor bp, bp
    mov si, 2                 /* first sector after boot sector */
    mov cx, BOOTLOADER_SECTORS

load_loop:
    push cx
    push si
    call read_lba_512_esbp
    jc disk_error
    add bp, 512
    pop si
    inc si
    pop cx
    loop load_loop

    mov al, [boot_daua]
    jmp LOAD_SEG:0x0000

disk_error:
    hlt
    jmp disk_error

/* in: SI=LBA, ES:BP=buffer. out: CF from BIOS. destroys AX,BX,CX,DX */
read_lba_512_esbp:
    push si
    xor dx, dx
    mov ax, si
    mov bx, SECTORS_PER_TRACK
    div bx                    /* AX = lba/spt, DX = sector-1 */
    mov dl, dl
    inc dl                    /* DL = sector, 1-origin */
    xor dx, dx                /* Oops: recompute carefully below */
    pop si

    mov ax, si
    xor dx, dx
    mov bx, SECTORS_PER_TRACK
    div bx                    /* AX=q, DX=r */
    mov [sec_no], dl
    inc byte ptr [sec_no]      /* sector, 1-origin */
    xor dx, dx
    mov bx, HEADS
    div bx                    /* AX=cyl, DX=head */
    mov cl, al                /* cylinder low byte; simple IPL: <256 cyl */
    mov dh, dl                /* head */
    mov dl, [sec_no]          /* sector */

    mov ah, PC98_READ_AH
    mov al, [boot_daua]
    mov bx, 512               /* DTL; for fixed disk BIOS this may be 256*n on old SASI */
    mov ch, 2                 /* sector length code: 512 bytes for FD BIOS */
    int 0x1b
    ret

boot_daua:
	.byte 0

sec_no:
	.byte 0

/*
 * MBR partition table (for Windows compatibility)
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
 * MBR signature (for Windows compatibility)
 */
.org 510
.word 0xaa55
