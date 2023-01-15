#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int16_t inversion_t;

struct region {
    int offset_x;
    int offset_y;

    int num_data_blocks;

    /*
    * The length of the usable data.
    */
    int data_total_length;

    /*
    * The length of the memory allocated for the data. This may be higher than data_total_length.
    * This is only used during region operations to expand the size of the data dynamically as new
    * data blocks are being added to it.
    * 
    * This is the number of inversion_ts that are allocated, not the number of bytes.
    * 
    * ONLY USED INTERNALLY IN THE REGION OPERATIONS. NOT GUARANTEED TO BE SET PROPERLY BEFORE/AFTER
    * COMPLETION, OR ON NEW OR RESULTING OBJECTS.
    */
    int allocated_amount;

    /*
    * The start index of the previous data block that was added. Used to check if the previous inversion
    * points are the same as the current one, and if so, they can be merged into one. Set to -1 initially.
    * 
    * ONLY USED INTERNALLY IN THE REGION OPERATIONS. NOT GUARANTEED TO BE SET PROPERLY BEFORE/AFTER
    * COMPLETION, OR ON NEW OR RESULTING OBJECTS.
    */
    int prev_add_index;

    /*
    * Data format:
    *       the number of inversion points, followed by the height, followed by that many inversion points
    *   this is done num_data_blocks times
    */
    inversion_t* data;
};

enum region_operation {
    REGION_OP_UNION,
    REGION_OP_INTERSECT,
    REGION_OP_DIFFERENCE
};

struct region region_operate(struct region a, struct region b, enum region_operation operation);

struct region region_create_rectangle(int x, int y, int w, int h);
struct region region_create_oval(int x, int y, int w, int h);
struct region region_create_rounded_rectangle(int x, int y, int w, int h, int radius);

void region_destroy(struct region r);