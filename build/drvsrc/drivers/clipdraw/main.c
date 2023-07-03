
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
    /*
    * TODO: we should be able to optimise this by only invalidating a certain window,
    * not the whole thing (i.e. the way we do it now, if something changes under another
    * window, the thing on top will still be repainted)
    * 
    * Or just have an 'dirty' flag on each window (or even region), and only paint it and 
    * its children if it is dirty??
    * 
    * We just have to be careful with alpha. (It might be that we check for any higher windows
    * that overlap the invalidated sections, and if there is alpha, it is also redrawn, i.e.
    * acts as if it has the dirty flag set).
    */

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

void _driver_entry_point() {
    struct region bg = region_create_rectangle(0, 0, 1200, 800);

    struct region cursor_b = region_create_cursor_black(50, 40);
    struct region cursor_w = region_create_cursor_white(50, 40);

    struct window cursor_b_win = {
        .children = {NULL},
        .num_children = 0,
        .colour = 0xFF000000,
        .rgn = cursor_b
    };

    struct window cursor_w_win = {
        .children = {NULL},
        .num_children = 0,
        .colour = 0xFFFFFFFF,
        .rgn = cursor_w
    };

    struct window cursor_window = {
        .children = {&cursor_w_win, &cursor_b_win},
        .num_children = 2,
        .colour = 0x00000000,
        .rgn = bg
    };

    struct window r1_win = create_window(100, 100, 500, 400, false);
    struct window r2_win = create_window(200, 130, 700, 400, false);
    struct window r3_win = create_window(300, 250, 400, 350, true);

    struct window bg_win = {
        .children = {&cursor_window, &r3_win, &r2_win, &r1_win},
        .num_children = 4,
        .colour = 0xFF60D0F0,
        .rgn = bg
    };

    window_paint_all(bg_win);

    /*struct region bg = region_create_rectangle(0, 0, 1200, 800);
    region_fill(&bg, 0xFF60D0F0);
    
    demo_window_draw(100, 100, 500, 400, false);
    demo_window_draw(200, 130, 700, 400, false);
    demo_window_draw(300, 250, 400, 350, true);*/

    /*struct region a = region_create_rectangle(400, 300, 100, 100);
    struct region b = region_create_rectangle(450, 350, 100, 100);
    struct region c = region_operate(b, a, REGION_OP_DIFFERENCE);

    region_fill(&a, 0xFF0000FF);
    region_fill(&b, 0xFF00FF00);
    region_fill(&c, 0xFFFF0000);*/
}
