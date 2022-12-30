#pragma once

#include <stddef.h>
#include <stdarg.h>

#define FOPEN_MAX 4096
#define EOF (-1)

#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
#define BUFSIZ 512

struct FILE;
typedef struct FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

FILE* fopen(const char* filename, const char* mode);
FILE* freopen(const char* filename, const char* mode, FILE* stream);
int fclose(FILE* stream);

int feof(FILE* stream);
int ferror(FILE* stream);

void clearerr(FILE* stream);

void flockfile(FILE* stream);
int ftrylockfile(FILE* stream);
void funlockfile(FILE* stream);

/*
* The 'core' read/write functions that (for now) everything else will be
* implemented in terms of. Obviously we will implement optimised fread/fwrite
* later on to increase performance greatly.
*/
int fputc(int c, FILE *stream);
int fgetc(FILE* stream);
int getc(FILE* stream);
int getchar(void);
int ungetc(int c, FILE* stream);

int fputs(const char* s, FILE* stream);
int fflush(FILE* stream);

int fileno(FILE* stream);

int setvbuf(FILE* stream, char* buf, int mode, size_t size);


int vfprintf(FILE* stream, const char* format, va_list ap);
int vsnprintf(char* str, size_t size, const char* format, va_list ap);
int vsprintf(char* str, const char* format, va_list ap);
int vprintf(const char* format, va_list ap);
int fprintf(FILE* stream, const char* format, ...);
int printf(const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);