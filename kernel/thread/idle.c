
#include <thread.h>
#include <arch.h>
#include <assert.h>
#include <cpu.h>
#include <kprintf.h>

/*
* thread/idle.c - Idle Thread
*
* The idle thread is only ran if there are no other threads which can.
* This ensure that there is always something to run.
*/


/*
* The thread ran when there is nothing else to run. Must never block.
*/
static void idle_function(void* arg) {
    (void) arg;

    /*
    * Keep the CPU into a low-power mode until we are preempted.
    */
    while (1) {
        arch_stall_processor();
    }
}

/*
* Starts the idle thread. Must be called per-CPU so every CPU can run an idle thread.
*/
void idle_thread_init() {
    struct thread* idle_thread = thread_create(idle_function, NULL, current_cpu->current_vas);
    idle_thread->priority = PRIORITY_IDLE;
}