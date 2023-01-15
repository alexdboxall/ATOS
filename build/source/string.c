
/*
* memcpy and memset call the GCC's 'builtin' version of the function.
* These may be optimised for the target platform, which increases efficiency
* without resorting to writing custom versions for each architecture.
*
* If they are not supported by your compiler or platform, or simply end up calling
* themselves, use the commented-out implementations instead.
*/

#include <string.h>
#include <errno.h>
#include <stdint.h>

// malloc
#ifdef COMPILE_KERNEL
#include <heap.h>
#else
#include <stdlib.h>
#endif

void* memchr(const void* addr, int c, size_t n)
{
	const uint8_t* ptr = (const uint8_t*) addr;

	while (n--) {
		if (*ptr == (uint8_t) c) {
			return (void*) ptr;
		}

		++ptr;
	}

	return NULL;
}

int memcmp(const void* addr1, const void* addr2, size_t n)
{
    const uint8_t* a = (const uint8_t*) addr1;
	const uint8_t* b = (const uint8_t*) addr2;

	for (size_t i = 0; i < n; ++i) {
		if (a[i] < b[i]) return -1;
		else if (a[i] > b[i]) return 1;
	}
	
	return 0;
}

void* memcpy(void* dst, const void* src, size_t n)
{
    /*
    * Use the compiler's platform-specific optimised version.
    * If that doesn't work for your system, use the below implementation.
    */
    return __builtin_memcpy(dst, src, n);

	/*uint8_t* a = (uint8_t*) dst;
	const uint8_t* b = (const uint8_t*) src;

	for (size_t i = 0; i < n; ++i) {
		a[i] = b[i];
	}

	return dst;*/
}

void* memmove(void* dst, const void* src, size_t n)
{
	uint8_t* a = (uint8_t*) dst;
	const uint8_t* b = (const uint8_t*) src;

	if (a <= b) {
		while (n--) {
			*a++ = *b++;
		}
	} else {
		b += n;
		a += n;

		while (n--) {
			*--a = *--b;
		}
	}

	return dst;
}

void* memset(void* addr, int c, size_t n)
{
    /*
    * Use the compiler's platform-specific optimised version.
    * If that doesn't work for your system or compiler, use the below implementation.
    */
    return __builtin_memset(addr, c, n);

	/*uint8_t* ptr = (uint8_t*) addr;
	for (size_t i = 0; i < n; ++i) {
		ptr[i] = c;
	}

	return addr;*/
}

char* strcat(char* dst, const char* src)
{
	char* ret = dst;

	if (*dst) {
		while (*++dst) {
			;
		}
	}

	while ((*dst++ = *src++)) {
		;
	}

	return ret;
}

char* strchr(const char* str, int c)
{
	do {
		if (*str == (char) c) {
			return (char*) str;
		}
	} while (*str++);

	return NULL;
}

int	strcmp(const char* str1, const char* str2)
{
	while ((*str1) && (*str1 == *str2)) {
		++str1;
		++str2;
	}

	return (*(uint8_t*) str1 - *(uint8_t*) str2);
}

char* strcpy(char* dst, const char* src)
{
	char* ret = dst;

	while ((*dst++ = *src++)) {
		;
	}

	return ret;
}

char* strerror(int err)
{
	switch (err) {
	case 0:
		return "Success";
	case ENOSYS:
		return "Not implemented";
	case ENOMEM:
		return "Not enough memory";
	case ENODEV:
		return "No device";
	case EALREADY:
		return "Driver has already been asssigned";
	case ENOTSUP:
		return "Operation not supported";
	case EDOM:
		return "Argument outside of domain of function";
	case EINVAL:
		return "Invalid argument";
	case EEXIST:
		return "File already exists";
	case ENOENT:
		return "No such file or directory";
	case EIO:
		return "Input / output error";
	case EACCES:
		return "Permission denied";
	case ENOSPC:
		return "No space left on device";
	case ENAMETOOLONG:
		return "Filename too long";
	case ENOTDIR:
		return "Not a directory";
	case EISDIR:
		return "Is a directory";
	case ELOOP:
		return "Too many loops in symbolic link resolution";
	case EROFS:
		return "Read-only filesystem";
	case EAGAIN: /* == EWOULDBLOCK */
		return "Resource temporarily unavailable / Operation would block";
    case EFAULT:
        return "Bad address";
    case EBADF:
        return "Bad file descriptor";
    case ENOTTY:
        return "Not a terminal";
    case ERANGE:
        return "Result of out range";
    case EILSEQ:
        return "Illegal byte sequence";
    case EMFILE:
        return "Too many open files";
    case ENFILE:
        return "Too many open files in system";
    case EPIPE:
        return "Broken pipe";
    case ESPIPE:
        return "Invalid seek";
	default:
		return "Unknown error";
	}
}

size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len]) {
		++len;
	}
	return len;
}

char* strncpy(char* dst, const char* src, size_t n)
{
	char* ret = dst;

	while (n--) {
		if (*src) {
			*dst++ = *src++;
		} else {
			*dst++ = '\0';
		}
	}

	return ret;
}

char* strdup(const char* str)
{
	char* copy = (char*) malloc(strlen(str) + 1);
	strcpy(copy, str);
	return copy;
}

int strncmp(const char* str1, const char* str2, size_t n)
{
	while (n && *str1 && (*str1 == *str2)) {
		++str1;
		++str2;
		--n;
	}
	if (n == 0) {
		return 0;
	} else {
		return (*(unsigned char*) str1 - *(unsigned char*) str2);
	}
}

char* strncat(char* dst, const char* src, size_t n);
char* strrchr(const char* str, int n);
char* strstr(const char* haystac, const char* needle);