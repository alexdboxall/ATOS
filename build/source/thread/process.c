#include <process.h>
#include <virtual.h>
#include <errno.h>
#include <assert.h>
#include <spinlock.h>
#include <kprintf.h>
#include <heap.h>

static struct spinlock pid_spinlock;
static int next_pid = 1;

struct process* process_create_with_vas(struct virtual_address_space* vas) {
    struct process* process = malloc(sizeof(struct process));
    process->threads = adt_list_create();
    process->vas = vas;

    spinlock_acquire(&pid_spinlock);
    process->pid = next_pid++;
    spinlock_release(&pid_spinlock);

    spinlock_init(&process->lock, "process lock");

    return process;
}

struct process* process_create(void) {
    return process_create_with_vas(vas_create());
}


/*
* Destroys all threads in the process and then deletes it.
*
* TODO: can this block? e.g. if a thread is blocked...?
*/
int process_kill(struct process* process) {
    (void) process;

    // When you terminate a process, you terminate all threads belonging to that process, 
    // except for the current one, then the current thread is also terminated if it belongs 
    // to the process being terminated.
    
    return ENOSYS;
}

void process_add_thread(struct process* process, struct thread* thread) {
    assert(process);
    assert(thread);

    spinlock_acquire(&process->lock);
    thread->process = process;
    adt_list_add_back(process->threads, thread);
    spinlock_release(&process->lock);
}

struct thread* process_create_thread(struct process* process, void(*initial_address)(void*), void* initial_argument) {
    struct thread* thr = thread_create(initial_address, initial_argument, process->vas);
    process_add_thread(process, thr);
    return thr;
}

void process_init(void) {
    spinlock_init(&pid_spinlock, "next pid lock");
}