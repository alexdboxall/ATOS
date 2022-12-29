ENTRY(_start)
OUTPUT_FORMAT("elf32-i386")

SECTIONS
{
	. = 1M;

	_kernel_start = .;
	.multiboot.data : {
		*(.multiboot.data)
	}
	
	.multiboot.text : {
		*(.multiboot.text)
	
	}
	
	. += 0xC0000000;
	
	.text ALIGN(4096) : AT (ADDR (.text) - 0xC0000000)
	{
		*(.text)
		*(.ctors)
		*(.dtors)
	}
	
	.rodata ALIGN(4096) : AT (ADDR (.rodata) - 0xC0000000)
	{
		*(.rodata)
	}

	.data ALIGN(4096) : AT (ADDR (.data) - 0xC0000000)
	{
		*(.data)
	}
	
	.bss ALIGN(4096) : AT (ADDR (.bss) - 0xC0000000)
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}
	
	_kernel_end = .;
	
	/DISCARD/ :
    {
        *(.comment)
    }
}