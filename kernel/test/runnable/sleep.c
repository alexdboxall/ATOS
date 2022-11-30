
#include <thread.h>
#include <cpu.h>
#include <spinlock.h>
#include <kprintf.h>

void test_sleep(void) {
	for (int i = 10; i > 0; --i) {
        kprintf("%d... ", i);
        thread_sleep(1);
    }
    kprintf("\nLiftoff!\n");

    thread_sleep(1);
}