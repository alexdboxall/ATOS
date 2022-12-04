#pragma once

#include <common.h>

struct x86_regs
{
	/*
	* The registers that are pushed to us in x86/lowlevel/trap.s
	* 
	* SS is the first thing pushed, and thus the last to be popped
	* GS is the last thing pushed, and thus the first to be popped
	*/
	size_t gs, fs, es, ds;
	size_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	size_t int_no, err_code;
	size_t eip, cs, eflags, useresp, ss;
};

void x86_interrupt_initialise(void);
int x86_register_interrupt_handler(int num, int(*handler)(struct x86_regs*));