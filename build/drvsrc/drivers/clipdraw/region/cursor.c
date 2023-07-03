#include "region/region.h"
#include <heap.h>
#include <kprintf.h>
#include <string.h>
#include <assert.h>

/*
* Creates a new region in the shape of the black part of the cursor.
* The black section is drawn first.
*/
struct region region_create_cursor_black(int x, int y) {
    return region_create_rectangle(x, y, 12, 18);
}

/*
* Creates a new region in the shape of the white part of the cursor.
* The white section is drawn after the black section.
*/
struct region region_create_cursor_white(int x, int y) {
    return region_create_rectangle(x + 1, y + 1, 10, 16);
}