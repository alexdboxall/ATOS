
#include <assert.h>

#ifdef COMPILE_KERNEL

#include <arch.h>
#include <kprintf.h>
#include <panic.h>

_Noreturn void assertion_fail(const char* file, const char* line, const char* condition, const char* msg)
{
	arch_disable_interrupts();
	kprintfnv("Assertion failed: %s %s [%s: %s]\n---------------\n@@@\n\n", condition, msg, file, line);
	kprintf("Assertion failed: %s %s [%s: %s]\n", condition, msg, file, line);
	panic("Assertion fail");
}

#else

void __empty_translation_unit_otherwise_assert_h() {

}


#endif