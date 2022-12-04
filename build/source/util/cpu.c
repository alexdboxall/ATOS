#include <cpu.h>
#include <arch.h>

static int cpu_count = 0;

/*
* The virtual memory manager (arch_virt_init) is responsible for initialising this.
*/ 
struct cpu* current_cpu;

int cpu_get_count(void) {
    return cpu_count;
}

void cpu_init(void) {
    arch_cpu_initialise_bootstrap();
    cpu_count++;

    // TODO: other CPUs
    // arch_next_cpu_initialise();
    // cpu_count++;
	
    arch_all_cpus_are_done();
}