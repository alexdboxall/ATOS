#pragma once

#include <stddef.h>

#define RAND_MAX 0x7FFFFFFF
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

_Noreturn void exit(int status);

void* malloc(size_t size);
void free(void *ptr);
void* calloc(size_t nmemb, size_t size);

int rand(void);
void srand(unsigned int seed);