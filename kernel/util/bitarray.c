
#include <bitarray.h>

void bitarray_set(uint8_t* array, size_t position)
{
	array[position / 8] |= (1 << (position % 8));
}

void bitarray_clear(uint8_t* array, size_t position)
{
	array[position / 8] &= ~(1 << (position % 8));
}

bool bitarray_is_set(uint8_t* array, size_t position)
{
	return array[position / 8] & (1 << (position % 8));
}