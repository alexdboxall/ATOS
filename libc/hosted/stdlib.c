#include <stdlib.h>
#include <string.h>

void* malloc(size_t size) {
    // TODO: use sbrk
    // TODO: use an actual allocator

    if (size == 0) {
        return NULL;
    }

    static size_t position = 0x20000000;
    size_t result = position;
    size = (size + 15) & 0xF;
    position += size;
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
        return NULL;
    }

    void* ptr = malloc(total_size);
    memset(ptr, 0, total_size);
    return ptr;
}