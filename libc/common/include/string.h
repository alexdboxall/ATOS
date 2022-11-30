#pragma once

#include <stddef.h>

#ifndef NULL
#define NULL ((void*) 0)
#endif

void bzero(void* addr, size_t n);
void* memccpy(void* dst, const void* src, int c, size_t n);
void* memchr(const void* addr, int c, size_t n);
int memcmp(const void* addr1, const void* addr2, size_t n);
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* addr, int c, size_t n);
char* strcat(char* dst, const char* src);
char* strchr(const char* str, int c);
int	strcmp(const char* str1, const char* str2);
char* strcpy(char* dst, const char* src);
char* strdup(const char* str);
char* strerror(int err);
size_t strlen(const char* str);
char* strncat(char* dst, const char* src, size_t n);
int strncmp(const char* str1, const char* str2, size_t n);
char* strncpy(char* dst, const char* src, size_t n);
char* strrchr(const char* str, int n);
char* strstr(const char* haystac, const char* needle);