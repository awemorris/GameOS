/*
 * elfboot
 * (C) Copyright 2005, 2026, Awe Morris.
 */

/*
 * A DOS program that loads ELF kernel and jumps to it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dos.h>
#include <i86.h>
#include <conio.h>

#undef DEBUG

/*
 * Architecture selection. Default to PC/AT.
 */
#if !defined(ARCH_PCAT) && !defined(ARCH_PC98)
#define ARCH_PCAT
#endif

/* Version banner. */
static const char msg_version[] =
	"\n"
	"bootelf: Suika3 Game OS bootloader for DOS\n"
	"Copyright (C) 2005, 2026, Awe Morris.\n\n";

/*
 * Constants
 */

/* Page size used to align each segment in memory. */
#define PAGE_SIZE	(4096L)

/* ELF header field values we accept. */
#define ET_EXEC		(2)	/* e_type    : executable file */
#define EM_386		(3)	/* e_machine : Intel 80386     */
#define PT_LOAD		(1)	/* p_type    : loadable segment */

/* Staging buffer size for one transfer (kept small for real mode). */
#define SEG_BUF_SIZE	(4096)

/*
 * Types
 */

typedef unsigned long u32;
typedef unsigned short u16;
typedef unsigned char u8;

/* ELF file header. */
struct elf_header {
	u8 ident[16];
	u16 type;
	u16 machine;
	u32 version;
	u32 entry;
	u32 prog_header_offset;
	u32 section_header_offset;
	u32 flags;
	u16 elf_header_size;
	u16 prog_header_entry_size;
	u16 prog_header_entries;
	u16 section_header_entry_size;
	u16 section_header_entries;
	u16 shstrndx;
} ELF_HEADER;

/* ELF program (segment) header. */
struct program_header {
	u32	type;
	u32	offset;
	u32	vaddr;
	u32	paddr;
	u32	size_on_file;
	u32	size_on_memory;
	u32	flags;
	u32	align;
};

/* Multiboot Infomation */
struct multiboot_info {
	u32	flags;
	u32	mem_lower;
	u32	mem_upper;
	u32	boot_device;
	u32	cmdline;
	u32	mods_count;
	u32	mods_addr;

	struct elf_section_header_table {
		u32 num;
		u32 size;
		u32 addr;
		u32 shndx;
	} shtab;

	u32	mmap_length;
	u32	mmap_addr;
} multiboot_info;

/*
 * GDT
 */

static unsigned char gdt[8*3] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0xff, 0xff, 0, 0, 0, 0x9a, 0xcf, 0,	/* 0x0008: 32-bit flat code. */
	0xff, 0xff, 0, 0, 0, 0x92, 0xcf, 0,	/* 0x0010: 32-bit flat data. */
};

static unsigned char gdtr[6];

/*
 * ELF loading
 */

/* Staging buffer for one segment transfer. */
static u8 seg_buf[SEG_BUF_SIZE];

/* Physical address where the next ELF segment will be placed. */
static u32 cur_load_addr = 0x100000L;

/* Current file and the headers read from it. */
static FILE *fp;
static struct elf_header elf_header;
static struct program_header code_header;
static struct program_header data_header;

/* Error messages. */
static char msg_err_fopen[] = " Can't open file.\n";
static char msg_err_fread[] = " File reading error.\n";
static char msg_err_elf[]   = " Invalid ELF file.\n";

/*
 * Forward declarations
 */

/* Top level. */
static int load_main(const char *fname);
static void show_hma_top(void);
static void boot_kernel(void);

/* ELF loading. */
static int load_elf_file(const char *fname);
static int read_elf_header(void);
static int read_segment_header(int index, struct program_header *header);
static int load_segment(struct program_header *header);
static void print_segment_info(void);

/* Low-level CPU / memory helpers. */
static unsigned short get_cs_reg(void);
static unsigned short get_ds_reg(void);
static unsigned short get_es_reg(void);
static unsigned short get_ss_reg(void);
static void lgdt(void);
static void enable_a20(void);
static void disable_a20(void);
static void linear_memcpy(u32 src_linear_addr, u32 dst_linear_addr, u32 bytes);
static void dump_hma(void);

/*
 * Entry point.
 */
int main(int argc, char *argv[])
{
	/* Check the argument count. */
	if(argc < 2) {
		printf("Specify the kernel file.\n");
		return 1;
	}

	/* Show the version banner. */
	printf(msg_version);

	/* Build the 6-byte GDTR image (limit + 32-bit linear base). */
	*(unsigned short *)(&gdtr[0]) = sizeof(gdt) - 1;
	*(unsigned long *)(&gdtr[2]) = ((u32)FP_SEG((void far *)gdt) << 4) + FP_OFF((void far *)gdt);

	/* Load the files. */
	if(load_main(argv[1]) != 0)
		return 1;

	/* Dump the head of the HMA (debug). */
#ifdef DEBUG
	show_hma_top();
#endif

	/* Boot the kernel. */
	boot_kernel();

	/* Never come here. */
	return 0;
}

/* Load every module given on the command line. */
static int
load_main(
	const char *fname)
{
	int i;

	printf("Loading kernel \"%s\"...\n", fname);

	/* Load the ELF file. */
	if(load_elf_file(fname) != 0) {
		/* failure */
		printf("Error.\n", fname);
		return -1;
	}

	printf("\nLoaded successfully.\n", fname);

	/* success */
	return 0;
}

/* Load an ELF file into high memory and fill in the module information. */
static int
load_elf_file(
	const char *fname)
{
	int success;

	/* Open the file. */
	fp = fopen(fname, "rb");
	if(fp == NULL) {
		printf(msg_err_fopen);
		return -1;
	}

	/* Read and load the ELF file. */
	success = 0;
	do {
		/* Read the ELF header. */
		if(read_elf_header() != 0)
			break;	/* error */

		/* Load the code segment. */
		if(read_segment_header(0, &code_header) != 0)
			break;	/* error */
		if(load_segment(&code_header) != 0)
			break;	/* error */

		/* Load the data segment. */
		if(read_segment_header(1, &data_header) != 0)
			break;	/* error */
		if(load_segment(&data_header) != 0)
			break;	/* error */

		/* success */
		success = 1;
	} while(0);

	/* Close the file. */
	fclose(fp);

	/* On failure. */
	if(!success)
		return -1;	/* error */

	/* Print the segment information. */
	print_segment_info();

	/* success */
	return 0;
}

/* Read and validate the ELF header. */
static int
read_elf_header(void)
{
	int success;

	/* Read from the file. */
	if(fread(&elf_header, sizeof(ELF_HEADER), 1, fp) < 1) {
		printf(msg_err_fread);
		return -1;	/* error */
	}

	/* Check that the file format is valid. */
	success = 0;
	do {
		/* Expected first 16 bytes of an i386 ELF executable. */
		u8 magic[16] = {0x7f, 'E', 'L', 'F', 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

		/* Check the header fields. */
		if(memcmp(&elf_header.ident, magic, sizeof(magic)) != 0)
			break;	/* error */
		if(elf_header.type != ET_EXEC)
			break;	/* error */
		if(elf_header.machine != EM_386)
			break;	/* error */
		if(elf_header.prog_header_entries < 2)
			break;	/* error */

		/* success */
		success = 1;
	} while(0);
	if(!success) {
		printf(msg_err_elf);
		return -1;	/* error */
	}

	/* success */
	return 0;
}

/* Read the program header of the given segment. */
static int
read_segment_header(
	int index,
	struct program_header *header)
{
	u32 offset = elf_header.prog_header_offset
		+ elf_header.prog_header_entry_size * index;

	/* Seek and read from the file. */
	if(fseek(fp, offset, SEEK_SET) != 0) {
		printf(msg_err_fread);
		return -1;	/* error */
	}
	if(fread(header, sizeof(struct program_header), 1, fp) < 1) {
		printf(msg_err_fread);
		return -1;	/* error */
	}

	/* Check that the segment type is valid. */
	if(header->type != PT_LOAD) {
		printf(msg_err_elf);
		return -1;	/* error */
	}

#ifdef DEBUG
	/* Segment info dump (kept for debugging). */
	printf(" [segment %d]\n", index);
	printf("  vaddr=%08lXH\n", header->vaddr);
	printf("  foffs=%08lXH\n", header->offset);
	printf("  fsize=%08lXH\n", header->size_on_file);
	printf("  msize=%08lXH\n", header->size_on_memory);
	printf("  align=%08lXH\n", header->align);
#endif

	/* success */
	return 0;
}

/* Load one segment into high memory. */
static int
load_segment(
	struct program_header *header)
{
	u32 src_addr;
	u32 fsize;
	u32 msize;
	u32 pad;

	src_addr = ((u32)FP_SEG((void far *)seg_buf) << 4) + (u32)FP_OFF((void * far)seg_buf);
	fsize = header->size_on_file;
	msize = header->size_on_memory;
	pad = msize - fsize;

	/* Round the in-memory size up to a page boundary. */
	if(msize % PAGE_SIZE != 0) {
		msize += PAGE_SIZE - (msize % PAGE_SIZE);
		pad = msize - fsize;
	}

	/* Record the physical placement and the padded size. */
	header->paddr = cur_load_addr;
	header->size_on_memory = msize;

	/* Seek to the start of the segment in the file. */
	if(fseek(fp, header->offset, SEEK_SET) != 0) {
		printf(msg_err_fread);
		return -1;	/* error */
	}

	/* Read from the file and move each chunk into high memory. */
	while(fsize > 0) {
		u32 read_size;

		/* Read one chunk. */
		read_size = (fsize >= SEG_BUF_SIZE) ? SEG_BUF_SIZE : fsize;
		if(fread(seg_buf, read_size, 1, fp) < 1) {
			printf(msg_err_fread);
			return -1;	/* error */
		}
		if(read_size != SEG_BUF_SIZE)
			memset(seg_buf + read_size, 0, SEG_BUF_SIZE - read_size);

		/* Copy the chunk to high memory. */
		linear_memcpy(src_addr, cur_load_addr, read_size);

		/* Advance. */
		cur_load_addr += read_size, fsize -= read_size;
	}

	/* Zero-fill the remaining (.bss) padding in high memory. */
	while(pad > 0) {
		u32 transfer_size;

		transfer_size = (pad >= SEG_BUF_SIZE) ? SEG_BUF_SIZE : pad;
		memset(seg_buf, 0, transfer_size);

		linear_memcpy(src_addr, cur_load_addr, transfer_size);

		cur_load_addr += transfer_size, pad -= transfer_size;
	}

	/* success */
	return 0;
}

/* Copy up to 65536 bytes between two 32-bit linear addresses. */
static void
linear_memcpy(
	u32 src_linear_addr,
	u32 dst_linear_addr,
	u32 bytes)
{
	u32 src, dst, len;
	u16 gdtr_addr;

	src = src_linear_addr;
	dst = dst_linear_addr;
	len = bytes;
	gdtr_addr = FP_OFF((void far *)gdtr);

	/* Enable A20 so addresses at/above 1MB are reachable. */
	enable_a20();

	/*
	 * - Disable interrupts.
	 * - Enter protected mode.
	 * - Transfer (DS:[SI] --> ES:[DI]).
	 * - Return to real mode and re-enable interrupts.
	 */
	__asm {
		/* Prologue. */
		cli
		pushf
		push ds
		push ax
		push cx
		push si
		push di

		/*
		 * Load GDT.
		 *  - Load every call because DOS calls may also load GDT.
		 *  - Assume the small memory model (gdtr is in the same segment.
		 */

		mov bx, gdtr_addr

		/* lgdt [bx] */
		db 0fh
		db 01h
		db 17h

		/*
		 * Enable protected mode.
		 */
		mov eax, cr0
		or al, 1
		mov cr0, eax
		jmp flush_pipeline_prot
	    flush_pipeline_prot:

		/*
		 * Copy low memory to high memory.
		 *  - Note: Instruction code of movsb with 32-bit addressing
		 *    in 16-bit mode is a bit confusing, so we used mov here.
		 */
		mov ax, 0x10	/* 32-bit flat data segment selector */
		mov ds, ax
		mov esi, src	/* src */
		mov edi, dst	/* dst */
		mov ecx, len	/* len */
	    copy_loop:
		mov al, [esi]
		mov [edi], al
		inc esi
		inc edi
		dec ecx
		cmp ecx, 0
		jnz copy_loop

		/*
		 * Leave protected mode.
		 */
		mov eax, cr0
		and al, 0xfe
		mov cr0, eax
		jmp flush_pipeline_real
	    flush_pipeline_real:

		/* Epilogue. */
		pop di
		pop si
		pop cx
		pop ax
		pop ds
		popf
		sti
	}

	/* Disable A20 again. */
	disable_a20();
}

/* Print the loaded segment information. */
static void
print_segment_info(void)
{
	printf("\n");
	printf(" [Code Segment Info]\n");
	printf("  physical addr:   %08lXH\n", code_header.paddr);
	printf("  virtual addr:    %08lXH\n", code_header.vaddr);
	printf("  size on file:    %08lXH\n", code_header.size_on_file);
	printf("  size on memory:  %08lXH\n", code_header.size_on_memory);
	printf("\n");
	printf(" [Data Segment Info]\n");
	printf("  physical addr:   %08lXH\n", data_header.paddr);
	printf("  virtual addr:    %08lXH\n", data_header.vaddr);
	printf("  size on file:    %08lXH\n", data_header.size_on_file);
	printf("  size on memory:  %08lXH\n", data_header.size_on_memory);
	printf("\n");
}

/* Show the head of the HMA (debug helper). */
static void
show_hma_top(void)
{
	enable_a20();

	dump_hma();

	disable_a20();
}

/* Jump to the kernel entry point. */
static void
boot_kernel(void)
{
	u32 entry = ((u32)FP_SEG((void far *)&multiboot_info) << 4)+
		    FP_OFF((void far *)&multiboot_info);

	/* Ask before jumping to the kernel. */
	printf("Press <Enter> to boot the kernel...");
	getchar();

	/* Create a smallest multiboot_info. */
	memset(&multiboot_info, 0, sizeof(multiboot_info));
	multiboot_info.flags = 1;		/* MBINFO_FLAG_MEMORY */
	multiboot_info.mem_lower = 640;		/* 640KB */
	multiboot_info.mem_upper = 31 * 1024;	/* 31MB */

	/* Jump to the kernel. */
	enable_a20();
	__asm {
		/* Set multiboot_info address. */
		mov ebx, entry

		cli
		mov eax, cr0
		or al, 1
		mov cr0, eax
		jmp flush_pipeline_prot_boot
	    flush_pipeline_prot_boot:

		/* Set DS. */
		mov ax, 0x10
		mov ds, ax
		mov es, ax
		mov fs, ax
		mov gs, ax
		mov ss, ax

		/* Clear eflags. */
		push 0x0002
		popf

		/* Multiboot magic */
		mov eax, 0x2badb002

		/* o32 far ret */
		mov edx, 0x08
		mov ecx, 0x100000
		push edx
		push ecx
		db 66h
		retf
	}
}

/* Dump the head of the HMA (physical 0x00100000). */
static void
dump_hma(void)
{
	char far *p = (char far *)0xFFFF0010;
	int i, j;
	for(i=0; i<8; i++) {
		printf("%08lX  ", 0x100000L + i*16);
		for(j=0; j<16; j++, p++) {
			char c = *p;
			printf("%02X ", c);
			if(j == 7)
				printf(" ");
		}
		printf("\n");
	}
	printf("\n");
}

/*
 * Low-level register accessors (OpenWatcom notation)
 */

/* Get the value of the CS register. */
#pragma aux get_cs_reg = \
	"mov ax, cs"	 \
	value [ax];

/* Get the value of the DS register. */
#pragma aux get_ds_reg = \
	"mov ax, ds"	 \
	value [ax];

/* Get the value of the ES register. */
#pragma aux get_es_reg = \
	"mov ax, es"	 \
	value [ax];

/* Get the value of the SS register. */
#pragma aux get_ss_reg = \
	"mov ax, ss"	 \
	value [ax];

/* Enable the A20 address line. */
static void
enable_a20(void)
{
	char far *lo = (char far *)0x00000000;
	char far *hi = (char far *)0xffff0010;
	char save;

#if defined(ARCH_PCAT)

	/* Send the command to the keyboard controller. */
	while(inp(0x64) & 0x02)
		;
	outp(0x64, 0xd1);
	while(inp(0x64) & 0x02)
		;
	outp(0x60, 0xdf);
	while(inp(0x64) & 0x02)
		;

#elif defined(ARCH_PC98)

	outp(0xf2, 0);

#endif

	/* Wait until A20 has actually taken effect. */
	save = *lo;
	while(1) {
		if(++(*lo) == *hi)
			continue;
		if(++(*lo) == *hi)
			continue;
		break;
	}
	*lo = save;
}

/* Disable the A20 address line. */
static void
disable_a20(void)
{
	char far *lo = (char far *)0x00000000;
	char far *hi = (char far *)0xffff0010;
	char save;

#if defined(ARCH_PCAT)

	/* Send the command to the keyboard controller. */
	while(inp(0x64) & 0x02)
		;
	outp(0x64, 0xd1);
	while(inp(0x64) & 0x02)
		;
	outp(0x60, 0xdd);
	while(inp(0x64) & 0x02)
		;

#elif defined(ARCH_PC98)

	/* TODO: the way to disable A20 on PC-98 is unknown. */
	return;

#endif

	/* Wait until A20 has actually been disabled. */
	save = *lo;
	while(1) {
		if(++(*lo) != *hi)
			continue;
		if(++(*lo) != *hi)
			continue;
		break;
	}
	*lo = save;
}
