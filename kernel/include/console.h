#pragma once

#include <common.h>
#include <interface.h>

void console_init(void);
int console_register_driver(struct console_device_interface* driver);
void console_received_character(char c, bool control_held);
void console_putc(char c);
char console_getc(void);
void console_gets(char* buffer, int size);