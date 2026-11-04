/*
 * Institute for System Programming of the Russian Academy of Sciences
 * Copyright (C) 2016 ISPRAS
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, Version 3.
 *
 * This program is distributed in the hope # that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License version 3 for more details.
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; no restrict qualifiers; strtok() is
 * declared; deviations are documented where a guideline can't be
 * followed.
 */

/*
 * MISRA C:2025 Rule 5.10 and Rule 20.15 deviation (this header): this is
 * the <string.h> header of libpok's own C library (partitions are built
 * freestanding, without the system one), so it declares the standard
 * names.
 */
#ifndef LIBJET_STRING_H
#define LIBJET_STRING_H

#include <stddef.h>

void *memcpy(void * dest, const void * src, size_t n);

void *memmove(void* dest, const void* src, size_t n);

char *strcpy(char * dest, const char * src);
char *strncpy(char * dest, const char * src, size_t n);

char *strcat(char * dest, const char * src);
char *strncat(char * dest, const char * src, size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

void *memchr(const void *s, int c, size_t n);

char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

size_t strcspn(const char *s1, const char *s2);
size_t strspn(const char *s1, const char *s2);

char *strpbrk(const char *s1, const char *s2);

char *strstr(const char *s1, const char *s2);

char *strtok(char *s1, const char *s2);

void *memset(void *s, int c, size_t n);

size_t strlen(const char *s);

/* POSIX */
int	strncasecmp(const char *s1, const char *s2, size_t n);

#endif /* LIBJET_STRING_H */
