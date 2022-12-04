
#include <stdint.h>
#include <beeper.h>
#include <machine/beeper.h>
#include <machine/portio.h>

/*
* x86/dev/beeper.c - PC Speaker
*
* A driver for the PC speaker, a generic beeping device found on nearly all
* x86 computers. It is a very simple device to program, merely requiring a
* timer channel to be configured to the right frequency, and then the speaker
* can just be turned on or off.
*
*/

#define DATA_PORT		0x42
#define COMMAND_PORT	0x43
#define CONTROL_PORT	0x61

/*
* Turn on the PC speaker at a desired frequency
*/
static void x86_beeper_start(int freq)
{
	if (freq <= 0) {
		return;
	}
	
	/*
	* Set the PIT to the correct frequency (it runs at 1193180 Hz)
	*/
	uint16_t divisor = 1193180 / freq;
	outb(COMMAND_PORT, 0xB6);
	outb(DATA_PORT, divisor & 0xFF);
	outb(DATA_PORT, divisor >> 8);

	/*
	* Turn on the speaker itself. For *historical reasons* the enable bit
	* is located on the keyboard controller.
	*/
	uint8_t tmp = inb(CONTROL_PORT);
	outb(CONTROL_PORT, tmp | 3);
}

/*
* Turn off the speaker.
*/
static void x86_beeper_stop(void) 
{
	uint8_t tmp = inb(CONTROL_PORT) & 0xFC;
	outb(CONTROL_PORT, tmp);
}

/*
* Register it as the beeper driver so the kernel beeper functions can be used.
*/
void x86_beeper_register(void)
{
	/*
	* Pointers to static objects remain valid even after the function return
	*/
	static struct beeper_device_interface dev;
	
	dev.start = x86_beeper_start;
	dev.stop = x86_beeper_stop;
	beeper_register_driver(&dev);
}