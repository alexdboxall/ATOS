#pragma once

#ifndef NDEBUG
#include <kprintf.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#define BEGIN_TEST(msg) //{ kprintf("Testing %s", msg); int j = 60 - strlen(msg); for (int i = 0; i < j; ++i) { kprintf(" ");} kprintf("..."); }
#define END_TEST()      //kprintf(" passed\n");

/*
* Internal kernel unit tests.
*/
void test_kernel(void);

/*
* Tests that can be run by the user.
*/
void test_run(const char* name);

#endif
