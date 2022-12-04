
#include <thread.h>
#include <cpu.h>
#include <heap.h>
#include <string.h>
#include <kprintf.h>

/*
* Tests that your malloc and free implementation work, and actually do free memory.
* This would use over 600MB of memory if freeing isn't implemented.
*
* Just because this test passes doesn't mean your heap is free of bugs!!
*/
void test_heap(void) {
    for (int i = 0; i < 500000; ++i) {
        void* addr = malloc(123);
        memset(addr, i, 123);
        free(addr);
    }

    for (int i = 0; i < 5000; ++i) {
        void* addr = malloc(123456);
        memset(addr, i, 123456);
        free(addr);
    }

    kprintf("Good.\n");
}