
#include <interface.h>
#include <spinlock.h>
#include <device.h>
#include <kprintf.h>
#include <errno.h>
#include <console.h>
#include <panic.h>
#include <string.h>
#include <synch.h>
#include <adt.h>
#include <heap.h>
#include <termios.h>
#include <beeper.h>
#include <cpu.h>


/*
* dev/console.c - Generic Console
*
* A console supporting reading and writing characters to it. The console has a line buffer
* which is flushed whenever a newline (or ^C) is encountered.
*
* Control codes (such as ^C, ^D, etc) are handled by the console and display as expected.
*
* There is going to be a problem with backspace if you get into a situation where
* a thread is reading from the console at the same time another thread is writing to it.
* This should be fixable by ensuring writes go into a seperate buffer if the line
* buffer has any characters in it, and then flushing it as soon as the line buffer empties.  
*/

#define INPUT_BUFFER_SIZE   1024
#define LINE_BUFFER_SIZE    1024

struct console_device_interface* default_console_driver = NULL;
struct spinlock console_driver_lock;
struct adt_blocking_byte_buffer* input_buffer = NULL;

char line_buffer[INPUT_BUFFER_SIZE];
uint8_t line_buffer_char_width[INPUT_BUFFER_SIZE];
int line_buffer_pos = 0;

static void console_add_to_line_buffer(char c, int width) {
    if (line_buffer_pos == INPUT_BUFFER_SIZE) {
        panic("TODO: console_add_to_line_buffer: line buffer is full");
    }

    line_buffer[line_buffer_pos] = c;
    line_buffer_char_width[line_buffer_pos] = width;
    line_buffer_pos++;
}

static void console_flush_line_buffer(void) {
    /*
    * Flush the line buffer.
    * For *reasons* we shouldn't do any important cleanup after the last character.
    * (e.g. like setting line_buffer_pos to zero)
    * 
    * (it's because that causes the main thread to start running again, causing
    *  it to block for keyboard. that causes a task switch which *may* go to us, in
    *  which case we can cleanup. but if it does to another thread that doesn't yield
    *  control, the preempter won't have anything to go to because the other thread
    *  is blocking for keyboard. as we are actually part of that thread that is blocking
    *  (just in the keyboard handler, the cleanup won't run until after the next line flush,
    *  which causes a double input of the inputted line)
    */

    int line_buffer_pos_copy = line_buffer_pos;

    line_buffer_pos = 0;

    for (int i = 0; i < line_buffer_pos_copy; i++) {
        adt_blocking_byte_buffer_add(input_buffer, line_buffer[i]);
    }
}


static void console_beep_thread(void* arg) {
    (void) arg;

    /*
    * Play A4 (440Hz) for half a second. Note that if a second beep were to start
    * before the first one finishes, the second beep will be cut off.
    */
    beeper_start(440);
    thread_nano_sleep(500 * 1000 * 1000);
    beeper_stop();
    thread_terminate();
}

static void console_make_beep(void) {
    /*
    * We need to create a new thread for this so that it can beep and sleep without
    * blocking the rest of the console.
    */
    thread_create(console_beep_thread, NULL, current_cpu->current_vas);
}



/*
* Only to be called by hardware devices (e.g. keyboard, serial port, etc.)
*/
void console_received_character(char c, bool control_held) {
    char converted_character = c;

    /*
    * When CTRL is held while pressing keys in the ASCII range from @ to _ terminals interpret it as
    * being a control code, mapping to the early ASCII values. Also accept lowercase, as this is what
    * most people would actually type.
    */
    bool is_control_code = control_held && ((c >= '@' && c <= '_') || (c >= 'a' && c <= 'z'));
    if (is_control_code) {
        /*
        * Convert to uppercase so it falls into the correct ASCII range.
        */
        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }

        /*
        * Write the converted character to the buffer. Don't modify the variable 'c', we still
        * need to know the original value later when we print a representation of the character
        * to the screen.
        */
        converted_character = c - '@';
    }        

    if (converted_character == '\b') {
        if (line_buffer_pos == 0) {
            /*
            * Nothing to delete.
            */
            return;
        }

        line_buffer_pos--;

        for (int i = 0; i < line_buffer_char_width[line_buffer_pos]; ++i) {
            /*
            * Erase the current character ("\b "), and then move it back again.
            * Control codes require two characters to be erased from the screen.
            */
            console_putc('\b');
            console_putc(' ');
            console_putc('\b');
        }

    } else if (converted_character == '\a') {
        /*
        * ^G is meant to create an audible alert.
        */
        console_make_beep();

    } else if (control_held) {
        /*
        * Show the control code with a caret before it, so Ctrl+C is shown as ^C
        * as you would expect. Will never show a lowercase character, as we converted
        * them to uppercase above.
        */
        if (is_control_code) {
            console_putc('^');
            console_putc(c);

            console_add_to_line_buffer(converted_character, 2);

        } else {
            /*
            * We don't print at all when control is held but it doesn't make a control code.
            */
        }
        
    } else {
        console_putc(c);
        console_add_to_line_buffer(converted_character, 1);
    }
    
    /*
    * ASCII 3 is the same as ^C, which we want to immediately flush so the application can
    * process it. 
    */
    if (converted_character == '\n' || converted_character == 3) {
        console_flush_line_buffer();
    }
}

int console_register_driver(struct console_device_interface* driver) 
{
    if (driver == NULL) {
        return ENODEV;
    }
    
    spinlock_acquire(&console_driver_lock);
    
    if (default_console_driver != NULL) {
        spinlock_release(&console_driver_lock);
        return EALREADY;
        
    } else {
        default_console_driver = driver;
        spinlock_release(&console_driver_lock);
        return 0;
    }
}

static int console_io(struct std_device_interface* dev, struct uio* io) {
    (void) dev;

    if (io->direction == UIO_READ) {
        if (input_buffer == NULL) {
            return ENODEV;
        }

        bool first_iteration = true;
        while (io->length_remaining > 0) {
            /*
            * Wait for a character to enter the buffer.
            */
            uint8_t c;
            if (first_iteration) {
                c = adt_blocking_byte_buffer_get(input_buffer);
                first_iteration = false;

            }  else {
                int status = adt_blocking_byte_buffer_try_get(input_buffer, &c);

                /*
                * It's not an error to not get any more characters.
                */
                if (status != 0) {
                    break;
                }
            }
            
            int status = uio_move(&c, io, 1);
            if (status != 0) {
                return status;
            }
        }
        
        return 0;

    } else {  
        spinlock_acquire(&console_driver_lock);
        if (default_console_driver == NULL || default_console_driver->putc == NULL) {
            spinlock_release(&console_driver_lock);
            return ENODEV;
        }

        while (io->length_remaining > 0) {
            char character;
            int err = uio_move(&character, io, 1);
            if (err) {
                spinlock_release(&console_driver_lock);
                return err;
            }

            default_console_driver->putc(default_console_driver, character);
        }
    
        spinlock_release(&console_driver_lock);
        return 0;
    }
}

void console_init(void) {
    /*
    * Static means the reference is not invalid upon returning.
    */
    static struct std_device_interface dev;

    spinlock_init(&console_driver_lock, "console driver lock");

    dev.data = NULL;
    dev.block_size = 0;
    dev.num_blocks = 0;
    dev.io = console_io;
    dev.check_open = interface_check_open_not_needed;
    dev.ioctl = interface_ioctl_not_needed;
    dev.termios = malloc(sizeof(struct termios));

    input_buffer = adt_blocking_byte_buffer_create(INPUT_BUFFER_SIZE);

    vfs_add_device(&dev, "con");
}

void console_panic(const char* message) {
    /*
    * No point locking at this point. If we did lock, it's possible we would
    * deadlock before showing the BSOD, which is not ideal.
    */
    default_console_driver->panic(message);
}

/*
* console_io does not use the value of the device passed in.
* Therefore we can just pass in NULL.
*/
void console_putc(char c) {
    struct uio io = uio_construct_kernel_write(&c, 1, 0);
    console_io(NULL, &io);
}

char console_getc(void) {
    char c;
    struct uio io = uio_construct_kernel_read(&c, 1, 0);

    console_io(NULL, &io);
    return c;
}

/*
* This really shouldn't live here, but there is no point having a header just for it.
*/
void console_gets(char* buffer, int size) {
    /*
    * TODO: this should be re-written to only do a blocking getc once, and then just
    * keep reading until we block (this will handle newlines, and ^C)
    * 
    * OR: this could be done in console_io (use adt_blocking_byte_buffer_get for the
    * first character, then adt_blocking_byte_buffer_try_get every other iteration)
    */

    for (int i = 0; i < size - 1; ++i) {
        char c = console_getc();

        /*
        * Ensure it is always null-terminated.
        */
        buffer[i + 1] = 0;
        buffer[i] = c;
        if (c == '\n' || c == 3) {
            break;
        }
    }
}