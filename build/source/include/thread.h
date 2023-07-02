#pragma once

/*
* thread.h
*/

#include <common.h>

struct process;
struct signal_state;

#define PRIORITY_NORMAL		128
#define PRIORITY_IDLE		255

enum thread_state {
	/* Currently running on a CPU. */
	THREAD_STATE_RUNNING,

	/* Available to be scheduled. */
	THREAD_STATE_READY,

	/* On the sleep list */
	THREAD_STATE_SLEEPING,

	/* Stopped due SIGSTOP, can be resumed by SIGCONT */
	THREAD_STATE_STOPPED,

	/* Blocked, but allowed to be woken by a signal. */
	THREAD_STATE_INTERRUPTIBLE,

	/*
	* Blocked, and will never be interrupted by a signal, and thus cannot
	* be killed while in this state.
	*/
	THREAD_STATE_UNINTERRUPTIBLE,

	/* Thread has terminated, but has not yet been cleaned up yet. */
	THREAD_STATE_TERMINATED,
};

struct thread
{
	/*
    * These must be the first two entries in the struct, as they may be used
    * by assembly code for thread switching. 
	* 
	* virtual_address_space is the platform-specific data found in a
	* struct virtual_address_space.
	*
	* kernel_stack_top is used when switching from userspace to kernel mode.
    */ 
   	size_t NOW_UNUSED;
	size_t kernel_stack_top;			/* offset sizeof(size_t)		*/
	size_t stack_pointer;				/* offset sizeof(size_t) * 2	*/

	/*
	* May be in any order from now on.
	*/
	struct virtual_address_space* vas;

	int thread_id;						/* System-wide unique ID */
	enum thread_state state;
	void (*initial_address)(void*);		/* Address to initially start execution */
	uint64_t time_used;					/* In system-specific units */
	struct thread* next;				/* Used in the implementation of the ready/sleeping lists */
	char* name; 
	uint64_t sleep_expiry;
	int priority;
	void* argument;
	size_t canary_position;
	uint64_t timeslice_expiry;			/* Time since boot in nanoseconds. 0 means no preemption */
    size_t kernel_stack_size;
    struct process* process;            /* The owning process, or NULL if it is a freestanding thread. */
	struct signal_state* signals;
};

struct thread* thread_init(void);
struct thread* thread_create(void (*initial_address)(void*), void* argument, struct virtual_address_space* vas);
void thread_schedule(void);
void thread_block(enum thread_state reason);
void thread_unblock(struct thread* thr);
void thread_nano_sleep_until(uint64_t when);
void thread_nano_sleep(uint64_t amount);
void thread_sleep(int seconds);
void thread_yield(void);
void thread_received_timer_interrupt_bsp(uint64_t delta);
void thread_received_timer_interrupt_bsp(uint64_t delta);
void thread_received_timer_interrupt(void);
void thread_postpone_switches(void);
void thread_end_postpone_switches(void);
void thread_terminate(void);
int thread_set_priority(int priority);

extern struct thread* terminated_thread_list;

/*
* Pass this into thread_create
*/
void thread_execute_in_usermode(void* arg);

int thread_fork(void);
uint64_t get_time_since_boot(void);
extern struct spinlock scheduler_lock;

void idle_thread_init(void);
void cleaner_thread_init(void);
void cleaner_thread_awaken(void);
