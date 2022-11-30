#pragma once

struct virtual_address_space;
struct thread;

struct cpu {
    /*
    * These must be the first three entries in the struct, as they may be used
    * by assembly code for thread switching. 
    */ 
    struct thread* current_thread;                  /* offset 0                     */
    struct virtual_address_space* current_vas;      /* offset sizeof(size_t)        */
    void* platform_specific_data;                   /* offset sizeof(size_t) * 2    */

    /*
    * From now on, order doesn't matter.
    */
    int cpu_number;

};

/*
* We must never use the address of current_cpu as identification of which
* CPU we are looking at (e.g. for storing the owner of a spinlock), as they
* all share the same virtual address.
*
* Instead use cpu_number for identification. 
*/
extern struct cpu* current_cpu;


/*
* We initialise CPUs one at a time. This variable keeps track of how many have
* completed initialisation so far. Therefore it can be used to work out the
* current CPU number when bootstrapping a new CPU (we guarantee only one CPU
* can bootstrap at any given time).
*/
int cpu_get_count(void);

/*
* Initialises all CPUs on the system.
*/
void cpu_init(void);