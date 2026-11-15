/*
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The memory routines that the kernel and the hypervisor share
 * (common/memcpy.c and common/memset.c). They have the standard names
 * and types: the compiler may call them for structure copies and
 * initializations, and neither program is linked with a C library.
 *
 * The declarations were in kernel/include/libc.h, and the hypervisor
 * had its own functions (hv_memcpy() and hv_memset(), with memcpy() and
 * memset() on top of them).
 */

#ifndef JET_COMMON_STRING_H
#define JET_COMMON_STRING_H

#include <stddef.h>

/*
 * MISRA C:2025 Rules 20.15 and 5.10 deviation: memcpy() and memset() are
 * the standard names, which the programs have to define themselves.
 */
/** \cond */
void *memcpy(void *to, const void *from, size_t n);
void *memset(void *dest, int val, size_t count);
/** \endcond */

#endif /* JET_COMMON_STRING_H */
