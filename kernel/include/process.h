
#pragma once

#include <adt.h>
#include <thread.h>
#include <virtual.h>
#include <spinlock.h>

struct process {
    struct adt_list* threads;
    int pid;
    struct virtual_address_space* vas;
    struct spinlock lock;
};

void process_init(void);
struct process* process_create(void);
struct process* process_create_with_vas(struct virtual_address_space* vas);
int process_kill(struct process* process);
void process_add_thread(struct process* process, struct thread* thread);
struct thread* process_create_thread(struct process* process, void(*initial_address)(void*), void* initial_argument);