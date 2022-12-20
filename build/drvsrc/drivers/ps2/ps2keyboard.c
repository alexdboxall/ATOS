
#include <common.h>
#include <console.h>
#include <kprintf.h>
#include <string.h>
#include <errno.h>
#include <machine/pic.h>
#include <machine/ps2keyboard.h>
#include <machine/portio.h>
#include <machine/interrupt.h>

/*
* x86/dev/ps2keyboard.c - PS/2 Keyboard
*
* Basic support for PS/2 keyboards. Although USB keyboards have entirely
* replaced PS/2, the firmware will set up PS/2 emulation so USB keyboards
* will act like a PS/2 keyboard. (This emulation works for mice too.)
* This is useful as PS/2 is significantly simpler to program for than USB. 
*
* Ideally we would want the PS/2 controller and PS/2 keyboard drivers to
* be seperate, but we are not going to implement support for other PS/2
* devices (such as mice). Therefore, we will just put in all in here.

* The PS/2 keyboard sends scancodes every time a key is pressed or release, 
* which represents whether it was a press or release, and which key it was.
* For these to be useful to us, they need to be translated to device-independent
* key code. This OS uses ASCII characters, with certain special keys represented
* by a character in the higher half of the ASCII table. (see keycodes.h)
*
* There are two standard commonly used sets of scancodes. The PS/2 keyboard should
* default to scancode set 2, but with a special mode enabled which converts them to
* scancode set 1 (for backwards compatibility). To get the set 2 scancodes, we need
* to disable this translation. Unfortunately, this is sometimes unreliable. 
* Therefore, we will just deal with both sets of scancodes.
*
* Note that scancodes may be multiple bytes long. For example, in scancode set 2
* a key release scancode will be the same as a key press scancode, but with an
* additional byte (0xF0) at the start. For *historical reasons* the scancode for 
* the pause key is 6 bytes long in set 1, and 8 bytes long in set 2!
*/


/*
* Lookup tables for translating scancodes to ASCII.
*
* We are going to use 8 tables to perform the translation, for each one of the
* three boolean conditions:
*
*   - shift is held ( ...upper...)
*   - CAPS is on    ( ...caps...)
*   - scancode set in use
*
* These lookup tables don't deal with special keys (e.g. backspace, F1...F12, etc.),
* so we will need to catch these and return a special value before the lookup is done.
*
* For a full table of the scancodes, see the below link. Gawk in horror at the
* scancode for the pause key!
*
*       https://web.archive.org/web/20220611172440/https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Sets
*
*/

static const char set1_map_lower_norm[] = "  1234567890-=  qwertyuiop[]  asdfghjkl;'` \\zxcvbnm,./ *               789-456+1230.                                            ";
static const char set1_map_upper_norm[] = "  !@#$%^&*()_+  QWERTYUIOP{}  ASDFGHJKL:\"~ |ZXCVBNM<>? *               789-456+1230.                                            ";
static const char set1_map_lower_caps[] = "  1234567890-=  QWERTYUIOP[]  ASDFGHJKL;'` \\ZXCVBNM,./ *               789-456+1230.                                            ";
static const char set1_map_upper_caps[] = "  !@#$%^&*()_+  qwertyuiop{}  asdfghjkl:\"~ |zxcvbnm<>? *               789-456+1230.                                            ";

#if 0
static const char set2_map_lower_norm[] = "              `      q1   zsaw2  cxde43   vftr5  nbhgy6   mju78  ,kio09  ./l;p-   ' [=     ] \\           1 47   0.2568   +3-*9             -";
static const char set2_map_upper_norm[] = "              ~      Q!   ZSAW@  CXDE$#   VFTR%  NBHGY^   MJU&*  <KIO)(  >?L:P_   \" {+     } |           1 47   0.2568   +3-*9              ";
static const char set2_map_lower_caps[] = "              `      Q1   ZSAW2  CXDE43   VFTR5  NBHGY6   MJU78  ,KIO09  ./L;P-   ' [=     ] \\           1 47   0.2568   +3-*9             -";
static const char set2_map_upper_caps[] = "              ~      Q!   zsaw@  cxde$#   vftr%  nbhgy^   mju&*  <kio)(  >?l:p_   \" {+     } |           1 47   0.2568   +3-*9              ";
#endif

/*
* Special keys that we might want to catch that aren't in the lookup tables.
* Some have ASCII equivalents (e.g. tab), whereas others we will just define
* an arbitrary value to.
*
* TODO: these should be common to all keyboards, and thus not defined here.
*
*/
#define KEYCODE_ENTER           '\n'
#define KEYCODE_BACKSPACE       '\b'
#define KEYCODE_TAB             '\t'
#define KEYCODE_ESCAPE          '\x1B'
#define KEYCODE_LEFT_ARROW      128
#define KEYCODE_RIGHT_ARROW     129

/*
* The scancodes for those special keys.
*/
#define SET1_ENTER          0x1C
#define SET1_BACKSPACE      0x0E
#define SET1_TAB            0x0F
#define SET1_ESCAPE         0x01
#define SET1_SHIFT          0x2A
#define SET1_SHIFT_R        0x36
#define SET1_CTRL           0x1D
#define SET1_EXTENSION      0xE0
#define SET1_CAPS_LOCK      0x3A

/*
* Doesn't need locking (I hope...) as it is only accessed in the keyboard
* interrupt handler, and we can't get a second keyboard interrupt while the
* first one is processing as EOI wouldn't have been sent yet.
*/
bool release_mode = false;
bool control_held = false;
bool shift_held = false;
bool shift_r_held = false;
bool caps_lock_on = false;

#define PS2_STATUS_BIT_OUT_FULL		1
#define PS2_STATUS_BIT_IN_FULL		2
#define PS2_STATUS_BIT_SYSFLAG		4
#define PS2_STATUS_BIT_CONTROLLER	8
#define PS2_STATUS_BIT_TIMEOUT		64
#define PS2_STATUS_BIT_PARITY		128

static int ps2_wait(bool writing) {
    int timeout = 0;
    while (true) {
        uint8_t status = inb(0x64);

        if (writing) {
            if (!(status & PS2_STATUS_BIT_IN_FULL)) {
                break;
            }
        } else {
            if (status & PS2_STATUS_BIT_OUT_FULL) {
                break;
            }
        }

        if (++timeout >= 2000 || (status & PS2_STATUS_BIT_TIMEOUT) || (status & PS2_STATUS_BIT_PARITY)) {
            return EIO;
        }
    } 

    return 0;
}


/*
* For all of these, we are going to ignore error checking and proceed
* regardless, as the error detection can be a bit unreliable on real
* hardware and between different emulators. (This is thanks to the
* various quality of USB to PS/2 emulation implementations.)
*/
static void ps2_controller_write(uint8_t data) {
    outb(0x64, data);
}

static void ps2_controller_write_with_arg(uint8_t data, uint8_t arg) {
    ps2_controller_write(data);
    ps2_wait(true);
    outb(0x60, arg);
}

static uint8_t ps2_controller_read(void) {
    ps2_wait(false);
    return inb(0x60);
}

static void ps2_device_write(uint8_t data, bool port2) {
    if (port2) {
        ps2_controller_write_with_arg(0x64, 0xD4);
    }
    ps2_wait(true);
    outb(0x60, data);
}

static uint8_t ps2_device_read(void) {
    return ps2_controller_read();
}

static void ps2_set_leds(void) {
    uint8_t data = caps_lock_on << 2;

    ps2_device_write(0xED, false);
    ps2_device_read();
    ps2_device_write(data, false);
    ps2_device_read();
}

static void ps2_translate_set_1(uint8_t scancode) {
    if (scancode & 0x80) {
        release_mode = true;
        scancode &= 0x7F;
    }

    uint8_t received_character = 0;
    switch (scancode) {
    case SET1_ENTER:
        received_character = KEYCODE_ENTER;
        break;

    case SET1_BACKSPACE:
        received_character = KEYCODE_BACKSPACE;
        break;

    case SET1_TAB:
        received_character = KEYCODE_TAB;
        break;

    case SET1_ESCAPE:
        received_character = KEYCODE_ESCAPE;
        break;

    case SET1_SHIFT:
        shift_held = !release_mode;
        break;

    case SET1_SHIFT_R:
        shift_r_held = !release_mode;
        break;

    case SET1_CTRL:
        control_held = !release_mode;
        break;

    case SET1_CAPS_LOCK:
        if (!release_mode) {
            caps_lock_on = !caps_lock_on;
            ps2_set_leds();
        }
        break;

    default:
        if (scancode < strlen(set1_map_lower_norm)) {
            bool shift = shift_held ^ shift_r_held;

            if (caps_lock_on) {
                if (shift) {
                    received_character = set1_map_upper_caps[scancode];
                } else {
                    received_character = set1_map_lower_caps[scancode];
                }
            } else {
                if (shift) {
                    received_character = set1_map_upper_norm[scancode];
                } else {
                    received_character = set1_map_lower_norm[scancode];
                }
            }
        }
    }

    if (received_character != 0 && !release_mode) {
        console_received_character(received_character, control_held);
    }

    release_mode = false;
}

/*
static void ps2_translate_set_2(uint8_t scancode) {
    if (scancode == 0xF0) {
        next_is_release = true;
        return;
    }

    // ...

    // we return early on a release scancode, so we can put this here
    next_is_release = false;
}
*/


static int ps2_handler(struct x86_regs* r) {
    (void) r;

	uint8_t scancode = inb(0x60);

    ps2_translate_set_1(scancode);

    return 0;
}

    
void _driver_entry_point(void) {
    /*
    * Follow the mantra: 
    *
    *   "assume the BIOS set it up correctly, and don't bother initialising it yourself"
    * 
    * This is a bad idea, but as we only need the keyboard to work, the BIOS should
    * have initialised at least that much. (Trusting the BIOS for the mouse is a bit iffier).
    */
    x86_register_interrupt_handler(PIC_IRQ_BASE + 1, ps2_handler);
}