#pragma once

/*
* assert.h - Assertions
*
*
* Used to verify that assumptions are true.
*
*/

#ifdef COMPILE_KERNEL

#ifndef NDEBUG

#define ___tostr(x) #x
#define __tostr(x) ___tostr(x)

_Noreturn void assertion_fail(const char* file, const char* line, const char* condition, const char* msg);

#define assert(condition) (condition ? (void)0 : (void)assertion_fail(__FILE__, __tostr(__LINE__), __tostr(condition), ""));
#define assert_with_message(condition, msg) (condition ? (void)0 : (void)assertion_fail(__FILE__, __tostr(__LINE__), __tostr(condition), msg));

#else

#define assert(condition)
#define assert_with_message(condition, msg)

#endif


#endif