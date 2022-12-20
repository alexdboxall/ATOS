#pragma once

#include <stdbool.h>

int load_driver(const char* filename, bool lock_in_memory);
int load_program(const char* filename, size_t* entry_point, size_t* sbrk_point);