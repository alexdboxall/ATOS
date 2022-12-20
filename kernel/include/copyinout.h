#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

int copyin(void* kernel_addr, const void* user_addr, size_t size);
int copyout(const void* kernel_addr, void* user_addr, size_t size);