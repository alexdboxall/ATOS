#pragma once

/*
* x86/mem/virtual.h - Virtual Memory for x86
* 
* 
* Implements virtual memory management for x86 computers.
* 
*/

#include <common.h>
#include <virtual.h>
#include <machine/config.h>
#include <machine/interrupt.h>

int x86_handle_page_fault(struct x86_regs* addr);

size_t lowmem_physical_to_virtual(size_t physical);
void x86_per_cpu_virt_initialise(void);