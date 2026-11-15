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
 *
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier; essentially Boolean conditions; U
 * suffixes for unsigned constants; the macros use no reserved
 * identifiers; deviations are documented where a guideline can't be
 * followed.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: Doxygen comments for its macros.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * kernel/arch/arm64/sysreg.h (the CPU support packages are in kernel/csps);
 * moved from kernel/csps/arm64/sysreg.h, because the hypervisor uses it too;
 * write_sysreg() tells the compiler that the write may change memory.
 */

#ifndef JET_ARM64_SYSREG_H
#define JET_ARM64_SYSREG_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @macroname `read_sysreg`
 *
 * @description
 * Reads a system register: its value (uint64_t).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `reg` | The name of the register (e.g. daif). |
 */
/*
 * MISRA C:2025 Dir 1.2 and Dir 4.3 deviation (this header): system registers
 * are reached with GNU inline assembly, isolated in these macros and in
 * isb(); read_sysreg() is a GNU statement expression, so that it yields the
 * value read. Rule 20.10 (advisory): # makes the register name a part of the
 * instruction.
 */
#define read_sysreg(reg) ({                                 \
    uint64_t sysreg_val;                                    \
    asm volatile("mrs %0, " #reg : "=r" (sysreg_val));      \
    sysreg_val;                                             \
})

/**
 * @macroname `write_sysreg`
 *
 * @description
 * Writes a system register.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `reg` | The name of the register. |
 * | `val` | The value written. |
 */
#define write_sysreg(reg, val) do {                         \
    uint64_t sysreg_val = (uint64_t)(val);                  \
    asm volatile("msr " #reg ", %0" : : "r" (sysreg_val)    \
                 : "memory");                               \
} while(false)

/*
 * Instruction synchronization barrier (isb): later instructions see the effects
 * of system register writes before it.
 */
static inline void isb(void)
{
    asm volatile("isb" ::: "memory");
}

/**
 * @macroname `DAIF_I`
 *
 * @description
 * DAIF.I: IRQs are masked.
 */
#define DAIF_I (1U << 7)

#endif /* JET_ARM64_SYSREG_H */
