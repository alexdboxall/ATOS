
#include <interface.h>
#include <beeper.h>
#include <random.h>
#include <console.h>

/*
* Initialise and register the interface between devices and the VFS.
* This initialisation also involves configuring and initialising the 
* devices themselves.
*/
void interface_init() {
	console_init();
    random_init();
}


/*
* These are for devices which don't need to do anything when one is called.
* Hence we return success, as they did what they were meant to do (which is nothing). 
*/
int interface_io_not_needed(struct std_device_interface* dev, struct uio* io) {
    (void) dev;
    (void) io;

    return 0;
}

int interface_check_open_not_needed(struct std_device_interface* dev, const char* name, int flags) {
    (void) dev;
    (void) flags;
    (void) name;

    return 0;
}

int interface_ioctl_not_needed(struct std_device_interface* dev, int command, void* buffer) {
    (void) dev;
    (void) command;
    (void) buffer;

    return 0;
}