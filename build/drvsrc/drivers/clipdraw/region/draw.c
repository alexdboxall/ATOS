#include <video.h>
#include <kprintf.h>

#include "region/draw.h"
#include "region/region.h"

/*
* Data format:
*       the number of inversion points, followed by the height, followed by that many inversion points
*   this is done num_data_blocks times
*/

void region_fill(struct region* region, uint32_t colour) {
    inversion_t* data = region->data;
    int vertical_offset = region->offset_y;

    for (int i = 0; i < region->num_data_blocks; ++i) {
        inversion_t num_inversion_points = *data++;
        inversion_t height = *data++;

        for (int j = 0; j < num_inversion_points; j += 2) {
            inversion_t start = *data++;
            inversion_t end = *data++;      // what if odd number of inv. points? can this even happen? if so, should it??

            video_putrect(start + region->offset_x, vertical_offset, end - start, height, colour);
        }    
        
        vertical_offset += height;
    }
}
