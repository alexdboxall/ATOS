
#include <common.h>
#include <cpu.h>
#include <machine/cpu.h>

/*
* x86/lowlevel/idt.c - Interrupt Descriptor Table
*
* The interrupt decriptor table (IDT) is essentially a lookup table for where
* the CPU should jump to when an interrupt is receieved.
*/

extern void x86_idt_load(size_t addr);

/*
* Our trap handlers, defined in lowlevel/trap.s, which will be called
* when an interrupt occurs. 
*/
extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr128();
extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

/*
* Fill in an entry in the IDT. There are a number of 'types' of interrupt, determining
* whether interrupts are disabled automatically before calling the handler, whether
* it is a 32-bit or 16-bit entry, and whether user mode can invoke the interrupt manually.
*/
void x86_idt_set_entry(int num, size_t isr_addr, uint8_t type)
{
	struct x86_cpu_specific_data* cpu_data = current_cpu->platform_specific_data;

	cpu_data->idt[num].isr_offset_low = (isr_addr & 0xFFFF);
	cpu_data->idt[num].isr_offset_high = (isr_addr >> 16) & 0xFFFF;
	cpu_data->idt[num].segment_selector = 0x08;
	cpu_data->idt[num].reserved = 0;
	cpu_data->idt[num].type = type;
}

/*
* Initialise the IDT. After this has occured, interrupts may be enabled.
*/
void x86_idt_initialise(void)
{
	struct x86_cpu_specific_data* cpu_data = current_cpu->platform_specific_data;

	void (*const isrs[])() = {
		isr0 , isr1 , isr2 , isr3 , isr4 , isr5 , isr6 , isr7 ,
		isr8 , isr9 , isr10, isr11, isr12, isr13, isr14, isr15,
		isr16, isr17, isr18, isr19, isr20, isr21,
	};

	void (*const irqs[])() = {
		irq0 , irq1 , irq2 , irq3 , irq4 , irq5 , irq6 , irq7 ,
		irq8 , irq9 , irq10, irq11, irq12, irq13, irq14, irq15
	};

	/*
	* Install handlers for CPU exceptions (e.g. for page faults, divide-by-zero, etc.).
	*/
	for (int i = 0; i < 21; ++i) {
		x86_idt_set_entry(i, (size_t) isrs[i], 0x8E);
	}
	
	/*
	* Install handlers for IRQs (hardware interrupts, e.g. keyboard, system timer).
	*/
	for (int i = 0; i < 16; ++i) {
		x86_idt_set_entry(i + 32, (size_t) irqs[i], 0x8E);
	}

	/*
	* Install our system call handler. Note that the flag byte is 0xEE instead of 0x8E,
	* this allows user code to directly invoke this interrupt.
	*/
	x86_idt_set_entry(128, (size_t) isr128, 0xEE);
	
	cpu_data->idtr.location = (size_t) &cpu_data->idt;
	cpu_data->idtr.size = sizeof(cpu_data->idt) - 1;
	
	x86_idt_load((size_t) &cpu_data->idtr);
}