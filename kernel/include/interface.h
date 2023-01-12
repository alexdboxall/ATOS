#pragma once

/*
* dev/interface.h - Device Driver Interfaces to Kernel
* 
* 
* These interfaces are the 'glue' between the device-independent kernel, and 
* the device-specific hardware drivers.
* 
* There are a number of driver interfaces defined for different types of
* hardware, with each containing a number of function pointers. A driver can
* allocate an appropriate structure for itself, and then fill in the function
* pointers with the appropriate device-specific functions. This interface
* can then be registered using the appropriate ..._register_driver call.
* 
* The most important of these is std_device_interface, which is used for 
* all devices that are mounted to the filesystem (such as hard drives,
* CD-ROMs, USBs, serial ports, terminals, printers, etc.)
* 
*/

#include <common.h>
#include <uio.h>
#include <sys/types.h>

/*
* Interface for beepers - devices that can produce audible beeps of a given frequency.
*/
struct beeper_device_interface
{
	void (*start)(int freq);
	void (*stop)(void);
};


/*
* We don't need one for getc, as input devices just need to call console_received_character
*/
struct console_device_interface
{
	void (*putc)(struct console_device_interface*, char c);
    void (*panic)(const char* message);
	void* data;
};

struct video_device_interface
{
    void* data;
    int (*putpixel)(struct video_device_interface* dev, int x, int y, uint32_t color);
    int (*putline)(struct video_device_interface* dev, int x, int y, int width, uint32_t colour);
    int (*putrect)(struct video_device_interface* dev, int x, int y, int width, int height, uint32_t colour);
};

/*
* Interface for VFS mountable devices - supporting the standard read, write, open and ioctl
*/
struct std_device_interface
{
	int (*check_open)(struct std_device_interface* dev, const char* name, int flags);
	int (*ioctl)(struct std_device_interface* dev, int request, void* arg);
	int (*io)(struct std_device_interface* dev, struct uio* trans);

    int is_tty;
    
	blkcnt_t num_blocks;
	blksize_t block_size;

	void* data;
};

int interface_io_not_needed(struct std_device_interface* dev, struct uio* io);
int interface_check_open_not_needed(struct std_device_interface* dev, const char* name, int flags);
int interface_ioctl_not_needed(struct std_device_interface* dev, int command, void* buffer);

void interface_init(void);