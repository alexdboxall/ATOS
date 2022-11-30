#pragma once

/* 
* heap.h - Kernel Heap 
*/

#include <common.h>

void heap_init(void);
void* malloc(size_t size) warn_unused;
void free(void* ptr);