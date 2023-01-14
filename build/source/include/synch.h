#pragma once

#include <common.h>
#include <spinlock.h>
#include <thread.h>

struct semaphore {
    int max_count;
    int current_count;

    struct thread* first_waiting_thread;
    struct thread* last_waiting_thread;
};

struct semaphore* semaphore_create(int max_count);
void semaphore_destory(struct semaphore* sem);
void semaphore_acquire(struct semaphore* sem);
void semaphore_release(struct semaphore* sem);
void semaphore_set_count(struct semaphore* sem, int count);
int semaphore_try_acquire(struct semaphore* sem);


struct rw_lock {
    struct semaphore* global;
    struct semaphore* readers;

    int counter;
};

struct rw_lock* rw_lock_create();
void rw_lock_destroy(struct rw_lock* lock);
void rw_lock_acquire_read(struct rw_lock* lock);
void rw_lock_release_read(struct rw_lock* lock);
void rw_lock_acquire_write(struct rw_lock* lock);
void rw_lock_release_write(struct rw_lock* lock);