
#include <stdint.h>
#include <beeper.h>
#include <machine/mem/virtual.h>
#include <machine/portio.h>
#include <machine/console.h>
#include <arch.h>
#include <string.h>
#include <spinlock.h>
#include <thread.h>
#include <console.h>
#include <video.h>
#include <heap.h>
#include <errno.h>
#include <vfs.h>
#include <kprintf.h>
#include <device.h>
#include <assert.h>
#include "font.h"

/*
* VESA Terminal and Video Driver for x86 Systems
* 
* A driver for the BIOS VBE video modes. Implements an emulated VGA terminal which can have an
* arbitrary number of rows and columns, based on the video mode. It also allows for direct
* access to the framebuffer via the video interface.
*/

int SCREEN_WIDTH = 128;
int SCREEN_HEIGHT = 48;

/*
* Additional data that is stored within the device interface.
*/
struct vesa_data {
    int cursor_x;
    int cursor_y;
    uint32_t fg_colour;
    uint32_t bg_colour;
    uint32_t framebuffer_physical;
    uint8_t* framebuffer_virtual;
    int width;
    int pitch;
    int height;
    int depth_in_bits;
};



/*
* Update the position of the hardware cursor (that little blinking
* underline where the cursor is).
*/
static void vesa_update_cursor(struct console_device_interface* dev) {
    (void) dev;

    /*
    * TODO: draw a cursor
    */
}


/*
* Moves the cursor position to a newline, handling the case where
* it reaches the bottom of the terminal
*/
static void vesa_newline(struct console_device_interface* dev) {
    struct vesa_data* data = dev->data;

    data->cursor_x = 0;

    assert(data->cursor_y < SCREEN_HEIGHT);

    if (data->cursor_y == SCREEN_HEIGHT - 1) {
        /*
        * Keep the cursor on the final line, but scroll the terminal
        * back and clear the final line.
        */

        memmove((void*) data->framebuffer_virtual, (void*) (data->framebuffer_virtual + data->pitch * 16), data->pitch * (16 * SCREEN_HEIGHT - 16));
        memset((void*) (data->framebuffer_virtual + data->pitch * (16 * SCREEN_HEIGHT - 16)), 0, data->pitch * 16);

    } else {
        data->cursor_y++;
    }

    vesa_update_cursor(dev);
}

static void vesa_render_character(struct vesa_data* data, char c) {
    // TODO: this assumes 24bpp

    for (int i = 0; i < 16; ++i) {
        uint8_t* position = data->framebuffer_virtual + (data->cursor_y * 16 + i) * data->pitch + data->cursor_x * 8 * 3;
        uint8_t font_char = terminal_font[((uint8_t) c) * 16 + i];

        for (int j = 0; j < 8; ++j) {
            uint32_t colour = (font_char & 0x80) ? data->fg_colour : data->bg_colour;
            font_char <<= 1;

            *position++ = (colour >> 0) & 0xFF;
            *position++ = (colour >> 8) & 0xFF;
            *position++ = (colour >> 16) & 0xFF;
        }
    }
}

static int vesa_putpixel(struct video_device_interface* dev, int x, int y, uint32_t colour) {
    struct vesa_data* data = dev->data;

    if (x < 0 || x >= data->width || y < 0 || y >= data->height) {
        return EINVAL;
    }

    uint8_t* position = data->framebuffer_virtual + y * data->pitch + x * 3;

    *position++ = (colour >> 0) & 0xFF;
    *position++ = (colour >> 8) & 0xFF;
    *position++ = (colour >> 16) & 0xFF;

    return 0;
}

static int vesa_putline(struct video_device_interface* dev, int x, int y, int width, uint32_t colour) {
    struct vesa_data* data = dev->data;

    if (x < 0 || x >= data->width || y < 0 || y >= data->height) {
        return EINVAL;
    }

    if (x + width > data->width) {
        width = data->width - x;
    }

    uint8_t* position = data->framebuffer_virtual + y * data->pitch + x * 3;

    for (int i = 0; i < width; ++i) {
        *position++ = (colour >> 0) & 0xFF;
        *position++ = (colour >> 8) & 0xFF;
        *position++ = (colour >> 16) & 0xFF;
    }

    return 0;
}

static int vesa_putrect(struct video_device_interface* dev, int x, int y, int width, int height, uint32_t colour) {
    struct vesa_data* data = dev->data;

    printf("VESA_PUTRECT\n");

    if (x < 0 || x >= data->width || y < 0 || y >= data->height) {
        return EINVAL;
    }

    if (x + width > data->width) {
        width = data->width - x;
    }

    if (y + height > data->height) {
        height = data->height - y;
    }

    for (int i = 0; i < height; ++i) {
        vesa_putline(dev, x, y + i, width, colour);
    }

    return 0;
}

/*
* Writes a character to the current cursor position and increments the 
* cursor location. Can handle newlines and the edges of the screen.
*/
static void vesa_putchar(struct console_device_interface* dev, char c) {
    struct vesa_data* data = dev->data;

    if (c == '\n') {
        vesa_newline(dev);
        return;
    }

    if (c == '\r') {
        data->cursor_x = 0;
        vesa_update_cursor(dev);
        return;
    }

    if (c == '\b') {
        /*
        * Needs to be able to backspace past the beginning of the line (i.e. when
        * you write a line of the terminal that goes across multiple lines, then you
        * delete it).
        */
        if (data->cursor_x > 0) {
            data->cursor_x--;
            vesa_update_cursor(dev);
        } else {
            data->cursor_x = SCREEN_WIDTH - 1;
            data->cursor_y--;
            vesa_update_cursor(dev);
        }
        return;
    }

    if (c == '\t') {
        /*
        * Add at least one space, then round up to the nearest 8th column. We use
        * spaces in case we need to overwrite something after a '\r'
        * 
        * TODO: has a bug! backspacing it won't work!
        */
        vesa_putchar(dev, ' ');
        while (data->cursor_x % 8 != 0) {
            vesa_putchar(dev, ' ');
        }
        return;
    }

    vesa_render_character(data, c);

    data->cursor_x++;

    if (data->cursor_x == SCREEN_WIDTH) {
        vesa_newline(dev);
    } else {
        vesa_update_cursor(dev);
    }
}

static void vesa_panic(const char* message) {
    arch_disable_interrupts();

    (void) message;
}

/*
* Set up the hardware, and install the device into the virtual filesystem.
*/
void _driver_entry_point(void) {
    /*
    * Static means the reference is not invalid upon returning.
    */
    static struct console_device_interface dev;
    static struct video_device_interface video;

    extern uint32_t vesa_framebuffer;
    extern uint16_t vesa_pitch;
    extern uint16_t vesa_height;
    extern uint16_t vesa_width;
    extern uint8_t vesa_depth;

    struct vesa_data* data = malloc(sizeof(struct vesa_data));
    data->cursor_x = 0;
    data->cursor_y = 0;
    data->fg_colour = 0xC0C0C0;
    data->bg_colour = 0x000000;
    data->pitch = vesa_pitch;
    data->width = vesa_width;
    data->height = vesa_height;
    data->depth_in_bits = vesa_depth;
    data->framebuffer_physical = vesa_framebuffer;
    
    SCREEN_WIDTH = data->width / 8;
    SCREEN_HEIGHT = data->height / 16;

    size_t pages_needed = virt_bytes_to_pages(data->pitch * data->height);

    data->framebuffer_virtual = (uint8_t*) virt_allocate_unbacked_krnl_region(pages_needed * ARCH_PAGE_SIZE);

    for (size_t i = 0; i < pages_needed; ++i) {
        vas_map(vas_get_current_vas(), data->framebuffer_physical + i * ARCH_PAGE_SIZE, (size_t) data->framebuffer_virtual + i * ARCH_PAGE_SIZE, VAS_FLAG_PRESENT | VAS_FLAG_LOCKED);
    }

    dev.data = data;
    dev.putc = vesa_putchar;
    dev.panic = vesa_panic;

    /*
    * Clear the screen.
    */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
        vesa_putchar(&dev, ' ');
    }
    data->cursor_x = 0;
    data->cursor_y = 0;
    vesa_update_cursor(&dev);

    console_register_driver(&dev);

    /*
    * Now register the direct access driver.
    */
    video.data = data;
    video.putpixel = vesa_putpixel;
    video.putline = vesa_putline;
    video.putrect = vesa_putrect;
    video_register_driver(&video);
}