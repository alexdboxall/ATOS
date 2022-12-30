#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

struct region;
typedef int16_t inversion_t;

enum region_operation {
    REGION_OP_UNION,
    REGION_OP_INTERSECT,
    REGION_OP_DIFFERENCE
};

struct region region_create_rectangle(int x, int y, int w, int h);
struct region region_operate(struct region a, struct region b, enum region_operation operation);

void region_destroy(struct region r);