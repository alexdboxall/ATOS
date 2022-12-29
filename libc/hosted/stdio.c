#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/stat.h>

#define BUFFER_DIR_UNSET 0
#define BUFFER_DIR_READ  1
#define BUFFER_DIR_WRITE 2

struct FILE {
    int fd;
    bool binary_mode;
    int ungetc;        /* EOF if there is none */
    bool eof;
    bool error;
    int buffer_mode;
    char* buffer;           
    size_t buffer_used;     
    size_t buffer_size;
    bool buffer_needs_freeing;
    int buffer_direction;
    bool been_accessed;
    volatile size_t lock_count;
    volatile int lock_owner;
};

static FILE* fopen_existing_stream(const char* filename, const char* mode, FILE* stream) {
    /*
    * This function requires that the lock is already set, and that the caller
    * will take care of the unlock. This is so freopen can be handled correctly.
    * 
    * i.e. this function doesn't touch the lock at all, so fopen will need to
    * manually initialise it
    */
    
    int flags = 0;

    if (filename == NULL || mode == NULL || stream == NULL) {
        errno = EINVAL;
        return NULL;
    }

    stream->been_accessed = false;
    stream->ungetc = EOF;
    stream->eof = false;
    stream->binary_mode = false;
    stream->error = false;
    stream->buffer = malloc(BUFSIZ);
    stream->buffer_size = BUFSIZ;
    stream->buffer_used = 0;
    stream->buffer_needs_freeing = true;
    stream->buffer_direction = BUFFER_DIR_UNSET;

    if (strlen(mode) == 0 || strlen(mode) > 3) {
        errno = EINVAL;
        return NULL;
    }

    /*
    * This works as our OS defines O_RDWR as O_RDONLY | O_WRONLY
    */
    if (mode[0] == 'r') flags |= O_RDONLY;
    else if (mode[0] == 'w') flags |= O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode[0] == 'a') flags |= O_WRONLY | O_CREAT | O_APPEND;
    else {
        errno = EINVAL;
        return NULL;
    }

    for (int i = 1; mode[i]; ++i) {
        if (mode[i] == '+') {
            flags |= O_RDWR;

        } else if (mode[i] == 'b') {
            stream->binary_mode = true;

        } else if (mode[i] == 'x') {
            /* Defined in ISO C11 */
            flags |= O_EXCL;
        }
    }

    int fd = open(filename, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd == -1) {
        /* errno is already set */
        return NULL;
    }

    /* TODO: if we are outputting (i.e. write mode) call istty, and if so, set to _IOLBF */
    stream->buffer_mode = _IOFBF;

    stream->fd = fd;
    return stream;
}

FILE* fopen(const char* filename, const char* mode) {
    FILE* stream = malloc(sizeof(FILE));

    /*
    * fopen_existing_stream does not touch the lock. We don't need to set the
    * lock, as no-one else would have access to this FILE until we return it.
    */
    stream->lock_count = 0;
    stream->lock_owner = -1;
    return fopen_existing_stream(filename, mode, stream);
}

FILE* freopen(const char* filename, const char* mode, FILE* stream) {
    flockfile(stream);
    fclose(stream);
    FILE* result = fopen_existing_stream(filename, mode, stream);
    funlockfile(stream);
    return result;
}

int fclose(FILE* stream) {
    /*
    * No need to unlock, as we are freeing the stream!!
    */
    flockfile(stream);
    fflush(stream);

    if (stream->buffer_needs_freeing) {
        free(stream->buffer);
    }

    int status = close(stream->fd);
    if (status != 0) {
        return EOF;
    }
    free(stream);
    return 0;
}

int feof(FILE* stream) {
    flockfile(stream);
    int result = stream->eof;
    funlockfile(stream);
    return result;
}

int ferror(FILE* stream) {
    flockfile(stream);
    int result = stream->error;
    funlockfile(stream);
    return result;
}

void clearerr(FILE* stream) {
    flockfile(stream);
    stream->eof = false;
    stream->error = false;
    funlockfile(stream);
}

int setvbuf(FILE* stream, char* buf, int mode, size_t size) {
    flockfile(stream);

    if (stream == NULL || stream->been_accessed) {
        errno = EBADF;
        funlockfile(stream);
        return -1;
    }

    if (stream->buffer_needs_freeing) {
        free(stream->buffer);
    }

    stream->buffer_needs_freeing = buf == NULL;

    if (buf == NULL) {
        stream->buffer = malloc(size + 1);
    } else {
        stream->buffer = buf;
    }

    stream->buffer_size = size;
    stream->buffer_used = 0;
    stream->buffer_mode = mode;

    funlockfile(stream);

    return 0;
}

int fflush(FILE* stream) {
    flockfile(stream);

    if (stream == NULL) {
        // TODO: flush all open buffers
        errno = ENOSYS;
        funlockfile(stream);
        return EOF;
    }

    if (stream->buffer_direction == BUFFER_DIR_UNSET) {
        funlockfile(stream);
        return 0;

    } else if (stream->buffer_direction == BUFFER_DIR_READ) {
        /*
        * Discard all existing input.
        */ 
        stream->buffer_used = 0;
        stream->buffer_direction = BUFFER_DIR_UNSET;
        funlockfile(stream);
        return 0;

    }  else if (stream->buffer_direction == BUFFER_DIR_WRITE) {
        /*
        * Perform the write.
        */
        // TODO: fwrite(...);
        stream->buffer_used = 0;
        stream->buffer_direction = BUFFER_DIR_UNSET;
        funlockfile(stream);
        return 0;
    
    } else {
        errno = EBADF;
        funlockfile(stream);
        return EOF;
    }
}

int ungetc(int c, FILE* stream) {
    flockfile(stream);

    stream->been_accessed = true;

    if (c == EOF) {
        funlockfile(stream);
        return EOF;
    }

    if (stream->ungetc == EOF) {
        stream->ungetc = c;   
        funlockfile(stream);
        return c;

    } else {
        funlockfile(stream);
        return EOF;
    }
}

int fputc(int c, FILE *stream) {
    flockfile(stream);

    stream->been_accessed = true;

    if (stream->buffer_direction == BUFFER_DIR_UNSET) {
        stream->buffer_direction = BUFFER_DIR_WRITE;

    } else if (stream->buffer_direction == BUFFER_DIR_READ) {
        // hmm... TODO: ?
    }

    unsigned char byte = c;

    if (stream->buffer_mode == _IONBF) {
        ssize_t written = write(stream->fd, &byte, 1);
        if (written == -1) {
            /* errno already set */
            stream->error = true;
            funlockfile(stream);
            return EOF;
        }

    } else if (stream->buffer_mode == _IOLBF) {
        if (stream->buffer_used < stream->buffer_size) {
            stream->buffer[stream->buffer_used++] = byte;

        } else {
            int status = fflush(stream);
            if (status == EOF) {
                stream->error = true;
                funlockfile(stream);
                return EOF;
            }
            status = fputc(c, stream);
            if (status == EOF) {
                funlockfile(stream);
                return EOF;
            }
        }

        if (byte == '\n') {
            int status = fflush(stream);
            if (status == EOF) {
                funlockfile(stream);
                return EOF;
            }
            stream->buffer_direction = BUFFER_DIR_WRITE;    
        } 

    } else if (stream->buffer_mode == _IOFBF) {
        if (stream->buffer_used < stream->buffer_size) {
            stream->buffer[stream->buffer_used++] = byte;

        } else {
            int status = fflush(stream);
            if (status == EOF) {
                funlockfile(stream);
                return EOF;
            }
            funlockfile(stream);
            return fputc(c, stream);
        }
    }
    
    funlockfile(stream);
    return c;
}

int fgetc(FILE* stream) {
    flockfile(stream);

    stream->been_accessed = true;

    if (stream->buffer_direction == BUFFER_DIR_UNSET) {
        stream->buffer_direction = BUFFER_DIR_READ;

    } else if (stream->buffer_direction == BUFFER_DIR_WRITE) {
        // hmm... TODO: ?
    }

    if (stream->ungetc != EOF) {
        int c = stream->ungetc;
        stream->ungetc = EOF;
        funlockfile(stream);
        return c;
    }

    if (stream->buffer_size == 0) {
        ssize_t bytes_read = read(stream->fd, stream->buffer, stream->buffer_mode == _IONBF ? 1 : stream->buffer_size);
        if (bytes_read == -1) {
            stream->error = true;
            funlockfile(stream);
            return EOF;   
        }
        stream->buffer_used = bytes_read;
        if (bytes_read == 0) {
            funlockfile(stream);
            return EOF;
        }
    }   

    int c = stream->buffer[0];
    memmove(stream->buffer, stream->buffer + 1, --stream->buffer_used);
    funlockfile(stream);
    return c;
}

int getc(FILE* stream) {
    return fgetc(stream);
}

int getchar(void) {
    // TODO:
    return 0; //return fgetc(stdin);
}



/*
size_t lock_count;
    int lock_owner;*/



int ftrylockfile(FILE* stream) {
    /*
    * TODO: this whole function needs to be atomic!!
    * (wrap with a non-IRQ manipulating spinlock!)
    */
    int thread_id = 0;
    int result = 0;

    // TODO: spinlock_acquire

    if (stream->lock_count == 0 || stream->lock_owner == thread_id) {
        stream->lock_count++;
        stream->lock_owner = thread_id;

    } else {
        result = 1;
    }

    // TODO: spinlock_release

    return result;
}

void funlockfile(FILE* stream) {
    // TODO: must ensure this is atomic

    stream->lock_count--;
}

void flockfile(FILE* stream) {
    while (ftrylockfile(stream) != 0) {
        ;
    }    
}
