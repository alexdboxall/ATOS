
#include <common.h>
#include <fcntl.h>
#include <vfs.h>
#include <uio.h>
#include <console.h>

static char* stringifyxWithBase(uint32_t i, char b[], int base)
{
	char const digit[] = "0123456789ABCDEF";

	char* p = b;
	uint32_t shifter = i;
	do { //Move to where representation ends
		++p;
		shifter = shifter / base;
	} while (shifter);
	*p = '\0';
	do { //Move back, inserting digits as u go
		*--p = digit[i % base];
		i = i / base;
	} while (i);
	return b;
}

static void outb(uint16_t port, uint8_t value)
{
	asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t inb(uint16_t port)
{
	uint8_t value;
	asm volatile ("inb %1, %0"
		: "=a"(value)
		: "Nd"(port));
	return value;
}

static void logc(char c)
{
	console_putc(c);
	
	while ((inb(0x3F8 + 5) & 0x20) == 0) {
		;
	}
	outb(0x3F8, c);
}

static void logs(char* a)
{
	while (*a) logc(*a++);
}

static void logWriteInt(uint32_t i)
{
	char y[12];
	logs(stringifyxWithBase(i, y, 10));
}

static void logWriteIntBase(uint32_t i, int base)
{
	char y[12];
	logs(stringifyxWithBase(i, (char*) y, base));
}

void kprintf(const char* format, ...)
{
	if (format == NULL) {
		kprintf("<< kprintf called with format == NULL >>\n");
	}

	va_list list;
	va_start(list, format);
	int i = 0;

	while (format[i]) {
		if (format[i] == '%') {
			switch (format[++i]) {
			case '%': 
				logc('%'); break;
			case 'c':
				logc(va_arg(list, int)); break;
			case 's': 
				logs(va_arg(list, char*)); break;
			case 'd': 
				logWriteInt(va_arg(list, signed)); break;
			case 'x':
			case 'X': 
				logWriteIntBase(va_arg(list, unsigned), 16); break;
			case 'l':
			case 'L': 
				logWriteIntBase(va_arg(list, unsigned long long), 16); break;
			case 'u':
				logWriteInt(va_arg(list, unsigned)); break;
			default: 
				logc('%'); 
				logc(format[i]);
				break;
			}
		} else {
			logc(format[i]);
		}
		i++;
	}

	va_end(list);
}