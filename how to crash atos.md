# How to Crash ATOS #

**Method 1: Acquire a lock twice** (deadlocks)

    spinlock_acquire(&vas->vas_lock);
    do_something_that_also_acquires_the_same_lock(vas);
    spinlock_release(&vas->vas_lock);

**Method 2: Allocate memory while the scheduler lock is held** (deadlocks on low memory, *cough cough* thread_fork *cough*)

    spinlock_acquire(&scheduler_lock);
    phys_allocate_page();
    spinlock_release(&scheduler_lock);

**Method 3: Access unlocked memory while the scheduler lock is held** (may deadlock on low memory)

    spinlock_acquire(&scheduler_lock);
    *some_unlocked_memory = 0;
    spinlock_release(&scheduler_lock);

**Method 4: Forget to lock a page table**
    Because the page directories use physical addresses, when the page gets 'recycled' on a page swap, the new data will be treated like a page table.
    On the **next** page swap, it will look at this garbage 'page table', and try to swap out memory at an invalid physical address.