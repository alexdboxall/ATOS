
#include <kprintf.h>
#include <fcntl.h>
#include <vfs.h>
#include <string.h>
#include <heap.h>
#include <vnode.h>
#include <sys/stat.h>
#include <video.h>

#include "region/region.h"
#include "region/draw.h"

void _driver_entry_point() {
    struct region bg = region_create_rectangle(0, 0, 800, 600);
    region_fill(&bg, 0xFF60D0F0);

    struct region shadow_1 = region_create_rounded_rectangle(100, 100, 500, 400, 10);
    struct region shadow_2 = region_create_rounded_rectangle(101, 101, 498, 398, 10);
    struct region shadow_3 = region_create_rounded_rectangle(102, 102, 496, 396, 10);
    struct region shadow_4 = region_create_rounded_rectangle(103, 103, 494, 394, 10);
    struct region shadow_5 = region_create_rounded_rectangle(104, 104, 492, 392, 10);
    struct region rgn = region_create_rounded_rectangle(105, 105, 490, 390, 10);

    region_fill(&shadow_1, 0x01000000);
    region_fill(&shadow_2, 0x04000000);
    region_fill(&shadow_3, 0x10000000);
    region_fill(&shadow_4, 0x10000000);
    region_fill(&shadow_5, 0x10000000);
    region_fill(&rgn, 0xFFF2F0F0);
}