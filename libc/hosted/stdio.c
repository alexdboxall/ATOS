#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>

#define BUFFER_DIR_UNSET 0
#define BUFFER_DIR_READ  1
#define BUFFER_DIR_WRITE 2

struct FILE {
    int fd;
    int ungetc;        /* EOF if there is none */
    bool eof;
    bool error;
    int buffer_mode;
    char* buffer;           
    size_t buffer_used;     
    size_t buffer_size;
    bool buffer_needs_freeing;

    ssize_t (*read)(struct FILE* stream, void* buffer, size_t count);
    ssize_t (*write)(struct FILE* stream, void* buffer, size_t count);

    int flags;

    char* mem_buffer;
    size_t mem_size;
    size_t mem_seek_pos;
    bool mem_buffer_needs_freeing;

    /*
    * The direction of the most recent operation. Used by fflush() to determine
    * how to flush the buffer. As per the C standard, you cannot interleave reads
    * and writes without a call to fflush(), or fseek() (which calls fflush()), or
    * similar. Thus the 'most recent operation' should stay the same between calls to
    * fflush() if the program conforms to the spec.
    * 
    * It is set to BUFFER_DIR_UNSET if no operation has taken place yet, this is also
    * used by setvbuf to determine if any operation has been performed on it yet.
    */
    int buffer_direction;

    volatile size_t lock_count;
    volatile int lock_owner;
    bool memopen;
};

/*
* These will be initialised in _start
*/
FILE* stdin;
FILE* stdout;
FILE* stderr;

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

    stream->ungetc = EOF;
    stream->eof = false;
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

    if (mode[0] == 'r') flags |= O_RDONLY;
    else if (mode[0] == 'w') flags |= O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode[0] == 'a') flags |= O_WRONLY | O_CREAT | O_APPEND;
    else {
        errno = EINVAL;
        return NULL;
    }

    for (int i = 1; mode[i]; ++i) {
        if (mode[i] == '+') {
            flags &= ~O_ACCMODE;
            flags |= O_RDWR;

        } else if (mode[i] == 'b') {
            /* Binary mode has no effect on ATOS */

        } else if (mode[i] == 'x') {
            /* Defined in ISO C11 */
            flags |= O_EXCL;
        }
    }

    stream->flags = flags;

    if (stream->memopen) {
        stream->buffer_mode = _IOFBF;
        return stream;
    }

    int fd = open(filename, flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if (fd == -1) {
        /* errno is already set */
        return NULL;
    }

    if ((flags & O_ACCMODE) != O_RDONLY && isatty(fd)) {
        stream->buffer_mode = _IOLBF;
    } else {
        stream->buffer_mode = _IOFBF;
    }

    stream->fd = fd;
    return stream;
}

ssize_t _file_read(FILE* stream, void* buffer, size_t size) {
    return read(stream->fd, buffer, size);
}

ssize_t _file_write(FILE* stream, void* buffer, size_t size) {
    return write(stream->fd, buffer, size);
}


ssize_t _mem_read(FILE* stream, void* buffer, size_t size) {
    if ((stream->flags & O_ACCMODE) == O_WRONLY) {
        errno = EINVAL;
        return -1;
    }

    if (stream->mem_seek_pos >= stream->mem_size) {
        return 0;
    }
    if (stream->mem_seek_pos + size >= stream->mem_size) {
        size = stream->mem_size - stream->mem_seek_pos;
    }

    memcpy(buffer, stream->mem_buffer + stream->mem_seek_pos, size);
    stream->mem_seek_pos += size;

    return size;
}

ssize_t _mem_write(FILE* stream, void* buffer, size_t size) {
    if ((stream->flags & O_ACCMODE) == O_RDONLY) {
        errno = EINVAL;
        return -1;
    }

    if (stream->flags & O_APPEND) {
        while (stream->mem_buffer[stream->mem_seek_pos] && stream->mem_seek_pos < size) {
            ++stream->mem_seek_pos;
        }
    }
    
    if (stream->mem_seek_pos + size >= stream->mem_size) {
        errno = ENOSPC;
        return -1;
    }

    memcpy(stream->mem_buffer + stream->mem_seek_pos, buffer, size);
    stream->mem_seek_pos += size;

    return size;
}

FILE* fmemopen(void* buffer, size_t size, const char* mode) {    
    if (size == 0) {
        errno = ENOMEM;
        return NULL;
    }

    FILE* stream = malloc(sizeof(FILE));

    /*
    * fopen_existing_stream does not touch the lock. We don't need to set the
    * lock, as no-one else would have access to this FILE until we return it.
    */
    stream->lock_count = 0;
    stream->lock_owner = -1;
    stream->memopen = true;
    stream->read = _mem_read;
    stream->write = _mem_write;

    FILE* result = fopen_existing_stream("", mode, stream);

    stream->mem_size = size;
    stream->mem_seek_pos = 0;

    if (buffer == NULL) {
        stream->mem_buffer = malloc(size);
        stream->mem_buffer[0] = 0;      /* to keep append modes happy */
        if (stream->mem_buffer == NULL) {
            errno = ENOMEM;
            return NULL;
        }

        stream->mem_buffer_needs_freeing = true;

    } else {
        stream->mem_buffer = buffer;
        stream->mem_buffer_needs_freeing = false;
    }

    if (mode[0] == 'a') {
        while (stream->mem_buffer[stream->mem_seek_pos] && stream->mem_seek_pos < size) {
            ++stream->mem_seek_pos;
        }

    } else if (!strcmp(mode, "w+")) {
        stream->mem_buffer[0] = 0;
    }

    return result;
}

FILE* fopen(const char* filename, const char* mode) {
    FILE* stream = malloc(sizeof(FILE));

    /*
    * fopen_existing_stream does not touch the lock. We don't need to set the
    * lock, as no-one else would have access to this FILE until we return it.
    */
    stream->lock_count = 0;
    stream->lock_owner = -1;
    stream->memopen = false;
    stream->read = _file_read;
    stream->write = _file_write;
    return fopen_existing_stream(filename, mode, stream);
}

FILE* freopen(const char* filename, const char* mode, FILE* stream) {
    flockfile(stream);
    fclose(stream);
    FILE* result = fopen_existing_stream(filename, mode, stream);
    stream->memopen = false;
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

    if (!stream->memopen) {
        int status = close(stream->fd);
        if (status != 0) {
            return EOF;
        }
    } else if (stream->mem_buffer_needs_freeing) {
        free(stream->mem_buffer);
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

    if (stream == NULL || stream->buffer_direction != BUFFER_DIR_UNSET) {
        errno = EBADF;
        funlockfile(stream);
        return -1;
    }

    if (stream->buffer_needs_freeing) {
        free(stream->buffer);
    }

    stream->buffer_needs_freeing = buf == NULL;

    /*
    * We are allowed to increase the buffer size (and we must if someone
    * passes 0, expecting this increase, such as setlinebuf).
    */
    if (size < 32) {
        size = 32;
    }

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


void setbuffer(FILE* stream, char* buf, size_t size) {
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, size);
}

void setbuf(FILE* stream, char* buf) {
    setbuffer(stream, buf, BUFSIZ);
}

void setlinebuf(FILE* stream) {
    setvbuf(stream, NULL, _IOLBF, 0);
}

int fflush(FILE* stream) {
    if (stream == NULL) {
        // TODO: flush all open streams
        errno = ENOSYS;
        return EOF;
    }

    flockfile(stream);

    if (stream->buffer_direction == BUFFER_DIR_UNSET) {
        funlockfile(stream);
        return 0;

    } else if (stream->buffer_direction == BUFFER_DIR_READ) {
        /*
        * Discard all existing input.
        */ 
        stream->buffer_used = 0;
        funlockfile(stream);
        return 0;

    }  else if (stream->buffer_direction == BUFFER_DIR_WRITE) {
        /*
        * Perform the write.
        */
        ssize_t bytes_written = stream->write(stream, stream->buffer, stream->buffer_used);
        if (bytes_written == -1) {
            stream->error = true;
            funlockfile(stream);
            return EOF;   
        }
        
        stream->buffer_used = 0;

        if (stream->memopen) {
            /* "When a stream that has been opened for writing is flushed
            * (fflush(3)) or closed (fclose(3)), a null byte is written at the
            * end of the buffer if there is space.  The caller should ensure
            * that an extra byte is available in the buffer (and that size
            * counts that byte) to allow for this."
            */
            if (stream->mem_seek_pos < stream->mem_size) {
                stream->mem_buffer[stream->mem_seek_pos++] = 0;
            }
        }

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

    stream->buffer_direction = BUFFER_DIR_WRITE;

    unsigned char byte = c;

    if (stream->buffer_mode == _IONBF) {
        ssize_t written = stream->write(stream, &byte, 1);
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

    stream->buffer_direction = BUFFER_DIR_READ;
    stream->eof = false;

    if (stream->ungetc != EOF) {
        int c = stream->ungetc;
        stream->ungetc = EOF;
        funlockfile(stream);
        return c;
    }

    if (stream->buffer_used == 0) {
        ssize_t bytes_read = stream->read(stream, stream->buffer, stream->buffer_mode == _IONBF ? 1 : stream->buffer_size);
        if (bytes_read == -1) {
            stream->error = true;
            funlockfile(stream);
            return EOF;   
        }
        stream->buffer_used = bytes_read;
        if (bytes_read == 0) {
            stream->eof = true;
            funlockfile(stream);
            return EOF;
        }
    }   

    int c = stream->buffer[0];
    memmove(stream->buffer, stream->buffer + 1, --stream->buffer_used);
    funlockfile(stream);
    return c;
}

char* fgets(char* buffer, int n, FILE* stream) {
    /*
    The fgets() function shall read bytes from stream into the array
       pointed to by s until n-1 bytes are read, or a <newline> is read
       and transferred to s, or an end-of-file condition is encountered.
       A null byte shall be written immediately after the last byte read
       into the array.  If the end-of-file condition is encountered
       before any bytes are read, the contents of the array pointed to
       by s shall not be changed.
    */

    int i = 0;
    for (; i < n - 1; ++i) {
        buffer[i] = fgetc(stream);
        buffer[i + 1] = 0;

        if (stream->error) {
            //errno = ??;
            return NULL;
        }
        if (feof(stream)) {
            return NULL;
        }
        if (buffer[i] == '\n') {
            break;
        }
    }

    return buffer;
}

int getc(FILE* stream) {
    return fgetc(stream);
}

int getchar(void) {
    return fgetc(stdin);
}

int fputs(const char* s, FILE* stream) {
    flockfile(stream);
    for (int i = 0; s[i]; ++i) {
        int status = fputc(s[i], stream);
        if (status == EOF) {
            funlockfile(stream);
            return EOF;
        }
    }
    funlockfile(stream);
    return 0;
}

int puts(const char* s) {
    int status = fputs(s, stdout);
    if (status < 0) {
        return EOF;
    }
    return fputc('\n', stdout);
}

int putc(int c, FILE* stream) {
    return fputc(c, stream);
}

int putchar(int c) {
    return putc(c, stdout);
}

int fileno(FILE* stream) {
    flockfile(stream);
    if (stream->memopen) {
        funlockfile(stream);
        errno = EBADF;
        return -1;
    }
    int fd = stream->fd;
    funlockfile(stream);
    return fd;
}

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

int vfprintf(FILE* stream, const char* format, va_list ap) {
    /*
    * We will not support the '$' syntax.
    */

    flockfile(stream);

    int chars_written = 0;
    for (int i = 0; format[i]; ++i) {
        if (format[i] != '%') {
            fputc(format[i], stream);
            ++chars_written;

        } else {
            ++i;

            /*
            * Look for flag characters.
            */
            bool alternative_form = false;      /* # */
            bool pad_with_zeroes = false;       /* 0 */
            bool left_align = false;            /* - */
            bool space = false;                 /*   */
            bool display_sign = false;          /* + */

            while (format[i] == '#' || format[i] == '0' || format[i] == '-' ||
                   format[i] == ' ' || format[i] == '+') {
                
                /*
                * Assign the flags. We will 'allow' duplicates, but no one should do that anyway.
                * (I don't want to do a lot of if (...) { funlockfile(stream); return -1;} )
                */

                if (format[i] == '#') {
                    alternative_form = true;
                }
                if (format[i] == '0') {
                    pad_with_zeroes = true;
                }
                if (format[i] == '-') {
                    left_align = true;
                }
                if (format[i] == ' ') {
                    space = true;
                }
                if (format[i] == '+') {
                    display_sign = true;
                }

                ++i;
            }

            /*
            * Now grab the field width. It may also be *, which reads it in from
            * an argument. We will use -1 for a non-specified field width (although
            * as conversions never truncate the argument, we could kust set it to 0).
            */
            int field_width = -1;
            if (format[i] == '*') {
                field_width = va_arg(ap, int);

                if (field_width < 0) {
                    field_width = -field_width;
                    left_align = true;
                }

                ++i;

            } else {
                if (format[i] >= '1' && format[i] <= '9') {
                    field_width = format[i] - '0';
                    ++i;

                    while (format[i] >= '0' && format[i] <= '9') {
                        field_width *= 10;
                        field_width += format[i] - '0';
                        ++i;
                    }
                }
            }

            /*
            * Check for the precision. -1 means no precision specified.
            */
            int precision = -1;
            if (format[i] == '.') {
                precision = 0;
                ++i;

                if (format[i] == '*') {
                    precision = va_arg(ap, int);
                    ++i;

                } else {
                    while (format[i] >= '0' && format[i] <= '9') {
                        precision *= 10;
                        precision += format[i] - '0';
                        ++i;
                    }
                }

                if (precision < -1) {
                    precision = -1;
                }
            }

            /*
            * Now clean up the conflicting flags as per the spec.
            */
            if (pad_with_zeroes && left_align) {
                pad_with_zeroes = false;
            }
            if (display_sign && space) {
                space = false;
            }
            if (precision != -1) {
                pad_with_zeroes = false;
            }

            /*
            * Check for the length modifier.
            * We will store no modifier as 0, hh as H, and ll as q.
            */
            char modifier = 0;
            if (format[i] == 'h') {
                modifier = 'h';
                ++i;

                if (format[i] == 'h') {
                    modifier = 'H';
                    ++i;
                }

            } else if (format[i] == 'l') {
                modifier = 'l';
                ++i;

                if (format[i] == 'l') {
                    modifier = 'q';
                    ++i;
                }

            } else if (format[i] == 'q') {
                modifier = 'q';
                ++i;

            } else if (format[i] == 'L') {
                modifier = 'L';
                ++i;
                
            } else if (format[i] == 'j') {
                modifier = 'j';
                ++i;
                
            } else if (format[i] == 'z' || format[i] == 'Z') {
                modifier = 'z';
                ++i;
                
            } else if (format[i] == 't') {
                modifier = 't';
                ++i;
            }

            /*
            * Now the conversion specifier. The for loop handles the increment of i.
            */
            if (format[i] == '%') {
                fputc('%', stream);
                ++chars_written;

            } else if (format[i] == 'n') {
                if (modifier == 'H') *va_arg(ap, signed char*) = chars_written;
                else if (modifier == 'h') *va_arg(ap, short*) = chars_written;
                else if (modifier == 'l') *va_arg(ap, long*) = chars_written;
                else if (modifier == 'q') *va_arg(ap, long long*) = chars_written;
                else if (modifier == 'j') *va_arg(ap, intmax_t*) = chars_written;
                else if (modifier == 'z') *va_arg(ap, size_t*) = chars_written;
                else if (modifier == 't') *va_arg(ap, ptrdiff_t*) = chars_written;
                else {
                    *va_arg(ap, int*) = chars_written;
                }
 
            } else {
                bool numeric = false;
                bool integral = false;
                bool pointer = false;
                char base = 0;      // or 'x' or 'X' or 'o'
                bool signed_arg = false;
                char sign_to_display = 0;       // either '', '+', '-', or ' '
                
                char* buffer;
                int buffer_manual_len = -1;

                char num_buf[32];

                // TODO: ^^ that needs to contain everything *except* for padding
                //       (i.e. including the sign/space/null, 0x if # is set)


                if (format[i] == 'd' || format[i] == 'i') {
                    numeric = true;
                    integral = true;
                    signed_arg = true;

                } else if (format[i] == 'u') {
                    numeric = true;
                    integral = true;
                    signed_arg = false;

                } else if (format[i] == 'p') {
                    numeric = true;
                    integral = true;
                    signed_arg = false;
                    base = 'x';
                    pointer = true;
                    alternative_form = true;
                
                } else if (format[i] == 'o' || format[i] == 'x' || format[i] == 'X') {
                    numeric = true;
                    integral = true;
                    signed_arg = false;
                    base = format[i];
                
                } else if (format[i] == 'c') {
                    num_buf[0] = (char) va_arg(ap, int);
                    num_buf[1] = 0;
                    buffer = num_buf;

                } else if (format[i] == 's') {
                    buffer = va_arg(ap, char*);
                    buffer_manual_len = precision;

                } else {
                    // scary floating point stuff...
                    buffer = "floating point!";
                }

                if (integral) {
                    if (precision == -1) {
                        precision = 1;
                    } else {
                        pad_with_zeroes = false;
                    }

                    uint64_t real_value;
                    if (signed_arg) {
                        int64_t value;

                        if (modifier == 'H') value = (signed char) va_arg(ap, int);
                        else if (modifier == 'h') value = (short) va_arg(ap, int);
                        else if (modifier == 'l') value = va_arg(ap, long);
                        else if (modifier == 'q') value = va_arg(ap, long long);
                        else if (modifier == 'j') value = va_arg(ap, intmax_t);
                        else if (modifier == 'z') value = va_arg(ap, ssize_t);
                        else if (modifier == 't') value = va_arg(ap, ptrdiff_t);
                        else {
                            value = va_arg(ap, int);
                        }

                        bool negative = value < 0;
                        if (negative) {
                            value = -value;
                        }

                        real_value = value;
                        
                        if (negative) {
                            sign_to_display = '-';

                        } else if (display_sign) {
                            sign_to_display = '+';

                        } else if (space) {
                            sign_to_display = ' ';

                        } 

                    } else {
                        uint64_t value;

                        if (pointer) {
                            value = (size_t) va_arg(ap, void*);

                        } else {
                            if (modifier == 'H') value = (unsigned char) va_arg(ap, unsigned int);
                            else if (modifier == 'h') value = (unsigned short) va_arg(ap, unsigned int);
                            else if (modifier == 'l') value = va_arg(ap, unsigned long);
                            else if (modifier == 'q') value = va_arg(ap, unsigned long long);
                            else if (modifier == 'j') value = va_arg(ap, uintmax_t);
                            else if (modifier == 'z') value = va_arg(ap, size_t);
                            else if (modifier == 't') value = va_arg(ap, ptrdiff_t);
                            else {
                                value = va_arg(ap, unsigned int);
                            }
                        }
                        

                        real_value = value;

                        if (display_sign) {
                            sign_to_display = '+';

                        } else if (space) {
                            sign_to_display = ' ';
                        }
                    }
                    
                    memset(num_buf, 0, 32);

                    int j = 31;     // need to make room for null

                    char base_lookup[] = "0123456789ABCDEF0123456789abcdef";

                    int base_as_int = base == 0 ? 10 : (base == 'o' ? 8 : 16);
                    while (real_value > 0 || precision > 0) {
                        --precision;
                        num_buf[--j] = base_lookup[(real_value % base_as_int) + (base == 'x' ? 16 : 0)];
                        real_value /= base_as_int;
                    }

                    buffer = num_buf + j;
                }

            
                int padding_required = field_width - (buffer_manual_len == -1 ? (int) strlen(buffer) : buffer_manual_len) - (sign_to_display == 0 ? 0 : 1) - (alternative_form && base != 0 ? (base == 'o' ? 1 : 2) : 0);
                pad_with_zeroes = !left_align && pad_with_zeroes && numeric;

                char* order;
                if (pad_with_zeroes) {
                    order = "s0d";
                } else if (left_align) {
                    order = "sd ";
                } else {
                    order = " sd";
                }

                for (int j = 0; order[j]; ++j) {
                    if (order[j] == 's') {
                        if (sign_to_display != 0) {
                            fputc(sign_to_display, stream);
                            ++chars_written;
                        }
                        if (alternative_form && base != 0) {
                            if (base != 'o') {
                                fputc('0', stream);
                                ++chars_written;
                            }
                                
                            fputc(base, stream);
                            ++chars_written; 
                        }
                    } else if (order[j] == 'd') {
                        for (int k = 0; buffer[k] && (buffer_manual_len == -1 || k < buffer_manual_len); ++k) {
                            fputc(buffer[k], stream);
                            ++chars_written;
                        }
                    
                    } else {
                        while (padding_required-- > 0) {
                            fputc(order[j], stream);
                            ++chars_written;
                        }
                    }
                }
            }
        }
    }   

    funlockfile(stream);

    return chars_written;
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    FILE* f = fmemopen(str, size - 1, "w");

    if (f == NULL) {
        return -1;
    }

    int status_1 = vfprintf(f, format, ap);
    int status_2 = fclose(f);
    
    if (status_1 == -1) {
        return -1;
    }
    if (status_2 != 0) {
        errno = status_2;
        return -1;
    }

    if (status_1 >= (int) size - 1) {
        str[size - 1] = 0;
    }

    return status_1;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, 0x7FFFFFFFU, format, ap);
}

int vprintf(const char* format, va_list ap) {
    return vfprintf(stdout, format, ap);
}

int fprintf(FILE* stream, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vfprintf(stream, format, ap);
    va_end(ap);
    return result;
}

int printf(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vprintf(format, ap);
    va_end(ap);
    return result;
}

int sprintf(char* str, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vsprintf(str, format, ap);
    va_end(ap);
    return result;
}

int snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int result = vsnprintf(str, size, format, ap);
    va_end(ap);
    return result;
}