
#include <common.h>
#include <cpu.h>
#include <machine/cpu.h>
#include <machine/gdt.h>
#include <machine/tss.h>

/*
* x86/lowlevel/gdt.c - Global Descriptor Table
*
* The global descriptor table (GDT) is a structure used for segmenation.
* It contains a number of entries, containing the address, size and flags
* for each segment. We can then load these offsets into the table (called
* descriptiors) in to the segment registers CS, DS, ES, FS, GS and SS.
*
* Although we don't want to use segmentation, it is actually required.
* Therefore, we set up a few segments that over the entire address space
* from 0x00000000 to 0xFFFFFFFF so that segmentation can be effectively
* ignored.
*
*/


/*
* We need assembly to tell the CPU where the GDT is.
* Defined in x86/lowlevel/gdt.s
*/
extern void x86_gdt_load(size_t addr);

/*
* Returns an entry that can be placed into the GDT.
*/
struct gdt_entry x86_gdt_create_entry(size_t base, size_t limit, uint8_t access, uint8_t granularity)
{
	struct gdt_entry entry;

	entry.base_low = base & 0xFFFF;
	entry.base_middle = (base >> 16) & 0xFF;
	entry.base_high = (base >> 24) & 0xFF;
	entry.limit_low = limit & 0xFFFF;
	entry.flags_and_limit_high = (limit >> 16) & 0xF;
	entry.flags_and_limit_high |= (granularity & 0xF) << 4;
	entry.access = access;

	return entry;
}

/*
* Initialises the GDT and loads it. 
*/
void x86_gdt_initialise(void)
{
	struct x86_cpu_specific_data* cpu_data = current_cpu->platform_specific_data;

	cpu_data->gdt[0] = x86_gdt_create_entry(0, 0, 0, 0);					// null segment
	cpu_data->gdt[1] = x86_gdt_create_entry(0, 0xFFFFFFFF, 0x9A, 0xC);		// kernel code
	cpu_data->gdt[2] = x86_gdt_create_entry(0, 0xFFFFFFFF, 0x92, 0xC);		// kernel data
	cpu_data->gdt[3] = x86_gdt_create_entry(0, 0xFFFFFFFF, 0xFA, 0xC);		// user code
	cpu_data->gdt[4] = x86_gdt_create_entry(0, 0xFFFFFFFF, 0xF2, 0xC);		// user data

	cpu_data->gdtr.size = sizeof(cpu_data->gdt) - 1;
	cpu_data->gdtr.location = (size_t) &cpu_data->gdt;

	x86_gdt_load((size_t) &cpu_data->gdtr);
}

/*
* Adds a Task State Segment (TSS) entry into the GDT. This allows the TSS to be
* used to switch from user mode to kernel mode.
*
* Returns the selector used in the GDT.
*/
uint16_t x86_gdt_add_tss(struct tss* tss)
{
	struct x86_cpu_specific_data* cpu_data = current_cpu->platform_specific_data;
	cpu_data->gdt[5] = x86_gdt_create_entry((size_t) tss, sizeof(struct tss), 0x89, 0x0);
	return 5 * 0x8;
}