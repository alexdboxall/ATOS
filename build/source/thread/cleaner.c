
#include <thread.h>
#include <virtual.h>
#include <assert.h>
#include <kprintf.h>
#include <cpu.h>
#include <heap.h>
#include <process.h>
#include <physical.h>
#include <adt.h>


/*
* Cleans up a process (including freeing memory) once all of its threads
* have been terminated.
*/
static void cleanup_process(struct process* process) {
    assert(process->vas != vas_get_current_vas());

    vas_destroy(process->vas);
    adt_list_destroy(process->threads);
    free(process);
}

/*
* Frees a kernel stack. This cannot be done while the kernel stack is in use.
*/
static void thread_destroy_kernel_stack(struct thread* thread) { 
    size_t num_pages = virt_bytes_to_pages(thread->kernel_stack_size);
    size_t stack_bottom = thread->kernel_stack_top - thread->kernel_stack_size;

    virt_free_backed_pages(stack_bottom, num_pages);
}


/*
* Cleans up, and frees memory from a thread. This is called by the cleaner thread,
* instead of in thread_terminate(), as it needs to destroy the stack, and the stack
* would still be in use in thread_terminate()
*/
static void cleanup_thread(struct thread* thread) {
    thread_destroy_kernel_stack(thread);

    /*
    * Userspace cleaning is done in thread_terminate(), as it has access
    * to the correct VAS (and can do so, unlike with the kernel stack, as it
    * isn't using the user stack during thread_terminate()).
    */

    /*
    * If it belonged to a process, remove it, and then check if the process
    * is now empty (and thus can be cleaned up also).
    */
    struct process* process = thread->process;
    if (process != NULL) {
        assert(adt_list_size(process->threads) > 0);

        adt_list_remove_element(process->threads, thread);

        if (adt_list_size(process->threads) == 0) {
            cleanup_process(process);
        }
    }

    free(thread);
}

/*
* The implementation of the thread responsible for cleaning up and freeing
* other threads which have terminated.
*/
void cleaner_thread_task(void* arg) {
    (void) arg;

    while (true) {
        /*
        * Wait for someone to call cleaner_thread_awaken() (i.e. wait for a task
        * to terminate).
        */
        kprintf("Cleaner blocking...\n");

        spinlock_acquire(&scheduler_lock);
        thread_block(THREAD_STATE_INTERRUPTIBLE);

        kprintf("Cleaner running...\n");

        /*
        * Cleanup all of the threads that are terminated but yet to be freed.
        */
        while (terminated_thread_list != NULL) {
            struct thread* thread = terminated_thread_list;
            terminated_thread_list = thread->next;

            assert(thread->state == THREAD_STATE_TERMINATED);

            cleanup_thread(thread);
        }

        spinlock_release(&scheduler_lock);
    }
}


static struct thread* cleaner_thread;

/*
* Begins the cleaner thread.
*/
void cleaner_thread_init(void) {
    cleaner_thread = thread_create(cleaner_thread_task, NULL, vas_get_current_vas());
}

/*
* Unblocks the cleaner thread. Should be called from thread_terminate().
*/
void cleaner_thread_awaken(void) {
    assert(spinlock_is_held(&scheduler_lock));

    /*
    * If (somehow) the cleaner thread is already running, don't bother unlocking
    * it. This probably shouldn't happen, as the cleaner holds the schedule lock
    * while it is running.
    */
    if (cleaner_thread->state == THREAD_STATE_INTERRUPTIBLE) {
        thread_unblock(cleaner_thread);
    }
}