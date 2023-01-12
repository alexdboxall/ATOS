#include "region.h"
#include <stdio.h>      // printf
#include <stdlib.h>     // malloc, realloc, free
#include <string.h>     // memcpy, memcmp
#include <assert.h>
#include <math.h>       // sqrt for debugging things

#define MAX_VERTICAL_CHANGES        4096
#define MAX_HORIZONTAL_INVERSIONS   256

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

/*
* We are in the kernel, and therefore we want to avoid using the FPU as much as possible, for a few
* reasons:
*
*       - it would require the FPU state to be saved on kernel task switches, which is slow
*       - ATOS doesn't support FPU yet
*       - not all platforms have an FPU
*
* The first item on that list is the most important in the long run, due to the number of FPU 
* registers that need saving, and/or the complexity of only *sometimes* saving them (FPU traps, etc.).
*
* Hence we will use fixed width integers instead of floats. This should work fine for now, we only really
* need floats to calculate curved sections (ovals, curved rectangles), which only requires sqrt().
* And fixed point sqrt() is really easy (Newton's method).
*/

uint64_t sqrt_fp8(uint64_t val) {
    uint64_t guess = 1 << 8;      // guessing '1'
    uint64_t n_one = val << 8ULL;
    
    for (int i = 0; i < 16; ++i) {
        uint64_t prev_guess = guess;
        if (guess == 0) {
            return 0;
        }
        guess = (guess + n_one / guess) / 2;
        if (prev_guess == guess) {
            break;
        }
    }

    return guess;
}

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

void region_destroy(struct region r) {
    free(r.data);
}

struct region region_create_oval(int x, int y, int w, int h) {
    assert(w >= 0 && h >= 0);

    struct region r;
    r.offset_x = x;
    r.offset_y = y;
    r.num_data_blocks = h;
    r.data_total_length = 4 * h;
    r.data = malloc(r.data_total_length * sizeof(inversion_t));

    int x_radius = w / 2;
    int y_radius = h / 2;

    /*
    * Work out the width of the circle at each scanline, and put it in the middle of the
    * bounding box.
    */
    for (int i = 0; i < h; ++i) {
        /*
        * This is from the formula of an oval. We multiply by 256 in the square root to convert
        * to our 24-8 fixed point format, and then we divide it out at the end.
        */
        int per_side_width = x_radius * sqrt_fp8(256 * (h * h / 4 - (i - y_radius) * (i - y_radius))) / y_radius / 256;

        r.data[i * 4 + 0] = 2;
        r.data[i * 4 + 1] = 1;
        r.data[i * 4 + 2] = x_radius - per_side_width;
        r.data[i * 4 + 3] = x_radius + per_side_width;
    }

    return r;
}

struct region region_create_rounded_rectangle(int x, int y, int w, int h, int radius) {
    assert(radius > 0 && w > radius && h > radius);

    struct region r;
    r.offset_x = x;
    r.offset_y = y;
    r.num_data_blocks = 1 + 2 * radius;
    r.data_total_length = 4 * r.num_data_blocks;
    r.data = malloc(r.data_total_length * sizeof(inversion_t));    

    /*
    * The middle, 'rectangular' part of the rectangle.
    */
    r.data[4 * radius + 0] = 2;
    r.data[4 * radius + 1] = h - 2 * radius;
    r.data[4 * radius + 2] = 0;
    r.data[4 * radius + 3] = w;

    /*
    * Now do the curved parts.
    */
    for (int i = 0; i < radius; ++i) {
        /*
        * This is the tricky bit, calculating how far along we are meant to start. The end point
        * is a simple calculation to get the equivalent on the other side.
        * 
        * Formula: 
        *   y = R * (1 - sqrt(1 - ((R - x) / R)^2)) + 1.5
        *     = R + 1.5 - sqrt(x(2R - x))
        */
        int start = radius + (384 - sqrt_fp8(256 * i * (2 * radius - i))) / 256;
        int end = w - start * 2;


        /*
        * The top curved part.
        */
        int top = i * 4;
        r.data[top + 0] = 2;
        r.data[top + 1] = 1;
        r.data[top + 2] = start;
        r.data[top + 3] = end;

        /*
        * To avoid computing everything twice, we will also do the bottom curved part here too.
        */
        int bottom = 8 * radius - top;
        r.data[bottom + 0] = 2;
        r.data[bottom + 1] = 1;
        r.data[bottom + 2] = start;
        r.data[bottom + 3] = end;
    }

    return r;
}

struct region region_create_rectangle(int x, int y, int w, int h) {
    assert(w >= 0 && h >= 0);

    struct region r;
    r.offset_x = x;
    r.offset_y = y;
    r.num_data_blocks = 1;
    r.data_total_length = 4;
    r.data = malloc(r.data_total_length * sizeof(inversion_t));

    /*
    * It's just a rectangle, so it just has a start and end point, and a height.
    */
    r.data[0] = 2;         // Inversion points in first entry
    r.data[1] = h;         // Height of first entry
    r.data[2] = 0;         // First inversion point
    r.data[3] = w;         // Second inversion point

    return r;
}

/*
* Count is in units of inversion_t, not bytes.
*/
void region_add_data_block(struct region* r, inversion_t* data) {
    int count = data[0] + 2;

    /*
    * Check if the previous block to be added is the same as the current one. If it is, we can just
    * merge the two together to save some time and memory.
    */
    if (r->prev_add_index != -1) {
        int prev_num_inversions = r->data[r->prev_add_index];

        /*
        * They can only be the same if they have the same number of inversion points.
        */
        if (prev_num_inversions == count - 2) {

            /*
            * Now check the inversion points themselves are the same.
            */
            if (!memcmp(r->data + r->prev_add_index + 2, data + 2, (count - 2) * sizeof(inversion_t))) {
                /*
                * Just add the two heights togther, and then stop.
                */
                r->data[r->prev_add_index + 1] += data[1];
                return;
            }
        }   
    }

    r->prev_add_index = r->data_total_length;


    /*
    * Make sure there is enough room for the data. If not, we reallocate the memory to make room for it.
    */
    int final_position = r->data_total_length + count;

    if (final_position >= r->allocated_amount) {
        r->allocated_amount = r->allocated_amount * 2 + count;

        inversion_t* new_data = realloc(r->data, r->allocated_amount * sizeof(inversion_t));
        if (new_data == NULL) {
            assert(false);
        }
        r->data = new_data;
    }

    /*
    * Copy the new data block into the region struct.
    */
    memcpy(r->data + r->data_total_length, data, count * sizeof(inversion_t));
    r->data_total_length += count;
}

void region_add_padding_data_block(struct region* r, int height) {
    /*
    * We don't want this region to be visible, so just make it zero width
    * with no inversion points. Then we can just add it like a data block.
    */
    inversion_t padding_data[2] = {
        0, height
    };

    region_add_data_block(r, padding_data);
}

void region_add_data_block_from_existing(struct region* target, struct region* from, inversion_t base, int height) {
    /*
    * We can't directly copy a data block from a region in the process of constructing a derived
    * region (e.g. from region_operate). This is because the height value needs to be updated (as we may only
    * be using part of the height of the original shape), as well as all of the X positions (as the derived
    * region will have an offset_x of zero, and the inversion points use absolute coordinates.
    */
    inversion_t inversions[MAX_HORIZONTAL_INVERSIONS];
    int num_inversions = from->data[base];

    inversions[0] = num_inversions;
    inversions[1] = height;

    for (int i = 0; i < num_inversions; ++i) {
        /*
        * Copy across, adjusting the offset_x to make up for the fact that target->offset_x
        * will be 0.
        */
        inversions[i + 2] = from->data[base + i + 2] + from->offset_x;
    }

    region_add_data_block(target, inversions);
}

bool region_operation_check_special_case(struct region* a, struct region* b, struct region* output, enum region_operation op, int height, inversion_t a_base, inversion_t b_base) {    
    /*
    * There are a number of special cases where only one shape is active. In this cases, we can just
    * use the type of operation to determine what the result is (it will either be the original, or nothing
    * depending on the operation).
    */
    
    /* An intersection with nothing is nothing. */
    if (op == REGION_OP_INTERSECT && (a_base == -1 || b_base == -1)) {
        region_add_padding_data_block(output, height);
        return true;
    }

    /* The union with nothing is just the original value. */
    if (op == REGION_OP_UNION && a_base == -1) {
        region_add_data_block_from_existing(output, b, b_base, height);
        return true;
    }
    if (op == REGION_OP_UNION && b_base == -1) {
        region_add_data_block_from_existing(output, a, a_base, height);
        return true;
    }

    /* Subtracting nothing makes no difference. */
    if (op == REGION_OP_DIFFERENCE && a_base == -1) {
        region_add_padding_data_block(output, height);
        return true;
    }

    /* If you started with nothing, removing something doesn't change that fact. */
    if (op == REGION_OP_DIFFERENCE && b_base == -1) {
        region_add_data_block_from_existing(output, a, a_base, height);
        return true;
    }

    return false;
}

void region_operation_on_data_block(struct region* a, struct region* b, struct region* output, enum region_operation op, int height, inversion_t a_base, inversion_t b_base) {
    /*
    * Check for cases where one of the input regions is not yet active (i.e. does not contain
    * any data for this part of the region).
    */
    if (region_operation_check_special_case(a, b, output, op, height, a_base, b_base)) {
        return;
    }

    /*
    * Where we are up to in the inversion array. We iterate through, and increment the one with
    * the smallest corresponding X value.
    */
    int a_index = 0;
    int b_index = 0;

    /*
    * "Empty" spaces have 0 inversion points, we must check for this straight away,
    * otherwise it will not see that they have already run out.
    */
    if (a_index >= a->data[a_base]) {
        a_index = -1;
    }
    if (b_index >= b->data[b_base]) {
        b_index = -1;
    }

    /*
    * Whether or not at X position at the current indexes, we are in or out of the region.
    */
    bool a_in = false;
    bool b_in = false;

    /*
    * The inversion points resulting from this operation/
    */
    inversion_t results[MAX_HORIZONTAL_INVERSIONS];
    int result_index = 0; 

    /*
    * Whether or not that the result of the previous operation was within, or not within
    * the resulting region. (i.e. if it was an intersection operation, was a_in && b_in true
    * on this previous iteration).
    * 
    * This is used to determine when we *invert* from being in or out of the result, and thus
    * lets us determine where to put the resulting inversion points.
    */
    bool previous_result = false;

    while (!(a_index == -1 && b_index == -1)) {
        /*
        * Get the current X positions at the indexes. If one of them has run out, then just put
        * a dummy value there (it should not get used anyway, as we will only ever use one of the
        * two values).
        */
        inversion_t a_x_pos = a_index == -1 ? 9999 : a->offset_x + a->data[a_base + 2 + a_index];
        inversion_t b_x_pos = b_index == -1 ? 8888 : b->offset_x + b->data[b_base + 2 + b_index];

        /*
        * One of the two values above, where the inversion (or not) actually happened.
        */
        inversion_t inversion_position;

        /*
        * Swap from being in or out of one or both of the input regions depending on where
        * we are. We increment the one we use as we go to iterate through them in order.
        * (a bit like the "merge" in mergesort)
        */
        if (a_x_pos == b_x_pos) {
            /*
            * Increment both, as we don't want or need to do this twice if they are both
            * the same.
            */
            a_in ^= true;
            b_in ^= true;
            inversion_position = a_x_pos;
            ++a_index;
            ++b_index;
        
        } else if (a_index == -1) {
            /*
            * a has run out, so use b.
            */
            b_in ^= true;
            inversion_position = b_x_pos;
            ++b_index;

        } else if (b_index == -1) {
            /*
            * b has run out, so use a.
            */
            a_in ^= true;
            inversion_position = a_x_pos;
            ++a_index;

        } else {
            /*
            * Pick the smaller one so we go in order.
            */
            if (a_x_pos > b_x_pos) {
                b_in ^= true;
                inversion_position = b_x_pos;
                ++b_index;

            } else {
                a_in ^= true;
                inversion_position = a_x_pos;
                ++a_index;
            }
        }

        /*
        * Check if we've reached the end of one of the regions.
        */
        if (a_index >= a->data[a_base]) {
            a_index = -1;
        }
        if (b_index >= b->data[b_base]) {
            b_index = -1;
        }

        /*
        * Perform the actual operation.
        */
        bool combined_result;
        if (op == REGION_OP_INTERSECT) {
            combined_result = a_in && b_in;

        } else if (op == REGION_OP_DIFFERENCE) { 
            combined_result = a_in && !b_in;

        } else if (op == REGION_OP_UNION) { 
            combined_result = a_in || b_in;

        } else {
            assert(false);
        }

        /*
        * If whether or not we are in the result changed from the previous iteration, it has
        * been "inverted". Therefore, this is where an inversion point should go.
        */
        if (combined_result != previous_result) {
            previous_result = combined_result;

            results[2 + result_index++] = inversion_position;
        }
    }

    results[0] = result_index;
    results[1] = height;
    region_add_data_block(output, results);
}

struct region region_operate(struct region a, struct region b, enum region_operation operation) {
    struct region output;
    output.allocated_amount = 32;
    output.num_data_blocks = 0;
    output.data_total_length = 0;
    output.prev_add_index = -1;
    output.offset_x = 0;
    output.offset_y = MIN(a.offset_y, b.offset_y);
    output.data = malloc(output.allocated_amount * sizeof(inversion_t));

    /*
    * We first must construct an ordered list of all of vertical positions where new blocks start.
    * num_vertical_changes will the resulting num_data_blocks, with the values in the table being
    * the height values of each data block.
    */
    int num_vertical_changes = 0;
    inversion_t vertical_changes[MAX_VERTICAL_CHANGES];

    /*
    * Here we fill in the list. We keep track of where we are in A and B, and choose the smallest,
    * and then update that one. If both A and B have the same value, only one of them should be included,
    * i.e. we must increment both.
    * 
    * The index keeps track of the array index into data, at the start of a data block (i.e. pointing to the
    * 'inversion points in this entry' field). Therefore, to move along to the next entry, we add that value,
    * plus 2 (for the header) to the index.
    * 
    */

    /*
    * The index of the next data block to be looked at.
    * We use an index of -1 to mark that we have already reached the end of the list.
    */
    int a_index = 0;        
    int b_index = 0;

    /*
    * Stores the last unique index value, NOT the last iteration's version of the value.
    * If it is -1, that means the shape hasn't started yet, or the shape has finished.
    * We move 'past' an index when we encounter it, but the data we moved past is still active,
    * as it has a height which goes 'forward', and hence we must still render using the old index.
    */
    int a_render_index = -1;
    int b_render_index = -1;

    inversion_t a_y_pos = a.offset_y;
    inversion_t b_y_pos = b.offset_y;

    while (!(a_index == -1 && b_index == -1)) {
        bool update_a = false;
        bool update_b = false;

        if (a_y_pos == b_y_pos) {
            /*
            * We don't want duplicates in our list, so only add it once, but update both indicies.
            * We don't need to check for -1, as that would have ended the loop.
            */
            update_a = true;
            update_b = true;

        } else if (a_index == -1) {
            /*
            * If A has run out, we must use B.
            */
            update_b = true;

        } else if (b_index == -1) {
            /*
            * If B has run out, we must use A.
            */
            update_a = true;

        } else {
            /*
            * Choose the smallest one, as we need our list to be in order.
            */
            update_a = a_y_pos < b_y_pos;
            update_b = !update_a;
        }

        /*
        * Doesn't check for both update_a and update_b being set, but it doesn't need to, as if they
        * are both set, then the same Y value is being used.
        */
        vertical_changes[num_vertical_changes++] = update_a ? a_y_pos : b_y_pos;

        if (num_vertical_changes >= 2) {
            /*
            * TODO:
            * This is where we would actually perform the scanline based set operation.
            */
            int height = vertical_changes[num_vertical_changes - 1] - vertical_changes[num_vertical_changes - 2];

            region_operation_on_data_block(&a, &b, &output, operation, height, a_render_index, b_render_index);
        }

        /*
        * Perform the updates.
        */
        if (update_a) {
            /* 
            * Check for the end of data. Note that we do this before, not after, updating it. This is
            * because even when we "run out of data", the data we had is still valid and in effect until
            * *next time* we try to update it.
            */

            a_render_index = a_index;

            if (a_index >= a.data_total_length) {
                a_index = -1;
                a_render_index = -1;

            } else {
                a_y_pos += a.data[a_index + 1];        // Height of this data block
                a_index += a.data[a_index] + 2;        // Move to the next entry
            }            
        }

        if (update_b) {
            /* As above. */

            b_render_index = b_index;

            if (b_index >= b.data_total_length) {
                b_index = -1;
                b_render_index = -1;

            } else {
                b_y_pos += b.data[b_index + 1];
                b_index += b.data[b_index] + 2;
            }
        }
    }

    return output;
}

void region_debug_print(struct region output) {
    int len = 0;
    for (int i = 0; i < output.data_total_length; ++i) {
        if (len == 0) {
            len = output.data[i] + 2;
        }
        printf("%d, ", output.data[i]);
        --len;
        if (len == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

int main() {
    //struct region a = region_create_rectangle(50, 50, 100, 75);
    //struct region b = region_create_oval(75, 60, 100, 150);
    //struct region c = region_operate(a, b, REGION_OP_DIFFERENCE);
    struct region d = region_create_rounded_rectangle(0, 0, 50, 30, 10);
    region_debug_print(d);


    return 0;
}