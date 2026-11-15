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
 * Modified by Kirill Brazhnikov, 2026-11-07: hv_crc32() and
 * hv_image_check() (crc.c).
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: only the common part is left
 * here (for x86_64 and arm64); the architecture's own state is the arch
 * member of each structure (<arch/arch.h>), and the x86_64 interfaces
 * moved to arch/x86_64/include/arch/internal.h.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros and types of crc.c, printf.c and
 * vm.c are here, with the declarations of memset() and memcpy(); the types and
 * macros are documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/include/hv/hv.h (the common code and its headers are in
 * hypervisor/core); the comments name the new paths.
 */

/*
 * Hypervisor data structures and the interfaces between its parts.
 *
 * Each physical CPU (hv_cpu_t) runs exactly one virtual CPU (struct
 * hv_vcpu) of one virtual machine (hv_vm_t), so there is no vCPU
 * scheduler. Only vCPU 0 of a VM runs guest code (JetOS is single-core);
 * the other vCPUs of the VM are parked. A VM's devices are therefore only
 * used by one physical CPU and need no locks.
 *
 * The common code (src/) uses the functions declared below as "arch";
 * each architecture (csps/x86_64, csps/arm64) defines them.
 */

#ifndef HV_HV_H
#define HV_HV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <hv/config.h>
#include <arch/arch.h>

/**
 * @macroname `HV_PAGE_SIZE`
 *
 * @description
 * The size of a page, in bytes.
 */
#define HV_PAGE_SIZE        0x1000UL

/**
 * @macroname `HV_LARGE_PAGE_SIZE`
 *
 * @description
 * The size of a large page (2 MiB), in bytes.
 */
#define HV_LARGE_PAGE_SIZE  0x200000UL

/**
 * @macroname `HV_VM_MAX_MEMORY`
 *
 * @description
 * The limit of the guest RAM size, in bytes (one page directory of 2 MiB
 * pages).
 */
#define HV_VM_MAX_MEMORY    0x40000000UL

/**
 * @macroname `HV_ALIGNED`
 *
 * @description
 * Aligns the object that follows it (GNU attribute).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `n` | The alignment, in bytes. |
 */
/*
 * GNU attributes.
 * MISRA C:2025 Dir 1.2 deviation: alignment, format checking and
 * no-return are GNU extensions (the code is compiled with -std=gnu99).
 */
#define HV_ALIGNED(n)       __attribute__((aligned(n)))

/**
 * @macroname `HV_PRINTF`
 *
 * @description
 * Makes the compiler check the arguments of a printf()-like function against
 * its format (GNU attribute).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `f` | The number of the format parameter. |
 * | `a` | The number of the first argument checked against the format. |
 */
#define HV_PRINTF(f, a)     __attribute__((format(printf, f, a)))

/**
 * @macroname `HV_NORETURN`
 *
 * @description
 * Marks a function that never returns (GNU attribute).
 */
#define HV_NORETURN         __attribute__((noreturn))

/**
 * @typename `hv_vm_t`
 *
 * @description
 * A virtual machine: its configuration, its place in the tables, its guest RAM
 * and the state of its architecture.
 */
typedef struct {
    const hv_vm_config_t *config; /**< The configuration of the VM. */
    uint32_t index; /**< The number of the VM in hv_vms. */
    uint32_t first_cpu; /**< Physical CPU running vCPU 0. */
    uint64_t ram_base; /**< Host-physical base of the guest RAM. */
    uint32_t resets; /**< The number of times the VM was restarted. */
    /**
     * The state of the VM that depends on the architecture.
     */
    hv_arch_vm_t arch;
} hv_vm_t;

/**
 * @typename `hv_vcpu_t`
 *
 * @description
 * A virtual CPU of a virtual machine, run by one physical CPU.
 */
typedef struct {
    hv_vm_t *vm; /**< The VM the vCPU belongs to. */
    uint32_t index; /**< VCPU number within its VM. */
    /**
     * The state of the vCPU that depends on the architecture.
     */
    hv_arch_vcpu_t arch;
} hv_vcpu_t;

/**
 * @typename `hv_cpu_t`
 *
 * @description
 * A physical CPU: its number, its hardware identifier, its state and the vCPU
 * it runs.
 */
typedef struct {
    uint32_t index; /**< The number of the CPU in hv_cpus. */
    uint32_t hw_id; /**< Local APIC ID (x86_64) or MPIDR (arm64). */
    volatile uint32_t online; /**< Non-zero once the CPU runs. */
    /**
     * The state of the CPU that depends on the architecture.
     */
    hv_arch_cpu_t arch;
    hv_vcpu_t vcpu; /**< The vCPU the CPU runs. */
} hv_cpu_t;

// What the run loop does after an exit was handled.

/**
 * @typename `hv_action_e`
 *
 * @description
 * What the run loop of a vCPU does after it handled an exit.
 */
typedef enum {
    HV_CONTINUE, /**< Run the vCPU again. */
    HV_RESET, /**< Restart the VM. */
    HV_STOP /**< Stop the VM. */
} hv_action_e;

extern hv_cpu_t hv_cpus[HV_MAX_CPUS];
extern uint32_t hv_nb_cpus;
extern hv_vm_t hv_vms[HV_MAX_VMS];

/**
 * @macroname `CRC32_POLYNOMIAL`
 *
 * @description
 * The polynomial of the CRC-32: 0x04C11DB7 with its bits reversed.
 */
#define CRC32_POLYNOMIAL    0xEDB88320U

/**
 * @macroname `CRC32_INITIAL`
 *
 * @description
 * The initial value of the CRC-32.
 */
#define CRC32_INITIAL       0xFFFFFFFFU

/**
 * @macroname `CRC32_FINAL_XOR`
 *
 * @description
 * The value the CRC-32 is combined with at the end.
 */
#define CRC32_FINAL_XOR     0xFFFFFFFFU

/**
 * @macroname `CRC32_TABLE_SIZE`
 *
 * @description
 * The number of entries of the CRC table (one per byte value).
 */
#define CRC32_TABLE_SIZE    256U

/**
 * @macroname `BITS_PER_BYTE`
 *
 * @description
 * The number of bits of a byte.
 */
#define BITS_PER_BYTE       8U

/**
 * @macroname `BYTE_MASK`
 *
 * @description
 * The mask of the bits of a byte.
 */
#define BYTE_MASK           0xFFU

/*
 * Linker addresses (hv.lds): the start of the loaded image, and the word
 * after .rodata that holds the CRC-32 of the bytes before it.
 */
extern const uint8_t hv_image_begin[];
extern const uint32_t hv_image_crc;

/**
 * @macroname `OUT_BUFFER_SIZE`
 *
 * @description
 * The size of the buffer of the console output, in characters.
 */
#define OUT_BUFFER_SIZE     128U

/**
 * @macroname `NUMBER_DIGITS`
 *
 * @description
 * The size of the buffer for the digits of a number (2^64 has 20 decimal
 * digits).
 */
#define NUMBER_DIGITS       24U

// Characters not yet written to the console.

/**
 * @typename `output_t`
 *
 * @description
 * The characters of the console output that are not written yet.
 */
typedef struct {
    char buffer[OUT_BUFFER_SIZE]; /**< The characters. */
    size_t length; /**< Their number. */
} output_t;

// A conversion specification without its conversion character.

/**
 * @typename `conversion_t`
 *
 * @description
 * A conversion specification of hv_printf(), without its conversion character.
 */
typedef struct {
    bool zero_pad; /**< Whether the number is padded with zeros. */
    uint32_t width; /**< The minimum width of the field. */
    uint32_t longs; /**< Number of l modifiers (z counts as one). */
} conversion_t;

// vm.c: the end of the hypervisor image (hv.lds).
extern uint8_t __hv_end[];

// cpus.c
/** \cond */
uint32_t hv_cpus_needed(void);
void hv_assign_cpus(const uint32_t hw_ids[HV_MAX_CPUS]);
void hv_cpu_main(hv_cpu_t *cpu) HV_NORETURN;
/** \endcond */

// console.c, printf.c
/** \cond */
void hv_console_init(uint16_t port);
void hv_console_lock(void);
void hv_console_unlock(void);
void hv_console_write(const char *s, size_t length);
void hv_printf(const char *format, ...) HV_PRINTF(1, 2);
void hv_fatal(const char *message) HV_NORETURN;
/** \endcond */

// crc.c
/** \cond */
uint32_t hv_crc32(const uint8_t *data, size_t length);
void hv_image_check(void);

// string.c
void hv_memset(void *dst, uint8_t value, size_t size);
void hv_memcpy(void *dst, const void *src, size_t size);

/*
 * MISRA C:2025 Rule 5.10 deviation: GCC may emit calls to memset() and
 * memcpy() (C standard semantics) even in freestanding code, so the
 * hypervisor, which has no C library, defines these reserved names.
 */
void *memset(void *dst, int value, size_t size);
void *memcpy(void *dst, const void *src, size_t size);

// vm.c
void hv_vms_init(uint64_t ram_start, uint64_t ram_end);
void vm_print(const hv_vm_t *vm, const char *message);

/*
 * arch: CPUs. hv_this_cpu() is the calling CPU's structure (CPU 0 before
 * the CPUs are known), hv_arch_cpu_id() its hardware number for messages
 * and the console lock.
 */
hv_cpu_t *hv_this_cpu(void);
uint32_t hv_arch_cpu_id(void);
void hv_halt_forever(void) HV_NORETURN;
/** \endcond */

// arch: the host serial port of a COM port name (HV_PORT_COMn).
/** \cond */
void uart_init(uint16_t port);
void uart_putc(uint16_t port, char c);
const char *hv_port_name(uint16_t port);

/*
 * arch: virtual machines. hv_arch_vm_init() sets up what a VM needs once
 * (guest-physical memory, devices), vm_load() (re)loads its image and
 * resets its devices, hv_arch_cpu_enable() prepares a CPU to run guests,
 * hv_arch_vcpu_init() gives a vCPU its state at power-on and
 * hv_arch_vcpu_run() runs it.
 */
void hv_arch_vm_init(hv_vm_t *vm);
void vm_load(hv_vm_t *vm);
void hv_arch_cpu_enable(hv_cpu_t *cpu);
void hv_arch_vcpu_init(hv_vcpu_t *vcpu);
void hv_arch_vcpu_run(hv_cpu_t *cpu) HV_NORETURN;
/** \endcond */

#endif // HV_HV_H
