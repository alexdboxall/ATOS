#include <stdlib.h>
#include <string.h>
#include <syscallnum.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

uint64_t rand_seed = 1;

int rand(void) {
    rand_seed = rand_seed * 164603309694725029ULL + 14738995463583502973ULL;
    return (rand_seed >> 33) & 0x7FFFFFFF;    
}

void srand(unsigned int seed) {
    rand_seed = seed;
}

_Noreturn void exit(int status) {
    fflush(NULL);

    // TODO: close all files

    // TODO: atexit handlers

    // TODO: tell the system to terminate the process with status 'status'
    (void) status;

    while (true) {
        _system_call(SYSCALL_YIELD, 0, 0, 0, 0);  
    }
}

void* malloc(size_t size) {
    // TODO: use an actual allocator

    if (size == 0) {
        errno = ENOMEM;
        return NULL;
    }

    static size_t recent_sbrk = 0;
    static size_t allocation_position = 0;

    size = size + (16 - (size % 16)) % 16;

    if (allocation_position == 0 || allocation_position + size >= recent_sbrk) {
        size_t prev_sbrk, current_sbrk;
        int result = _system_call(SYSCALL_SBRK, (size_t) size * 2, 0, (size_t) &prev_sbrk, (size_t) &current_sbrk);
        if (result != 0) {
            errno = result;
            return NULL;
        }

        allocation_position = prev_sbrk;
        recent_sbrk = current_sbrk;
    }
    
    size_t result = allocation_position;
    allocation_position += size;

    return (void*) result;
}

void free(void *ptr) {
    (void) ptr;

    // TODO: implement
}

void* calloc(size_t nmemb, size_t size) {
    size_t total_size = nmemb * size;
    
    if (nmemb != 0 && total_size / nmemb != size) {
        /*
        * The multiplication of nmemb and size caused an integer overflow.
        */
        errno = EINVAL;
        return NULL;
    }

    void* ptr = malloc(total_size);
    memset(ptr, 0, total_size);
    return ptr;
}