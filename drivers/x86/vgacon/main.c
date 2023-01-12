
#include <stdint.h>
#include <beeper.h>
#include <machine/mem/virtual.h>
#include <machine/portio.h>
#include <machine/console.h>
#include <arch.h>
#include <string.h>
#include <spinlock.h>
#include <console.h>
#include <heap.h>
#include <errno.h>
#include <vfs.h>
#include <kprintf.h>
#include <device.h>
#include <assert.h>

/*
* VGA Terminal Driver for x86 Systems
* 
* A console driver for VGA text mode. Supports 80 columns and 25 columns
* of ASCII text. It only supports writing bytes to the terminal in sequence.
*
*
* Possible extension: an ANSI control code parser so that the cursor
*                     position can adjusted manually, and to display
*                     other characters
*
*/

#define SCREEN_WIDTH  80
#define SCREEN_HEIGHT 25

/*
* Additional data that is stored within the device interface.
*/
struct vga_data {
    int cursor_x;
    int cursor_y;
    uint16_t colour;
};

/*
* Converts a device's cursor X and Y positions into a single offset
* from the top left corner of the screen.
*/
static int get_cursor_offset(struct console_device_interface* dev) {
    struct vga_data* data = dev->data;
    return data->cursor_x + data->cursor_y * SCREEN_WIDTH;
}


/*
* Update the position of the hardware cursor (that little blinking
* underline where the cursor is).
*/
static void vga_update_cursor(struct console_device_interface* dev) {
   
    uint16_t pos = get_cursor_offset(dev);

    /*
    * It is a hardware feature, so send the data to the VGA controller
    */
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, pos >> 8);
}


/*
* Moves the cursor position to a newline, handling the case where
* it reaches the bottom of the terminal
*/
static void vga_newline(struct console_device_interface* dev) {
    struct vga_data* data = dev->data;

    data->cursor_x = 0;

    assert(data->cursor_y < SCREEN_HEIGHT);

    if (data->cursor_y == SCREEN_HEIGHT - 1) {
        /*
        * Keep the cursor on the final line, but scroll the terminal
        * back and clear the final line.
        */

        uint16_t* video_ptr = (uint16_t*) lowmem_physical_to_virtual(0xB8000);
        memmove(video_ptr, video_ptr + SCREEN_WIDTH, 2 * SCREEN_WIDTH * (SCREEN_HEIGHT - 1));
        memset(video_ptr + SCREEN_WIDTH * (SCREEN_HEIGHT - 1), 0, 2 * SCREEN_WIDTH);

    } else {
        data->cursor_y++;
    }

    vga_update_cursor(dev);
}

/*
* Writes a character to the current cursor position and increments the 
* cursor location. Can handle newlines and the edges of the screen.
*/
static void vga_putchar(struct console_device_interface* dev, char c) {
    struct vga_data* data = dev->data;

    if (c == '\n') {
        vga_newline(dev);
        return;
    }

    if (c == '\r') {
        data->cursor_x = 0;
        vga_update_cursor(dev);
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
            vga_update_cursor(dev);
        } else {
            data->cursor_x = SCREEN_WIDTH - 1;
            data->cursor_y--;
            vga_update_cursor(dev);
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
        vga_putchar(dev, ' ');
        while (data->cursor_x % 8 != 0) {
            vga_putchar(dev, ' ');
        }
        return;
    }

    /*
    * Character video memory starts at physical address 0xB8000, and we
    * need to get the correct position by adding the cursor position to it.
    */
    uint16_t* video_ptr = (uint16_t*) lowmem_physical_to_virtual(0xB8000);
    video_ptr += get_cursor_offset(dev);

    /*
    * Video data uses 2 bytes per character, in the following format:

    * 0xABCD
    *   ^^^^
    *   ||||
    *   ||\\= ASCII value of the character
    *   || 
    *   |\--- foreground colour
    *   |
    *   \---- background colour
    * 
    * The colour values (each are stored in one nibble, ie. 4 bits)
    * 
    * 0 = black             8 = dark grey
    * 1 = blue              9 = light blue
    * 2 = green             A = light green
    * 3 = cyan              B = light cyan
    * 4 = red               C = light red
    * 5 = magenta           D = light magenta
    * 6 = brown/olive       E = yellow
    * 7 = light grey        F = white
    * 
    * For example, 0x3461 is a red lowercase 'a' on a cyan background.
    * 
    * We will use light grey text on a black background by default.
    */
    *video_ptr = data->colour | (int) c;

    data->cursor_x++;

    if (data->cursor_x == SCREEN_WIDTH) {
        vga_newline(dev);
    } else {
        vga_update_cursor(dev);
    }
}

static void vga_panic(const char* message) {
    arch_disable_interrupts();

    (void) message;

    return;
    #if 0

    /*
    * Make the entire screen blue.
    */
    uint16_t* video_ptr = (uint16_t*) lowmem_physical_to_virtual(0xB8000);
    for (int i = 0; i < SCREEN_HEIGHT * SCREEN_WIDTH; ++i) {
        *video_ptr++ = 0x1100;
    }

    struct console_device_interface dev;
    struct vga_data data;
    dev.data = &data;
    
    /*
    * Display the "kernel panic" heading, as well as the reason for the error.
    */
    const char* title = "  Kernel Panic  ";
    data.cursor_x = 4;
    data.cursor_y = 2;
    data.colour = 0xF100;           // blue on white background
    for (int i = 0; title[i]; ++i) {
        vga_putchar(&dev, title[i]);
    }

    data.cursor_x = 4;
    data.cursor_y = 4;
    data.colour = 0x1F00;           // white on blue background
    for (int i = 0; message[i]; ++i) {
        vga_putchar(&dev, message[i]);
    }

    /*
    * Because the background is blue on blue, moving the cursor to there makes it
    * invisible.
    */
    data.cursor_x = 0;
    data.cursor_y = 0;
    vga_update_cursor(&dev);
    #endif
}

/*
* Set up the hardware, and install the device into the virtual filesystem.
*/
void _driver_entry_point(void) {
    /*
    * Static means the reference is not invalid upon returning.
    */
    static struct console_device_interface dev;

    struct vga_data* data = malloc(sizeof(struct vga_data));
    data->cursor_x = 0;
    data->cursor_y = 0;
    data->colour = 0x0700;      // light grey text on black background
    dev.data = data;
    dev.putc = vga_putchar;
    dev.panic = vga_panic;

    /*
    * Some video cards might treat the high bit of the background colour as a
    * 'blink flag' by default, so we want to turn this off so we can use all of
    * the colours.
    */
    inb(0x3DA);
    outb(0x3C0, 0x30);
    outb(0x3C0, inb(0x3C1) & 0xF7);

    /*
    * Clear the screen.
    */
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
        vga_putchar(&dev, ' ');
    }
    data->cursor_x = 0;
    data->cursor_y = 0;
    vga_update_cursor(&dev);

    console_register_driver(&dev);    
}