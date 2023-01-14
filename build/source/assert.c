
#include <assert.h>

#ifdef COMPILE_KERNEL

#include <kprintf.h>
#include <panic.h>

_Noreturn void assertion_fail(const char* file, const char* line, const char* condition, const char* msg)
{
	kprintf("Assertion failed: %s %s [%s: %s]\n", condition, msg, file, line);
	panic("Assertion fail");
}

#else

void __empty_translation_unit_otherwise_assert_h() {

}


#endif