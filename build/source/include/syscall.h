#pragma once

#include <stddef.h>

#define SYSCALL_TABLE_SIZE 16

#include <syscallnum.h>

void syscall_init(void);
int syscall_perform(int call_number, size_t args[4]);