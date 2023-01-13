
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


void demo_window_draw(int x, int y, int w, int h, bool has_focus) {
    int shadow_width = has_focus ? 12 : 8;

    for (int i = 0; i < shadow_width; ++i) {
        struct region shadow = region_create_rounded_rectangle(x + i, y + i, w - 2 * i, h - 2 * i, 10 + shadow_width - i);
        region_fill(&shadow, (1 << (i / 8)) << 24);
    }

    struct region rgn = region_create_rounded_rectangle(x + shadow_width, y + shadow_width, w - shadow_width * 2, h - shadow_width * 2, 10);

    region_fill(&rgn, 0xFFF2F0F0);
}

void _driver_entry_point() {
    struct region bg = region_create_rectangle(0, 0, 1200, 800);
    region_fill(&bg, 0xFF60D0F0);

    
    demo_window_draw(100, 100, 500, 400, false);
    demo_window_draw(300, 250, 400, 350, true);
}