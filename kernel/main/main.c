
#include <arch.h>
#include <physical.h>
#include <virtual.h>
#include <heap.h>
#include <sys/stat.h>
#include <string.h>
#include <assert.h>
#include <kprintf.h>
#include <process.h>
#include <uio.h>
#include <cpu.h>
#include <vfs.h>
#include <video.h>
#include <fcntl.h>
#include <loader.h>
#include <device.h>
#include <console.h>
#include <process.h>
#include <syscall.h>
#include <test.h>
#include <termios.h>
#include <thread.h>
#include <fs/demofs/demofs.h>

void basic_shell(void* arg) {
	(void) arg;

	kprintf("\n ATOS Kernel\n     Copyright Alex Boxall 2022-2023\n\n");

	/*
	* Just launch into a very basic command line shell.
	*/
	while (1) {
        kprintf(">");

		char buffer[256];
        console_gets(buffer, sizeof(buffer));

		/*
		* Must check for an empty string to guard the following 'strlen() - 1'
		* in case some clown decides to type the ^@ escape code (a null char
        * without a newline).
		*/
		if (buffer[0] == '\n' || buffer[0] == 0) {
			continue;
		}

		/*
		* Strip the final newline if it exists.
		*/
		if (buffer[strlen(buffer) - 1] == '\n') {
			buffer[strlen(buffer) - 1] = '\0';
		}

		if (!strncmp(buffer, "test", 4)) {
#ifdef NDEBUG
			kprintf("Tests are not included in release builds.\n");
#else
			if (strlen(buffer) > 5) {
				test_run(buffer + 5);
			} else {
				kprintf("Missing test name (e.g. 'test sleep')\n");
			}
#endif
			kprintf("\n");
			continue;

		} else if (!strncmp(buffer, "type", 4)) {
			if (strlen(buffer) > 5) {
				char* filename = buffer + 5;
				struct vnode* node;
				int ret = vfs_open(filename, O_RDONLY, 0, &node);
				if (ret != 0) {
					kprintf("Cannot open: %s\n", strerror(ret));
				} else {
					struct stat st;
					ret = vnode_op_stat(node, &st);
					if (ret == 0) {
						uint8_t* buffer = malloc(st.st_size + 1);
						memset(buffer, 0, st.st_size + 1);

						struct uio uio = uio_construct_kernel_read(buffer, st.st_size, 0);
						int ret = vfs_read(node, &uio);
						if (ret != 0) {
							kprintf("Cannot read: %s\n", strerror(ret));
						} else {
							kprintf("%s", buffer);
						}

						free(buffer);

					} else {
						kprintf("Cannot stat: %s\n", strerror(ret));
					}
					
					vfs_close(node);
				}
			} else {
				kprintf("Missing file name\n");
			}
			kprintf("\n");
			continue;

        } else if (!strcmp(buffer, "fork")) {
            int res = thread_fork();

            while (true) {
                kprintf("%d\n", res);
                thread_sleep(1);
            }

        } else if (!strcmp(buffer, "user")) {
            struct process* p = process_create();
            process_create_thread(p, thread_execute_in_usermode, NULL);
            continue;

		} else if (!strcmp(buffer, "restart")) {
			arch_reboot();
			continue;

		} else if (!strcmp(buffer, "ram")) {
			// TODO: remove this as it isn't portable (or maintainable with 'extern'), this is just for our own use
			extern int num_pages_used;
			extern int num_pages_total;

			int percent = num_pages_used * 100 / num_pages_total;
			kprintf("Memory used: %d%% (%d / %d KB)\n\n", percent, num_pages_used * 4, num_pages_total * 4);
			continue;

		} else if (!strcmp(buffer, "gui")) {
            load_driver("sys:/clipdraw.sys", false);
            continue;
        }
		
		kprintf("Unsupported command '%s'\n\n", buffer);
	}
}

/*
* Our main kernel function - the first C function to be called when the
* kernel starts running. It responsible for initialising all of the other 
* subsystems and threads.
*/
void kernel_main()
{
	/*
	* The order in which subsystems are initialised in is crucial. Systems should
	* be initialised in order of least dependent on other systems to most dependent.
	*/

    kprintf("\nkernel_main\n");
	phys_init();
	virt_init();
    
	heap_init();
	cpu_init();
    heap_reinit();
    thread_init();  
    process_init();
    vfs_init();
	interface_init();
    video_init();

    arch_initialise_devices_no_fs();

	vfs_mount_filesystem("hd0", demofs_root_creator);
    vfs_add_virtual_mount_point("sys", "hd0:/System");
    swapfile_init();
    syscall_init();

    arch_initialise_devices_with_fs();

#ifndef NDEBUG
	test_kernel();
#endif

    process_create_thread(process_create(), basic_shell, NULL);

	/*
	* We ought not run anything else in this bootstrap thread,
	* as it (may) have a different stack to the rest (e.g. on x86, it is
	* a 16KB stack with no canary protection). We won't terminate it though,
    * again due to its (possibly) unique stack.
	*/
	while (1) {
		spinlock_acquire(&scheduler_lock);
		thread_block(THREAD_STATE_UNINTERRUPTIBLE);
		spinlock_release(&scheduler_lock);
	}
}