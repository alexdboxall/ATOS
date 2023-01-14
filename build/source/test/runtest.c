
#include <kprintf.h>
#include <test.h>
#include <string.h>
#include <stdbool.h>

struct runnable_test {
    void (*test)(void);
    const char* name;
};

void test_stack_canary(void);
void test_sleep(void);
void test_heap(void);

struct runnable_test tests[] = {
    {.name = "canary", .test = test_stack_canary},
    {.name = "sleep", .test = test_sleep},
    {.name = "heap", .test = test_heap},
};

void test_run(const char* name) {    
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        if (!strcmp(tests[i].name, name)) {
            tests[i].test();
            return;
        }
    }
    
    kprintf("Test not found.\n");
}