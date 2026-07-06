ENTRY(text_start)

SECTIONS
{
	. = 0x80100000;

	_text_start = (. + 3) & 0xfffffffc;
	.text ALIGN(4) : AT(0x100000)
	{
		gravity.elf.dir/sys/arch/x86/locore.S.o:(.text)
		*(.text)
	}
	_text_end = (. + 3) & 0xfffffffc;
	_text_size = _text_end - _text_start;

	_data_start = (. + 3) & 0xfffffffc;
	.data ALIGN(4096) : AT(0x100000 + _text_size)
	{
		*(.*data)
                *(.rodata.*)
	}
	_data_end = (. + 3) & 0xfffffffc;
	_data_size = _data_end - _data_start;

	_bss_start = (. + 3) & 0xfffffffc;
	.bss ALIGN(4096) : AT(0x100000 + _text_size + _data_size)
	{
		*(.bss) *(.*comm) *(COMMON)
	}
	_bss_end = (. + 3) & 0xfffffffc;
	_bss_size = _data_end - _data_start;

        .meta ALIGN(4096) : AT(0x100000 + _text_size + _data_size + _bss_size)
    	{
		*(.note.gnu.build-id)
	}
}
