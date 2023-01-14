#pragma once

#include <stddef.h>

size_t ksymbol_get_address_from_name(const char* name);
void ksymbol_add_symbol(const char* name, size_t address);
