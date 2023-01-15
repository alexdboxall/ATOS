#include <vfs.h>
#include <vnode.h>
#include <errno.h>
#include <device.h>
#include <pipe.h>
#include <heap.h>
#include <adt.h>
#include <fcntl.h>

struct pipe {
    struct adt_blocking_byte_buffer* buffer;
    int flags;
};

static int pipe_io(struct std_device_interface* dev, struct uio* io) {
    struct pipe* pipe = dev->data;
    struct adt_blocking_byte_buffer* buffer = pipe->buffer;

    if (io->direction == UIO_READ) {
        bool first_iteration = true;
        while (io->length_remaining > 0) {
            /*
            * Wait for a character to enter the buffer.
            */
            uint8_t c;
            if (first_iteration) {
                c = adt_blocking_byte_buffer_get(buffer);
                first_iteration = false;

            }  else {
                int status = adt_blocking_byte_buffer_try_get(buffer, &c);

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
        while (io->length_remaining > 0) {
            uint8_t c;
            int status = uio_move(&c, io, 1);
            if (status != 0) {
                return status;
            }

            adt_blocking_byte_buffer_add(buffer, c);
        }

        return 0;
    }
}

struct std_device_interface* pipe_initialise_internal(int(*io_func)(struct std_device_interface*, struct uio*)) {
    struct std_device_interface* dev = malloc(sizeof(struct std_device_interface));
    dev->termios = NULL;
    dev->check_open = interface_check_open_not_needed;
    dev->ioctl = interface_ioctl_not_needed;
    dev->io = io_func;
    dev->data = NULL;

    return dev;
}

int pipe_create(struct vnode** out, int flags) {
    if (out == NULL) {
        return EINVAL;
    }

    /*
    * We don't support the O_DIRECT flag.
    */
    if (flags & O_DIRECT) {
        return EINVAL;
    }

    /*
    * Check for flags that aren't supposed to passed in.
    */
    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        return EINVAL;
    }

    struct pipe* pipe_data = malloc(sizeof(struct pipe));
    pipe_data->flags = flags;
    pipe_data->buffer = adt_blocking_byte_buffer_create(4096);

    struct std_device_interface* data = pipe_initialise_internal(pipe_io);
    *out = dev_create_vnode(data);

    if (flags & O_NONBLOCK) {
        (*out)->flags |= O_NONBLOCK;
    }

    return 0;
}