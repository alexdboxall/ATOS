#include <thread.h>
#include <heap.h>
#include <virtual.h>
#include <cpu.h>
#include <panic.h>
#include <assert.h>
#include <arch.h>
#include <process.h>
#include <physical.h>
#include <spinlock.h>
#include <kprintf.h>
#include <loader.h>
#include <errno.h>
#include <machine/config.h>

/*
* thread/thread.c - Threads
*
* Operations on threads, such as creation, blocking and sleeping; as well as the
* scheduler.
*/


/*
* Local fixed sized arrays and variables need to fit on the kernel stack.
* Allocate at least 4KB (depending on the system page size).
*
* Please note that overflowing the kernel stack into non-paged memory will lead to
* an immediate and unrecoverable crash on most systems.
*/
#define KERNEL_STACK_SIZE   (virt_bytes_to_pages(4096) * ARCH_PAGE_SIZE)

/*
* The user stack is allocated as needed - this is the maximum size of the stack in
* user virtual memory. (However, a larger max stack means more page tables need to be
* allocated to store it - even if there are no actual stack pages in yet).
*
* On x86, allocating a 4MB region only requires one page table, hence we'll use that.
*/
#define USER_STACK_MAX_SIZE (virt_bytes_to_pages(4 * 1024 * 1024) * ARCH_PAGE_SIZE)

/*
* The scheduler lock must be held whenever calling thread_schedule(), and also must
* be held when doing things such as blocking and unblocking threads, as well as accessing
* the global 'next thread ID'.
*/
struct spinlock scheduler_lock;

static int next_thread_id = 1;

/*
* A linked list storing which threads are currently waiting for their turn on a CPU,
* and are not currently running or locked. The scheduler lock must be held while accessing.
*/
static struct thread* first_ready_thread = NULL;
static struct thread* last_ready_thread = NULL;

/*
* A linked list storing threads which are blocked due to a timer.
* Scheduler lock must be held while accessing.
*/
static struct thread* sleeping_thread_list = NULL;

struct thread* terminated_thread_list = NULL;


/*
* There are times where we need to postpone thread switches until we finish doing some
* processing (e.g. if iterating through a list of threads and unblocking them, we don't
* want to switch to another thread until we're done). 
*
* If postpone_switches is set, this postponing takes effect. if thread_schedule() is 
* called while postponing is active, postpone_switches will be set instead of performing
* an actual schedule.
*
* The postpone_lock be held when accessing these variables.
*/
static bool have_postponed_switch = false;
static bool postpone_switches = false;
static struct spinlock postpone_lock;

/*
* Number of nanoseconds passed since boot. The lock must be held while accessing.
*/
static uint64_t time_since_boot;
static struct spinlock time_since_boot_lock;


/*
* Kernel stack overflow normally results in a total system crash/reboot because 
* fault handlers will not work (they push data to a non-existent stack!).
*
* We will fill pages at the end of the stack with a certain value (CANARY_VALUE),
* and then we can check if they have been modified. If they are, we will throw a 
* somewhat nicer error than a system reboot.
*
* Note that we can still overflow 'badly' if someone makes an allocation on the
* stack lwhich is larger than the remaining space on the stack and the canary size
* combined.
*
* If the canary page is only partially used for the canary, the remainder of the
* page is able to be used normally. Hence at the moment kernel pages are 6KB.
*/
#define NUM_CANARY_BYTES 2048
#define NUM_CANARY_PAGES (virt_bytes_to_pages(NUM_CANARY_BYTES))

#define CANARY_VALUE     0x8BADF00D

static void thread_stack_canary_create(size_t canary_base) {
    uint32_t* canary_ptr = (uint32_t*) canary_base;

#ifdef ARCH_STACK_GROWS_DOWNWARD

    for (size_t i = 0; i < NUM_CANARY_BYTES / sizeof(uint32_t); ++i) {
        *canary_ptr++ = CANARY_VALUE;
    }

#elif ARCH_STACK_GROWS_UPWARD
    #error "please implement thread_stack_canary_create for upward stacks"
#else
    #error "machine/config.h has not defined ARCH_STACK_GROWS_UPWARD or ARCH_STACK_GROWS_DOWNWARD"
#endif
}

/*static*/ void thread_stack_check_canary(size_t canary_base) {
    uint32_t* canary_ptr = (uint32_t*) canary_base;

#ifdef ARCH_STACK_GROWS_DOWNWARD

    for (size_t i = 0; i < NUM_CANARY_BYTES / sizeof(uint32_t); ++i) {
        if (*canary_ptr++ != CANARY_VALUE) {
            panic("Kernel stack overflow detected!");
        }
    }

#elif ARCH_STACK_GROWS_UPWARD
    #error "please implement thread_stack_check_canary for upward stacks"
#else
    #error "machine/config.h has not defined ARCH_STACK_GROWS_UPWARD or ARCH_STACK_GROWS_DOWNWARD"
#endif
}

/*
* Allocates a new page-aligned stack for a kernel thread, and returns
* the address of either the top of the stack (if it grows downward),
* or the bottom (if it grows upward).
*/
static size_t thread_create_kernel_stack(int size, size_t* canary_position) {
    int num_pages = virt_bytes_to_pages(size) + NUM_CANARY_PAGES;
    
    size_t stack_bottom = virt_allocate_backed_pages(num_pages, VAS_FLAG_WRITABLE | VAS_FLAG_LOCKED);
    size_t stack_top = stack_bottom + num_pages * ARCH_PAGE_SIZE;

#ifdef ARCH_STACK_GROWS_DOWNWARD

    *canary_position = stack_bottom;
    thread_stack_canary_create(*canary_position);

    return stack_top;

#elif ARCH_STACK_GROWS_UPWARD
    #error "please implement thread_create_kernel_stack for upward stacks"
#else
    #error "machine/config.h has not defined ARCH_STACK_GROWS_UPWARD or ARCH_STACK_GROWS_DOWNWARD"
#endif
}

static size_t thread_create_user_stack(int size) {
    int num_pages = virt_bytes_to_pages(size);

    /*
    * All user stacks share the same area of virtual memory, but have different
    * mappings to physical memory.
    */

#ifdef ARCH_STACK_GROWS_DOWNWARD
    size_t stack_base = ARCH_USER_STACK_LIMIT - num_pages * ARCH_PAGE_SIZE;
    
    for (int i = 0; i < num_pages; ++i) {        
        //size_t phys = phys_allocate_page();
        //vas_map(current_cpu->current_thread->vas, phys, stack_base + i * ARCH_PAGE_SIZE, VAS_FLAG_USER | VAS_FLAG_WRITABLE);
        
        vas_reflag(vas_get_current_vas(), stack_base + i * ARCH_PAGE_SIZE, VAS_FLAG_USER | VAS_FLAG_WRITABLE | VAS_FLAG_ALLOCATE_ON_ACCESS);
    }

    return ARCH_USER_STACK_LIMIT;

#elif ARCH_STACK_GROWS_UPWARD
    #error "please implement thread_create_user_stack for upward stacks"
#else
    #error "machine/config.h has not defined ARCH_STACK_GROWS_UPWARD or ARCH_STACK_GROWS_DOWNWARD"
#endif
}


/* TODO: move to loader.c */
int thread_execve(const char* filename, char* const argv[], char* const envp[]) {
    /* TODO: argv and envp stuff */
    
    (void) argv;
    (void) envp;
    
    return load_program(filename);
}

void thread_execute_in_usermode(void* addr) {
    /*
    * We also need a usermode stack. We can use the current stack as the kernel
    * stack when it switches to kernel mode for interrupt handling, as we will
    * not return from here, and thus the existing stack can be overwritten.
    */
    size_t new_stack = thread_create_user_stack(USER_STACK_MAX_SIZE);

    spinlock_acquire(&scheduler_lock);
    current_cpu->current_thread->stack_pointer = new_stack;
    spinlock_release(&scheduler_lock);

    char* const argv[] = {"hd0:/usertest.exe", NULL};
    char* const envp[] = {NULL};
    int result = thread_execve(argv[0], argv, envp);
    if (result != 0) {
        kprintf("program load failed: %d\n", result);
        thread_terminate();
    }
   
    arch_flush_tlb();
    arch_switch_to_usermode((size_t) addr, new_stack);

    panic("thread_execute_in_usermode: usermode returned!");
}

/*
* Runs whenever a thread is scheduled for the first time. The implementation of 
* arch_prepare_stack should use the address of this function. 
*/
void thread_startup_handler(void) {
    /*
    * To get here, someone must have called thread_schedule(), and therefore
    * the lock must have been held.
    */
    spinlock_release(&scheduler_lock);

    /* Anything else you might want to do should be done here... */

    /* Go to the address the thread actually wanted. */
    current_cpu->current_thread->initial_address(current_cpu->current_thread->argument);   

    /* The thread has returned, so just panic for now instead of terminating it. */
    panic("not implemented: thread returned!");
}


/*
* Adds a thread to the list of threads that are waiting for their turn on a CPU.
* This allows it to be scheduled.
*/
static void thread_add_to_ready_list(struct thread* thr, bool add_to_front) {
    assert(thr);
    assert(spinlock_is_held(&scheduler_lock));

    thr->next = NULL;
    thr->state = THREAD_STATE_READY;

    if (first_ready_thread == NULL) {
        assert(last_ready_thread == NULL);
        
        /*
        * If there is not currently an entry in the list, we must set both the
        * first and last pointers.
        */
        first_ready_thread = thr;
        last_ready_thread = thr;

    } else {
        assert(last_ready_thread != NULL);
        assert(last_ready_thread->next == NULL);

        /*
        * By adding a thread to the front we can guarantee it runs next,
        * useful for preemption.
        */
        if (add_to_front) {
            thr->next = first_ready_thread;
            first_ready_thread = thr;

        } else {
            last_ready_thread->next = thr;
            last_ready_thread = thr;
        }
    }
}


/*
* Allocates and initialises a thread structure for the initial kernel thread
* on each CPU. Also initialises an idle thread for the CPU.
*/
struct thread* thread_init(void) {
    spinlock_init(&scheduler_lock, "big scheduler lock");
    spinlock_init(&postpone_lock, "postpone thread switch lock");
    spinlock_init(&time_since_boot_lock, "time since boot lock");

    struct thread* thr = malloc(sizeof(struct thread));

    /*
    * This gets set as we get switched out, so it should never be used.
    */
    thr->stack_pointer = 0xCAFEBABE;

    /*
    * Also not used by this thread.
    */
    thr->initial_address = (void(*)(void*)) 0xDEADC0DE;

    /*
    * The kernel VAS is going to be the only one which exists at this point
    * in time, but it doesn't really matter as this is always going to be
    * a kernel thread.
    */
    thr->vas = vas_get_current_vas();

    thr->kernel_stack_top = thread_create_kernel_stack(KERNEL_STACK_SIZE, &thr->canary_position);
    thr->kernel_stack_size = KERNEL_STACK_SIZE + NUM_CANARY_PAGES * ARCH_PAGE_SIZE;
    thr->state = THREAD_STATE_RUNNING;  
    thr->time_used = 0;
    thr->next = NULL;
    thr->name = "Kernel";
    thr->priority = PRIORITY_NORMAL;
    thr->sleep_expiry = 0;
    thr->timeslice_expiry = 1;
    thr->process = NULL;
    thr->argument = NULL;

    spinlock_acquire(&scheduler_lock);
    thr->thread_id = next_thread_id++;
    current_cpu->current_thread = thr;
    spinlock_release(&scheduler_lock);

    idle_thread_init();
    cleaner_thread_init();

    return thr;
}

int thread_fork(void) {
    struct thread* thr = malloc(sizeof(struct thread));

    struct thread* parent_thread = current_cpu->current_thread;

    /*
    * Copy all thread data from the currently running thread.
    * The thread ID must be unique, so update it. The linked list pointer
    * should also be cleared so it can be added to the ready list.
    */
    spinlock_acquire(&scheduler_lock);
    *thr = *parent_thread;
    thr->thread_id = next_thread_id++;
    spinlock_release(&scheduler_lock);

    thr->next = NULL;
    thr->time_used = 0;

    /*
    * The thread being forked is running, but the new one is obviously not.
    */
    thr->state = THREAD_STATE_READY;

    /*
    * Perform the memory allocation steps while the lock isn't held. It is okay to
    * fiddle with thr without the lock held, until thread_add_to_ready_list is called. 
    */

    /*
    * Create a new address space for the thread with everything marked as
    * copy-on-write.
    */
    kprintf("TODO: vas_copy allocates a lot of memory, but it also needs to lock the VAS! (which the pf handler also needs to do!)\n");
    thr->vas = vas_copy(thr->vas);

    struct process* process = process_create_with_vas(thr->vas);

    thr->kernel_stack_top = thread_create_kernel_stack(KERNEL_STACK_SIZE, &thr->canary_position);
    thr->kernel_stack_size = KERNEL_STACK_SIZE + NUM_CANARY_PAGES * ARCH_PAGE_SIZE;
    process_add_thread(process, thr);
    thr->process = process;

    /*
    * Must be done before both threads start executing.
    */
    spinlock_acquire(&scheduler_lock);
    thread_add_to_ready_list(thr, false);

    /*
    * Copy the stack data and set the stack pointer to the correct position in the new stack.
    * Both threads resume immediately after arch_set_forked_kernel_stack.
    */    
    arch_set_forked_kernel_stack(parent_thread, thr);

    /*
    * Both threads resume execution here
    */

    size_t retv = current_cpu->current_thread == parent_thread ? 0 : 1;
    
    /*
    * Both threads release the lock - which is correct. The parent thread will always
    * run first (as the lock is held), and release the lock. Then a task switch (may)
    * occur. When the child eventually switches in, the lock needs to be released because
    * task switching acquires the lock. (Remember, the child never switched out because it
    * was just created - it didn't go through thread_switch which would normally unlock,
    * hence it is necessary to unlock here).
    */
    spinlock_release(&scheduler_lock);

    /*
    * The x86 implementation doesn't save some registers, so we don't have access to
    * local variables. Hence the awkward way of getting the process instead of just using
    * process. (TODO: need to fix the x86 implementation)
    */
    return retv == 0 ? 0 : current_cpu->current_thread->process->pid;
}

/*
* Allocates and initialises a thread structure for new threads and adds it
* to the ready list.
*/
struct thread* thread_create(void (*initial_address)(void*), void* argument, struct virtual_address_space* vas) {    
    struct thread* thr = malloc(sizeof(struct thread));

    thr->state = THREAD_STATE_READY;
    thr->initial_address = initial_address;
    thr->argument = argument;
    thr->time_used = 0;
    thr->next = NULL;
    thr->name = "Kernel Thread";
    thr->priority = PRIORITY_NORMAL;
    thr->sleep_expiry = 0;
    thr->timeslice_expiry = 1;
    thr->process = NULL;

    /*
    * If we switch to usermode, we reassign the stack pointer to a new usermode
    * stack (this is done in thread_execute_in_usermode).
    * 
    * We still keep the existing kernel stack for kernel switches.
    */
    thr->kernel_stack_top = thread_create_kernel_stack(KERNEL_STACK_SIZE, &thr->canary_position);
    thr->kernel_stack_size = KERNEL_STACK_SIZE + NUM_CANARY_PAGES * ARCH_PAGE_SIZE;
    thr->stack_pointer = arch_prepare_stack(thr->kernel_stack_top);

    thr->vas = vas;

    spinlock_acquire(&scheduler_lock);

    thr->thread_id = next_thread_id++;
    thread_add_to_ready_list(thr, false);

    spinlock_release(&scheduler_lock);

    return thr;
}

static void thread_update_time_used(void) {
    static uint64_t prev = 0;

    /*
    * It's okay for prev to be zero the first time this is called, it just means
    * the first thread has the boot time added (which is correct, as it was the thread
    * runnning during the boot process).
    */

    uint64_t current = arch_read_timestamp();
    uint64_t elapsed = current - prev;
    prev = current;

    current_cpu->current_thread->time_used += elapsed;
}


/*
* Blocks the currently held thread. The scheduler lock must already be held.
*/
void thread_block(enum thread_state reason) {    
    assert(spinlock_is_held(&scheduler_lock));
    assert(current_cpu->current_thread->state == THREAD_STATE_RUNNING);

    current_cpu->current_thread->state = reason;
    thread_schedule();
}


/*
* Unblocks a given thread. The scheduler lock must already be held.
*/
void thread_unblock(struct thread* thr) {
    assert(thr);
    assert(thr->state != THREAD_STATE_RUNNING);
    assert(thr->state != THREAD_STATE_READY);
    assert(spinlock_is_held(&scheduler_lock));
    
    /*
    * We sometimes want to preempt the currently running thread, i.e. switch to the
    * newly unblocked thread before the current one's timeslice expires. We implement
    * this by putting the unblocked thread on the front of the ready list and then scheduling.
    * 
    * We preempt lower priority threads, and when there are no other threads (which means it
    * is likely that the currently executing thread would have been executing for a while).
    */
    bool preempt = thr->priority < current_cpu->current_thread->priority || first_ready_thread == NULL;
    thread_add_to_ready_list(thr, preempt);
    if (preempt) {
        thread_schedule();
    }
}

/*
* Prevents thread switching from occuring until thread_end_postpone_switches() is called.
*/
void thread_postpone_switches(void) {
    spinlock_acquire(&postpone_lock);
    postpone_switches = true;
    have_postponed_switch = false;
}

/*
* Allows thread switching to occur again. If a switch was supposed to happen between calling
* thread_postpone_switches() and thread_end_postpone_switches(), it will happen now.
*/
void thread_end_postpone_switches(void) {
    /*
    * Allows the scheduler to actually switch threads instead of just setting
    * the have_postponed_switch flag again. Must be done before we do the postponed
    * switch.
    */ 
    postpone_switches = false;

    /*
    * We must release the postpone lock before calling schedule. Trust me,
    * it gets ugly if you don't. (The thread being switched to has no idea the
    * lock is still held, and so doesn't release it).
    */
    if (have_postponed_switch) {
        have_postponed_switch = false;   
        
        spinlock_release(&postpone_lock);

        assert(spinlock_is_held(&scheduler_lock));

        thread_schedule();

    } else {
        spinlock_release(&postpone_lock);
    }
}

/*
* Sets the timeslice for a thread which has been newly switched in, or resets
* the timeslice for a thread which is 'having another turn'.
*/
static void thread_reset_timeslice(void) {
    spinlock_acquire(&time_since_boot_lock);

    /* 25 ms timeslice */
    current_cpu->current_thread->timeslice_expiry = time_since_boot + 25000000;

    spinlock_release(&time_since_boot_lock);
}

/*
* Selects which thread should run next and runs it. If postponing is enabled, after
* calling thread_postpone_switches(), this function will not actually switch, but
* instead just indicate a thread switch is required.
*/
void thread_schedule(void) {
    /*
    * The lock is required to be held. The reason why we don't acquire and release
    * the lock within this function is that certain operations (such as blocking)
    * require that we call this function with the lock already held, thereby causing
    * a deadlock.
    */
    assert(spinlock_is_held(&scheduler_lock));

    /* We must either have been running or blocked. */
    assert(current_cpu->current_thread->state != THREAD_STATE_READY);

    thread_update_time_used();

#ifndef NDEBUG
    /*
    * This is slow, and so is only enabled in debug mode.
    */
    thread_stack_check_canary(current_cpu->current_thread->canary_position);
#endif

    if (spinlock_is_held(&postpone_lock) && postpone_switches) {
        have_postponed_switch = true;
        return;
    }

    /*
    * This is only NULL if there are no other threads to switch to.
    * Hence this check increases our efficiency by not switching back to ourselves
    * if we're already running.
    */
    if (first_ready_thread != NULL) {
        assert(last_ready_thread != NULL);
        assert(first_ready_thread->state == THREAD_STATE_READY);
 
        struct thread* thr = first_ready_thread;
        first_ready_thread = first_ready_thread->next;
        if (first_ready_thread == NULL) {
            last_ready_thread = NULL;
        }

        if (thr->priority == PRIORITY_IDLE) {
            /*
            * We don't want to schedule an idle thread if we don't have to. If there
            * is a thread that is ready to run, use that instead. If not, check if it
            * is okay for the current thread to continue running.
            */

            if (first_ready_thread != NULL) {
                /*
                * Switch to the next thread which is ready.
                */
                thread_add_to_ready_list(thr, false);
                thr = first_ready_thread;
                first_ready_thread = first_ready_thread->next;
                if (first_ready_thread == NULL) {
                    last_ready_thread = NULL;
                }

            } else if (current_cpu->current_thread->state == THREAD_STATE_RUNNING) {
                /* 
                * Current thread hasn't blocked, so allow it to continue.
                */
                thread_reset_timeslice();
                thread_add_to_ready_list(thr, false);
                return;

            } else {
                /* 
                * No choice but to run the idle thread.
                */
                assert(thr != NULL);
            }
        }

        /*
        * If the current thread is still running (i.e. it did not block),
        * then re-add to the list for next time. If it did block, we have to
        * put it somewhere else.
        */    
        if (current_cpu->current_thread->state == THREAD_STATE_RUNNING) {
            thread_add_to_ready_list(current_cpu->current_thread, false);
        }

        thr->state = THREAD_STATE_RUNNING;

        /*
        * We load the VAS beforehand, as threads which are just starting will not
        * execute anything after arch_switch_thread (they will go to their entry point
        * when arch_switch_thread returns).
        */
        vas_load(thr->vas);
        arch_switch_thread(thr);
    }

    thread_reset_timeslice();
}
 
/*
* Returns the number of nanoseconds since the system was booted.
*/
uint64_t get_time_since_boot(void) {
    /*
    * A lock is needed in case we get interrupted halfway through reading
    * the global variable, as reading it may not be atomic (e.g. on 32-bit
    * systems it likely takes at least 2 reads - if we get interrupted
    * by the timer halfway through could lead to corrupted times.
    */
    spinlock_acquire(&time_since_boot_lock);
    uint64_t time = time_since_boot;
    spinlock_release(&time_since_boot_lock);
    return time;
}

/*
* Blocks a thread until at least a certain number of nanoseconds since the system
* was booted has passed.
*/
void thread_nano_sleep_until(uint64_t when) {
    spinlock_acquire(&scheduler_lock);

    /*
    * Lock should be held before doing this as the time might change
    * if the lock isn't held (and therefore could change in the time in
    * between doing the check and writing the time to the thread structure).
    */
    if (when < get_time_since_boot()) {
        spinlock_release(&scheduler_lock);
    }

    current_cpu->current_thread->sleep_expiry = when;

    /* Add to the sleeping thread list */
    current_cpu->current_thread->next = sleeping_thread_list;
    sleeping_thread_list = current_cpu->current_thread;

    /*
    * The lock still needs to be held when we block it, in case
    * we get interrupted in between modifying the linked list as 
    * we've done and setting the thread state correctly.
    */
    thread_block(THREAD_STATE_SLEEPING);
    spinlock_release(&scheduler_lock);
}

void thread_nano_sleep(uint64_t amount) {
    thread_nano_sleep_until(get_time_since_boot() + amount);
}

void thread_sleep(int seconds) {
    thread_nano_sleep((uint64_t) seconds * 1000000000ULL);
}

void thread_yield(void) {
    spinlock_acquire(&scheduler_lock);
    thread_schedule();
    spinlock_release(&scheduler_lock);
}




void thread_terminate(void) {
    /*
    * TODO: close files, free userspace memory, etc... up here
    */

    /*
    * We are not currently using the userspace stack, so it is okay to free it.
    * The cleaner will be in a different VAS and therefore cannot do it.
    * Doesn't need to be locked, as we cannot return to userspace in this thread
    * now.
    */

    for (size_t i = ARCH_USER_STACK_LIMIT - USER_STACK_MAX_SIZE; i < ARCH_USER_STACK_LIMIT; i += ARCH_PAGE_SIZE) {
        size_t physical = vas_unmap(vas_get_current_vas(), i);
        if (physical != 0) {
            phys_free_page(physical);
        }
    }
        
    spinlock_acquire(&scheduler_lock);
    thread_postpone_switches();

    current_cpu->current_thread->next = terminated_thread_list;
    terminated_thread_list = current_cpu->current_thread;

    thread_block(THREAD_STATE_TERMINATED);

    cleaner_thread_awaken();

    thread_end_postpone_switches();
    spinlock_release(&scheduler_lock);
}


/*
* Called when the BSP (bootstrap processor) receives its timer interrupt.
* This function is responsible for incrementing the global time variable.
*/
void thread_received_timer_interrupt_bsp(uint64_t delta) {
    spinlock_acquire(&time_since_boot_lock);
    time_since_boot += delta;
    spinlock_release(&time_since_boot_lock);
}

/*
* To be called when any processor receives a timer interrupt.
* This includes the BSP, but thread_received_timer_interrupt_bsp should
* be called first to update the global time.
*/
void thread_received_timer_interrupt(void) {
    spinlock_acquire(&scheduler_lock);
    if (current_cpu->current_thread == NULL) {
        spinlock_release(&scheduler_lock);
        return;
    }

    thread_postpone_switches();

    spinlock_acquire(&time_since_boot_lock);

    struct thread* current = NULL;

    /*
    * Move everything from the sleeping list to a temporary list.
    */
    current = sleeping_thread_list;
    sleeping_thread_list = NULL;

    /*
    * Then for each thread, either wake it up or add it back to the
    * sleeping list (which we just emptied).
    */
    while (current != NULL) {
        /*
        * Unblocking a thread changes the next pointer to NULL
        * (it needs to add it to the back of the list), and so we
        * must keep the next pointer stored.
        */
        struct thread* next = current->next;

        if (current->sleep_expiry <= time_since_boot) {
            thread_unblock(current);
        
        } else {
            current->next = sleeping_thread_list;
            sleeping_thread_list = current;
        }

        current = next;
    }

    /* 
    * Zero is a special value meaning not to preempt.
    */
    if (current_cpu->current_thread != NULL) {
        if (current_cpu->current_thread->timeslice_expiry != 0) {
            if (current_cpu->current_thread->timeslice_expiry < time_since_boot) {
                /*
                * Prevent any more preemption until the timeslice is reset.
                */
                current_cpu->current_thread->timeslice_expiry = 0;
                thread_schedule();
            }
        }
    }

    spinlock_release(&time_since_boot_lock);

    thread_end_postpone_switches();
    spinlock_release(&scheduler_lock);
}

/*
* Sets the priority of the current thread. A lower number indicates a 
* higher priority. PRIORITY_IDLE is the lowest priority and is treated specially,
* as they will not be scheduled unless there is anything else to run.
*/
int thread_set_priority(int priority) {
    if (priority < 0 || priority > 255) {
        return EINVAL;
    }

    spinlock_acquire(&scheduler_lock);
    current_cpu->current_thread->priority = priority;
    spinlock_release(&scheduler_lock);

    return 0;
}