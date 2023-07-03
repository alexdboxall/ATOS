#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct region region_create_cursor_black(int x, int y);
struct region region_create_cursor_white(int x, int y);
