#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void bitarray_set(uint8_t* array, size_t position);
void bitarray_clear(uint8_t* array, size_t position);
bool bitarray_is_set(uint8_t* array, size_t position);