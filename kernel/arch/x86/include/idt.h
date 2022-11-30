
#pragma once

#include <common.h>

/* x86/lowlevel/idt.h - Interrupt Descriptor Table
*
* 
*/

/*
* An entry in the IDT. The offset is the address the CPU will jump to,
* and the selector is what segment should be used (i.e. we need to have
* setup a GDT already). The layout of this structure is mandated by the CPU.
*/
struct idt_entry
{
	uint16_t isr_offset_low;
	uint16_t segment_selector;
	uint8_t reserved;
	uint8_t type;
	uint16_t isr_offset_high;

} __attribute__((packed));

/*
* Used to tell the CPU where the IDT is and how long it is.
* The layout of this structure is mandated by the CPU.
*/
struct idt_ptr
{
	uint16_t size;
	size_t location;
} __attribute__((packed));


void x86_idt_initialise(void);
