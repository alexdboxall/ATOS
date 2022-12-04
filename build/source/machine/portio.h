#pragma once

/*
* x86/portio.h - Port Input / Output
* 
* 
* On the x86, a lot of older hardware is accessed using IO ports. A port has an
* address from 0x0000 to 0xFFFF, and can be read from and written to using special
* instructions.
*
* We are going to inline these functions, as they are all single instructions.
*
*/

#include <common.h>

/*
* Writing to ports
*/
always_inline void outb(uint16_t port, uint8_t value)
{
	asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

always_inline void outw(uint16_t port, uint16_t value)
{
	asm volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

always_inline void outl(uint16_t port, uint32_t value)
{
	asm volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

/*
* Reading from ports
*/
always_inline uint8_t inb(uint16_t port)
{
	uint8_t value;
	asm volatile ("inb %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}

always_inline uint16_t inw(uint16_t port)
{
	uint16_t value;
	asm volatile ("inw %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}

always_inline uint32_t inl(uint16_t port)
{
	uint32_t value;
	asm volatile ("inl %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}