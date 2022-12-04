
#include <thread.h>
#include <cpu.h>
#include <spinlock.h>
#include <kprintf.h>

void test_stack_canary(void) {
	/*
	* The canary is only checked on task switches.
	*/
	spinlock_acquire(&scheduler_lock);
	thread_schedule();
	spinlock_release(&scheduler_lock);

	/*
	* Die violently with infinite recursion.
	*/
	test_stack_canary();

	/*
	* To prevent tail-call optimisation.
	*/
	kprintf("kaboom!\n");
}