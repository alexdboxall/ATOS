#pragma once

#include <common.h>

void heap_init(void);
void heap_reinit(void);

void* malloc(size_t size) warn_unused;
void* realloc(void* ptr, size_t size) warn_unused;
void free(void* ptr);