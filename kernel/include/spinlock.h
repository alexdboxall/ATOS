#pragma once

/*
* spinlock.h - Spinlocks
*
* Implemented in thread/spinlock.c
*/

#include <common.h>

struct spinlock
{
	volatile size_t lock;
	int cpu_number;
	char name[64];
};

void spinlock_init(struct spinlock* lock, const char* name);
void spinlock_acquire(struct spinlock* lock);
void spinlock_release(struct spinlock* lock);
bool spinlock_is_held(struct spinlock* lock);
