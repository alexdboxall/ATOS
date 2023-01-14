
#pragma once

#include <spinlock.h>

struct adt_list;
struct virtual_address_space;
struct filedes_table;
struct thread;

struct process {
    struct adt_list* threads;
    int pid;
    struct virtual_address_space* vas;
    struct filedes_table* fdtable;
    struct spinlock lock;
    size_t sbrk;
};

void process_init(void);
struct process* process_create(void);
struct process* process_create_child(struct virtual_address_space* vas, struct filedes_table* fdtable);
int process_kill(struct process* process);
void process_add_thread(struct process* process, struct thread* thread);
struct thread* process_create_thread(struct process* process, void(*initial_address)(void*), void* initial_argument);