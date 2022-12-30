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
        write(stream->fd, stream->buffer, stream->buffer_used);
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

int fileno(FILE* stream) {
    flockfile(stream);
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
                * Assign the flags, but don't allow duplicates.
                */

                if (format[i] == '#') {
                    if (alternative_form) return -1;
                    else alternative_form = true;
                }
                if (format[i] == '0') {
                    if (pad_with_zeroes) return -1;
                    else pad_with_zeroes = true;
                }
                if (format[i] == '-') {
                    if (left_align) return -1;
                    else left_align = true;
                }
                if (format[i] == ' ') {
                    if (space) return -1;
                    else space = true;
                }
                if (format[i] == '+') {
                    if (display_sign) return -1;
                    else display_sign = true;
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
                if (format[i] >= '1' && format[i] <= 9) {
                    field_width = format[i] - '0';
                    ++i;

                    while (format[i] >= '0' && format[i] <= 9) {
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
                    while (format[i] >= '0' && format[i] <= 9) {
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
            * Now the conversion specifier.
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
                char base = 0;      // or 'x' or 'X'
                bool signed_arg = false;
                char sign_to_display = 0;       // either '', '+', '-', or ' '
                
                char* buffer;

                char num_buf[32];

                // TODO: ^^ that needs to contain everything *except* for padding
                //       (i.e. including the sign/space/null, 0x if # is set)


                if (format[i] == 'd' || format[i] == 'i') {
                    numeric = true;
                    integral = true;
                    signed_arg = true;

                } else if (format[i] == 'o' || format[i] == 'u') {
                    numeric = true;
                    integral = true;
                    signed_arg = false;
                
                } else if (format[i] == 'x' || format[i] == 'X') {
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
                        else if (modifier == 'h') value = (short)  va_arg(ap, int);
                        else if (modifier == 'l') value = va_arg(ap, long);
                        else if (modifier == 'q') value = va_arg(ap, long long);
                        else if (modifier == 'j') value = va_arg(ap, intmax_t);
                        else if (modifier == 'z') value = va_arg(ap, ssize_t);
                        else if (modifier == 't') value = va_arg(ap, ptrdiff_t);
                        else {
                            value = va_arg(ap, unsigned int);
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

                    while (real_value > 0 || precision > 0) {
                        --precision;
                        num_buf[--j] = base_lookup[(real_value % (base == 0 ? 10 : 16)) + (base == 'x' ? 16 : 0)];
                        real_value /= (base == 0 ? 10 : 16);
                    }

                    buffer = num_buf + j;
                }

                int padding_required = field_width - strlen(buffer) - (sign_to_display == 0 ? 0 : 1) - (alternative_form && base != 0 ? 2 : 0);
                pad_with_zeroes = !left_align && pad_with_zeroes && numeric;

                char* order;
                if (pad_with_zeroes) {
                    order = "s0d";
                } else if (left_align) {
                    order = "sd ";
                } else {
                    order = " sd";
                }

                fputs(buffer, stream);

                (void) order;
                (void) padding_required;
                /*
                for (int j = 0; order[j]; ++j) {
                    if (order[j] == 's') {
                        if (sign_to_display != 0) {
                            fputc(sign_to_display, stream);
                            ++chars_written;
                        }
                        if (alternative_form && base != 0) {
                            fputc('0', stream);
                            fputc(base, stream);
                            chars_written += 2;
                        }
                    } else if (order[j] == 'd') {
                        for (int k = 0; buffer[k]; ++k) {
                            fputc(buffer[k], stream);
                            ++chars_written;
                        }
                    
                    } else {
                        while (padding_required--) {
                            fputc(order[j], stream);
                            ++chars_written;
                        }
                    }
                }*/
            }

            ++i;
        }
    }   

    return chars_written;
}

int vsnprintf(char* str, size_t size, const char* format, va_list ap) {
    // TODO: use fmemopen, and then call vfprintf
    (void) str;
    (void) size;
    (void) format;
    (void) ap;
    return 0;
}

int vsprintf(char* str, const char* format, va_list ap) {
    return vsnprintf(str, 0xFFFFFFFFU, format, ap);
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