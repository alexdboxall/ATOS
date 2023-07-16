
#include <kprintf.h>
#include <fcntl.h>
#include <vfs.h>
#include <string.h>
#include <heap.h>
#include <vnode.h>
#include <sys/stat.h>
#include <video.h>

#include "region/region.h"
#include "region/cursor.h"
#include "region/draw.h"

/*
struct window {
    struct region rgn;
    uint32_t colour;

    struct window* children[32];
    int num_children;
};

struct stack_entry {
    struct window win;
    struct region alpha_rgn;
};

struct stack_entry stack[256];
int stack_ptr = 0;

void stack_push(struct stack_entry e) {
    stack[stack_ptr++] = e;
}

struct stack_entry stack_pop() {
    return stack[--stack_ptr];
}

struct region window_paint(struct window win, struct region bound, struct region already_painted) {
    // must free already_painted, and return a new copy of it (or just return it unmodified)

    struct region intermediate = region_operate(bound, win.rgn, REGION_OP_INTERSECT);
    struct region new_bound = region_operate(intermediate, already_painted, REGION_OP_DIFFERENCE);

    if (region_is_empty(new_bound)) {
        region_destroy(intermediate);
        region_destroy(new_bound);
        return already_painted;
    }

    for (int i = 0; i < win.num_children; ++i) {
        already_painted = window_paint(*(win.children[i]), new_bound, already_painted);
    }

    region_destroy(new_bound);
    new_bound = region_operate(intermediate, already_painted, REGION_OP_DIFFERENCE);
    region_destroy(intermediate);

    if ((win.colour >> 24) != 0xFF) {
        struct stack_entry entry;
        entry.alpha_rgn = new_bound;
        entry.win = win;
        stack_push(entry);

        return already_painted; 

    } else {
        region_fill(&new_bound, win.colour);
        region_destroy(new_bound);

        struct region result = region_operate(already_painted, win.rgn, REGION_OP_UNION);
        region_destroy(already_painted);
        return result;
    }
}

void invalidate_part_of_screen(struct window root_window, int x, int y, int w, int h) {
    struct region empty = region_create_rectangle(0, 0, 0, 0);
    struct region invalidation = region_create_rectangle(x, y, w, h);
    window_paint(root_window, invalidation, empty);
    region_destroy(empty);
    region_destroy(invalidation);

    while (stack_ptr > 0) {
        struct stack_entry entry = stack_pop();
        region_fill(&entry.alpha_rgn, entry.win.colour);
    }
}

void window_paint_all(struct window root) {
    invalidate_part_of_screen(root, 0, 0, 9999, 9999);
}

void demo_window_draw(int x, int y, int w, int h, bool has_focus) {
    int shadow_width = has_focus ? 12 : 8;

    struct region previous = region_create_rounded_rectangle(x, y, w, h, 10 + shadow_width);

    for (int i = 1; i < shadow_width + 1; ++i) {
        struct region current = region_create_rounded_rectangle(x + i, y + i, w - 2 * i, h - 2 * i, 10 + shadow_width - i);
        struct region clipped = region_operate(previous, current, REGION_OP_DIFFERENCE);
        region_fill(&clipped, (i * i / 4 + 1) << 24);

        region_destroy(previous);
        region_destroy(clipped);
        previous = current;
    }

    struct region rgn = region_create_rounded_rectangle(x + shadow_width, y + shadow_width, w - shadow_width * 2, h - shadow_width * 2, 10);
    region_fill(&rgn, 0xFFF2F0F0);
}

void window_add_child(struct window* window, struct window* child) {
    window->children[window->num_children++] = child;
}

struct window create_window(int x, int y, int w, int h, bool has_focus) {
    struct window win = {
        .children = {NULL},
        .num_children = 0,
        .colour = 0x00000000,
        .rgn = region_create_rectangle(x, y, w, h)
    };

    int shadow_width = has_focus ? 12 : 8;

    struct region previous = region_create_rounded_rectangle(x, y, w, h, 10 + shadow_width);

    for (int i = 1; i < shadow_width + 1; ++i) {
        struct region current = region_create_rounded_rectangle(x + i, y + i, w - 2 * i, h - 2 * i, 10 + shadow_width - i);
        struct region clipped = region_operate(previous, current, REGION_OP_DIFFERENCE);

        struct window* win_sub = malloc(sizeof(struct window));
        win_sub->num_children = 0;
        win_sub->colour = (i * i / 4 + 1) << 24;
        win_sub->rgn = clipped;

        window_add_child(&win, win_sub);

        region_destroy(previous);
        previous = current;
    }

    struct region rgn = region_create_rounded_rectangle(x + shadow_width, y + shadow_width, w - shadow_width * 2, h - shadow_width * 2, 10);
    struct window* win_sub = malloc(sizeof(struct window));
    win_sub->num_children = 0;
    win_sub->colour = 0xFFF2F0F0;
    win_sub->rgn = rgn;
    window_add_child(&win, win_sub);
    return win;
}
*/

struct colour_region {
    struct region rgn;
    uint32_t colour;
};


void add_window(struct colour_region* rgns[], int* index, int x, int y, int w, int h, bool has_focus) {
    int shadow_width = has_focus ? 12 : 8;

    struct region previous = region_create_rounded_rectangle(x, y, w, h, 10 + shadow_width);

    for (int i = 1; i < shadow_width + 1; ++i) {
        struct region current = region_create_rounded_rectangle(x + i, y + i, w - 2 * i, h - 2 * i, 10 + shadow_width - i);
        struct region clipped = region_operate(previous, current, REGION_OP_DIFFERENCE);

        struct colour_region* col_rgn = malloc(sizeof(struct colour_region));
        col_rgn->colour = (i * i / 4 + 1) << 24;
        col_rgn->rgn = clipped;

        rgns[*index] = col_rgn;
        *index += 1;

        region_destroy(previous);
        previous = current;
    }

    struct region rgn = region_create_rounded_rectangle(x + shadow_width, y + shadow_width, w - shadow_width * 2, h - shadow_width * 2, 10);
    struct colour_region* col_rgn = malloc(sizeof(struct colour_region));
    col_rgn->colour = 0xFFF2F0F0;
    col_rgn->rgn = rgn;
    rgns[*index] = col_rgn;
    *index += 1;
}

/*
* 'Rgns' must end with a NULL to indicate the end of the list
*/
static void paint_all(int index, struct colour_region* rgns[], struct region mask) {
    if (rgns[index] == NULL) {
        return;
    }
    
    struct region current = rgns[index]->rgn;
    uint32_t colour = rgns[index]->colour;
    bool transparent = (colour >> 24) != 0xFF;

    struct region current_clipped = region_operate(current, mask, REGION_OP_INTERSECT);
    struct region visible_remainder = region_operate(mask, current, REGION_OP_DIFFERENCE);

    paint_all(index + 1, rgns, transparent ? mask : visible_remainder);
    region_fill(&current_clipped, colour);

    region_destroy(current_clipped);
    region_destroy(visible_remainder);
}


void _driver_entry_point() {
    struct region bg_ = region_create_rectangle(0, 0, 1200, 800);
    struct colour_region bg;
    bg.rgn = bg_;
    bg.colour = 0xFF008080;

    struct colour_region* rgns[256];
    int index = 0;
    memset(rgns, 0, sizeof(rgns));

    add_window(rgns, &index, 150, 150, 700, 500, true);
    add_window(rgns, &index, 50, 50, 400, 300, false);
    rgns[index++] = &bg;

    struct region dirty_rgn = region_create_rectangle(0, 0, 1200, 800);

    paint_all(0, rgns, dirty_rgn);
}
