
#include <spinlock.h>
#include <errno.h>
#include <panic.h>
#include <assert.h>
#include <arch.h>
#include <cpu.h>
#include <kprintf.h>
#include <string.h>

/*
* thread/spinlock.c - Spinlocks
*
* Locks to ensure that only one thread on one CPU is running a critical 
* section of code at any given time. They must be implemented atomically, 
* and thus the internal implementation is platform-specific.
*/

/*
* Checks whether a given lock is held.
*/
bool spinlock_is_held(struct spinlock* lock)
{
	assert(lock != NULL);
	return lock->lock;
}

/*
* Initialises a spinlock.
*/
void spinlock_init(struct spinlock* lock, const char* name)
{
	assert(lock != NULL);
	lock->lock = 0;
	lock->cpu_number = -1;

	/*
	* strncpy does not add a trailing zero if we exceed the length,
	* so stop one byte short and add it ourselves.
	*/
	strncpy(lock->name, name, 63);
	lock->name[63] = 0;
}

/*
* Acquires a spinlock. If the lock is already held, it will keep trying (spinning)
* until it can acquire the lock.
*/
void spinlock_acquire(struct spinlock* lock)
{
	assert_with_message(!spinlock_is_held(lock), lock->name);
	arch_irq_spinlock_acquire(&lock->lock);
}

/*
* Releases a spinlock which is currently held.
*/
void spinlock_release(struct spinlock* lock)
{	
	assert_with_message(spinlock_is_held(lock), lock->name);
	arch_irq_spinlock_release(&lock->lock);
}