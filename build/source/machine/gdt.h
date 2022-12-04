
#pragma once

/* x86/lowlevel/gdt.h - Global Descriptor Table
*
* 
*/

#include <common.h>

struct tss;

/*
* An entry in the GDT table. The layout of this structure is mandated by the CPU.
*/
struct gdt_entry
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t flags_and_limit_high;
	uint8_t base_high;
	
} __attribute__((packed));


/*
* Describes the GDT address and size. We use the address of this structure
* to tell the CPU where the GDT is. The layout of this structure is mandated by the CPU.
*/
struct gdt_ptr
{
	uint16_t size;
	size_t location;
} __attribute__((packed));

void x86_gdt_initialise(void);
uint16_t x86_gdt_add_tss(struct tss* tss);