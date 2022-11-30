#include <synch.h>
#include <thread.h>
#include <heap.h>
#include <assert.h>
#include <kprintf.h>
#include <cpu.h>
#include <panic.h>
#include <errno.h>
#include <spinlock.h>

/*
* thread/synch.c - Synchronisation
*
* Provides higher-level synchronisation primitives built on top of spinlocks.
* They interact with the scheduler so threads may block instead of spinning
* while waiting.
*/

/*
* Allocates and initialises a semaphore.
*/
struct semaphore* semaphore_create(int max_count) {
    assert(max_count >= 1);

    struct semaphore* sem = malloc(sizeof(struct semaphore));
    sem->current_count = 0;
    sem->max_count = max_count;
    sem->first_waiting_thread = NULL;
    sem->last_waiting_thread = NULL;
    
    return sem;
}

/*
* Cleans up and frees a semaphore.
*/
void semaphore_destory(struct semaphore* sem) {
    assert(sem);
    free(sem);
}

/*
* Acquires a semaphore (i.e. wait/V). 
* If the semaphore cannot be acquired, the thread will block until it can be acquired.
*/
void semaphore_acquire(struct semaphore* sem) {
    assert(sem);

    /*
    * All semaphores need to use the scheduler lock because they use the same next
    * pointer that the scheduler uses.
    * 
    * It would be more efficient, but more complicated, to use a seperate spinlock per semaphore.
    */
    spinlock_acquire(&scheduler_lock); 

    if (sem->current_count < sem->max_count) {
        /*
        * Available to acquire right now.
        */
        sem->current_count++;

    } else {
        thread_postpone_switches();

        /*
        * Add the current thread to the waiting list, then block.
        */
        current_cpu->current_thread->next = NULL;

        if (sem->first_waiting_thread == NULL) {
            sem->first_waiting_thread = current_cpu->current_thread;
        } else {
            sem->last_waiting_thread->next = current_cpu->current_thread;
        }

        sem->last_waiting_thread = current_cpu->current_thread;

        thread_block(THREAD_STATE_UNINTERRUPTIBLE);
        thread_end_postpone_switches();

    }

    spinlock_release(&scheduler_lock);
}


/*
* Tries to acquire a semaphore (i.e. wait/V). If the semaphore is available, it
* will be acquired. If not, EAGAIN is returned.
*/
int semaphore_try_acquire(struct semaphore* sem) {
    assert(sem);

    spinlock_acquire(&scheduler_lock); 

    if (sem->current_count < sem->max_count) {
        /*
        * Available to acquire right now.
        */
        sem->current_count++;
        spinlock_release(&scheduler_lock);
        return 0;

    } else {
        spinlock_release(&scheduler_lock);
        return EAGAIN;
    }
}


/*
* Releases a semaphore (i.e. signal/P).
* If there are any threads waiting on the semaphore, the first will acquire it.
*/
void semaphore_release(struct semaphore* sem) {
    assert(sem);

    spinlock_acquire(&scheduler_lock);
    thread_postpone_switches();

    if (sem->first_waiting_thread != NULL) {
        /*
        * Wake up the first waiting thread. No need to decrease the count,
        * as the thread being woken up would have acquired the semaphore.
        */
        struct thread* thread = sem->first_waiting_thread;
        sem->first_waiting_thread = thread->next;
        thread->next = NULL;
        thread_unblock(thread);

    } else {
        /*
        * No other threads are waiting.
        */
        sem->current_count--;
    }

    thread_end_postpone_switches();
    spinlock_release(&scheduler_lock);
}

/*
* Sets the semaphore's current count. Only to be used straight 
* after initialisation.
*/
void semaphore_set_count(struct semaphore* sem, int count) {
    assert(sem);
    assert(count >= 0);
    
    spinlock_acquire(&scheduler_lock);
    
    if (sem->current_count != 0) {
        panic("semaphore_set_count: current count != 0, only use set_count after init");
    }
    sem->current_count = count;

    spinlock_release(&scheduler_lock);
}





/*
* Readers-writer lock - allows multiple readers at the same time, but writing
* requires exclusive access (also disallowing reading at the same time).
*
* Based on the two-mutex algorithm here: https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
*
*/

struct rw_lock* rw_lock_create() {
    struct rw_lock* lock = malloc(sizeof(struct rw_lock));
    lock->counter = 0;
    lock->global = semaphore_create(1);
    lock->readers = semaphore_create(1);
    return lock;
}

void rw_lock_destroy(struct rw_lock* lock) {
    assert(lock);
    semaphore_destory(lock->global);
    semaphore_destory(lock->readers);
    free(lock);
}

void rw_lock_acquire_read(struct rw_lock* lock) {
    assert(lock);

    semaphore_acquire(lock->readers);
    
    lock->counter++;

    if (lock->counter == 1) {
        /*
        * The rw_lock is now in read mode. Writes will not be allowed
        * until the last reader has released the lock.
        */
        semaphore_acquire(lock->global);
    }

    semaphore_release(lock->readers);
}

void rw_lock_release_read(struct rw_lock* lock) {
    assert(lock);

    semaphore_acquire(lock->readers);
    
    lock->counter--;

    if (lock->counter == 0) {
        /*
        * The rw_lock is now available for either reading or writing.
        */
        semaphore_release(lock->global);
    }

    semaphore_release(lock->readers);
}

void rw_lock_acquire_write(struct rw_lock* lock) {
    /*
    * This will only lock when no readers have the lock.
    */
    semaphore_acquire(lock->global);
}

void rw_lock_release_write(struct rw_lock* lock) {
    semaphore_release(lock->global);
}