#pragma once

#include <stddef.h>

#define RAND_MAX 0x7FFFFFFF

void* malloc(size_t size);
void free(void *ptr);
void* calloc(size_t nmemb, size_t size);

int rand(void);
void srand(unsigned int seed);