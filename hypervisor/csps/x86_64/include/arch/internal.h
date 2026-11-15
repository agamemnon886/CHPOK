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
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros, types and external declarations of
 * the C files are here, documented; hv_main(), hv_ap_main() and hv_interrupt()
 * are declared here.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/x86_64/include/arch/internal.h (the code of the processor
 * architecture is in hypervisor/csps); the comments name the new paths.
 */

/*
 * Interfaces between the x86_64 parts of the hypervisor (csps/x86_64,
 * bsps/x86_64-qemu): local APIC, timers, ACPI, IDT, start-up of the other
 * CPUs, SVM, nested paging, I/O ports and the virtual devices.
 */

#ifndef HV_ARCH_INTERNAL_H
#define HV_ARCH_INTERNAL_H

#include <hv/hv.h>

/**
 * @macroname `HV_VECTOR_TIMER`
 *
 * @description
 * The vector of the timer interrupt of the host (the local APIC timer).
 */
#define HV_VECTOR_TIMER     0xF0U

/**
 * @macroname `HV_VECTOR_SPURIOUS`
 *
 * @description
 * The vector of the spurious interrupts of the local APIC.
 */
#define HV_VECTOR_SPURIOUS  0xFFU

/**
 * @macroname `HV_PIT_HZ`
 *
 * @description
 * The frequency of the input clock of the 8254 PIT, in Hz.
 */
#define HV_PIT_HZ           1193182U

/**
 * @macroname `HV_GUEST_MBI`
 *
 * @description
 * Guest-physical address of the Multiboot information block.
 */
#define HV_GUEST_MBI        0x9000U

/**
 * @macroname `EBDA_SEGMENT_PTR`
 *
 * @description
 * The address, in the BIOS data area, of the segment of the extended BIOS data
 * area.
 */
#define EBDA_SEGMENT_PTR    0x40EUL

/**
 * @macroname `BIOS_AREA_START`
 *
 * @description
 * The start of the BIOS area where the ACPI RSDP is searched.
 */
#define BIOS_AREA_START     0xE0000UL

/**
 * @macroname `BIOS_AREA_END`
 *
 * @description
 * The end (exclusive) of the BIOS area where the ACPI RSDP is searched.
 */
#define BIOS_AREA_END       0x100000UL

/**
 * @name ACPI tables
 * The sizes and values of the ACPI tables the hypervisor reads.
 * @{
 */

/**
 * @macroname `SDT_HEADER_SIZE`
 *
 * @description
 * The size of the header of an ACPI system description table, in bytes.
 */
#define SDT_HEADER_SIZE     36U

/**
 * @macroname `MADT_ENTRIES`
 *
 * @description
 * The offset of the first entry of the MADT, in bytes.
 */
#define MADT_ENTRIES        44U

/**
 * @macroname `MADT_LOCAL_APIC`
 *
 * @description
 * The type of a MADT entry of a processor local APIC.
 */
#define MADT_LOCAL_APIC     0U

/**
 * @macroname `MADT_LOCAL_X2APIC`
 *
 * @description
 * The type of a MADT entry of a processor local x2APIC.
 */
#define MADT_LOCAL_X2APIC   9U

/**
 * @macroname `MADT_CPU_ENABLED`
 *
 * @description
 * The flag of a MADT entry: the CPU is enabled.
 */
#define MADT_CPU_ENABLED    1U

/**
 * @macroname `MADT_CPU_ONLINE_CAPABLE`
 *
 * @description
 * The flag of a MADT entry: the CPU can be enabled.
 */
#define MADT_CPU_ONLINE_CAPABLE 2U

/** @} */

/**
 * @name IDT and vectors
 * The sizes and values of the interrupt descriptor table and its vectors.
 * @{
 */

/**
 * @macroname `IDT_ENTRIES`
 *
 * @description
 * The number of the vectors (gates of the IDT).
 */
#define IDT_ENTRIES         256U

/**
 * @macroname `ISR_STUB_SIZE`
 *
 * @description
 * The size of the entry stub of a vector in isr.S, in bytes.
 */
#define ISR_STUB_SIZE       16U

/**
 * @macroname `GDT_CODE64`
 *
 * @description
 * The selector of the 64-bit code segment of the host.
 */
#define GDT_CODE64          0x08U

/**
 * @macroname `IDT_INTERRUPT_GATE`
 *
 * @description
 * The type of a gate: present, DPL 0, 64-bit interrupt gate.
 */
#define IDT_INTERRUPT_GATE  0x8EU

/**
 * @macroname `EXCEPTION_VECTORS`
 *
 * @description
 * The number of the vectors of CPU exceptions (they start at 0).
 */
#define EXCEPTION_VECTORS   32U

/**
 * @macroname `LEGACY_PIC_FIRST`
 *
 * @description
 * The first vector of the legacy PICs of the guest.
 */
#define LEGACY_PIC_FIRST    0x20U

/**
 * @macroname `LEGACY_PIC_LAST`
 *
 * @description
 * The last vector of the legacy PICs of the guest.
 */
#define LEGACY_PIC_LAST     0x2FU

/** @} */

// Registers saved by isr.S, then the CPU's interrupt frame.

/**
 * @typename `hv_int_frame_t`
 *
 * @description
 * The registers saved by isr.S and the interrupt frame the CPU pushed, as
 * hv_interrupt() sees them.
 */
typedef struct {
    uint64_t r11; /**< Saved R11. */
    uint64_t r10; /**< Saved R10. */
    uint64_t r9; /**< Saved R9. */
    uint64_t r8; /**< Saved R8. */
    uint64_t rdi; /**< Saved RDI. */
    uint64_t rsi; /**< Saved RSI. */
    uint64_t rdx; /**< Saved RDX. */
    uint64_t rcx; /**< Saved RCX. */
    uint64_t rax; /**< Saved RAX. */
    uint64_t vector; /**< The vector of the interrupt. */
    /**
     * The error code (0 for the vectors that have none).
     */
    uint64_t error_code;
    uint64_t rip; /**< The RIP the CPU pushed. */
    uint64_t cs; /**< The CS the CPU pushed. */
    uint64_t rflags; /**< The RFLAGS the CPU pushed. */
    uint64_t rsp; /**< The RSP the CPU pushed. */
    uint64_t ss; /**< The SS the CPU pushed. */
} hv_int_frame_t;

/**
 * @typename `idt_gate_t`
 *
 * @description
 * A gate of the interrupt descriptor table (64-bit interrupt gate).
 */
typedef struct {
    uint16_t offset_low; /**< Bits 0-15 of the address of the handler. */
    uint16_t selector; /**< The code segment selector. */
    uint8_t ist; /**< The interrupt stack table index. */
    uint8_t type; /**< The type and attributes of the gate. */
    uint16_t offset_mid; /**< Bits 16-31 of the address of the handler. */
    uint32_t offset_high; /**< Bits 32-63 of the address of the handler. */
    uint32_t reserved; /**< Reserved. */
} idt_gate_t;

// Operand of lidt.
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idt_descriptor_t;

extern const uint8_t hv_isr_stubs[];

/**
 * @name I/O ports
 * The I/O ports of the devices of the guest that the hypervisor handles.
 * @{
 */

/**
 * @macroname `PORT_PIC_MASTER`
 *
 * @description
 * The first I/O port of the master PIC (2 ports).
 */
#define PORT_PIC_MASTER     0x20U

/**
 * @macroname `PORT_PIC_SLAVE`
 *
 * @description
 * The first I/O port of the slave PIC (2 ports).
 */
#define PORT_PIC_SLAVE      0xA0U

/**
 * @macroname `PORT_PIT_FIRST`
 *
 * @description
 * The first I/O port of the PIT.
 */
#define PORT_PIT_FIRST      0x40U

/**
 * @macroname `PORT_PIT_LAST`
 *
 * @description
 * The last I/O port of the PIT.
 */
#define PORT_PIT_LAST       0x43U

/**
 * @macroname `PORT_KBD_DATA`
 *
 * @description
 * The I/O port of the data of the keyboard controller.
 */
#define PORT_KBD_DATA       0x60U

/**
 * @macroname `PORT_PORT_B`
 *
 * @description
 * The I/O port of the system control port B.
 */
#define PORT_PORT_B         0x61U

/**
 * @macroname `PORT_KBD_STATUS`
 *
 * @description
 * The I/O port of the status and commands of the keyboard controller.
 */
#define PORT_KBD_STATUS     0x64U

/**
 * @macroname `PORT_POST`
 *
 * @description
 * The I/O port of the POST codes.
 */
#define PORT_POST           0x80U

/**
 * @macroname `PORT_DELAY`
 *
 * @description
 * The I/O port that guests write to for a short delay.
 */
#define PORT_DELAY          0xEDU

/**
 * @macroname `PORT_PCI_FIRST`
 *
 * @description
 * The first I/O port of the PCI configuration mechanism.
 */
#define PORT_PCI_FIRST      0xCF8U

/**
 * @macroname `PORT_PCI_LAST`
 *
 * @description
 * The last I/O port of the PCI configuration mechanism.
 */
#define PORT_PCI_LAST       0xCFFU

/**
 * @macroname `PORT_RESET_CONTROL`
 *
 * @description
 * The I/O port of the reset control register.
 */
#define PORT_RESET_CONTROL  0xCF9U

/** @} */

/**
 * @macroname `UART_PORTS`
 *
 * @description
 * The number of the I/O ports of a 16550 UART.
 */
#define UART_PORTS          8U

/**
 * @macroname `KBD_CMD_RESET`
 *
 * @description
 * The command of the keyboard controller that resets the CPU.
 */
#define KBD_CMD_RESET       0xFEU

/**
 * @macroname `RESET_CONTROL_CPU`
 *
 * @description
 * The bit of the reset control register that resets the CPU.
 */
#define RESET_CONTROL_CPU   0x04U

/**
 * @name Local APIC registers
 * The offsets of the registers of the local APIC.
 * @{
 */

/**
 * @macroname `LAPIC_ID`
 *
 * @description
 * The offset of the ID register of the local APIC.
 */
#define LAPIC_ID            0x020U

/**
 * @macroname `LAPIC_EOI`
 *
 * @description
 * The offset of the end of interrupt register.
 */
#define LAPIC_EOI           0x0B0U

/**
 * @macroname `LAPIC_SVR`
 *
 * @description
 * The offset of the spurious interrupt vector register.
 */
#define LAPIC_SVR           0x0F0U

/**
 * @macroname `LAPIC_ICR_LOW`
 *
 * @description
 * The offset of the low half of the interrupt command register.
 */
#define LAPIC_ICR_LOW       0x300U

/**
 * @macroname `LAPIC_ICR_HIGH`
 *
 * @description
 * The offset of the high half of the interrupt command register.
 */
#define LAPIC_ICR_HIGH      0x310U

/**
 * @macroname `LAPIC_LVT_TIMER`
 *
 * @description
 * The offset of the local vector table entry of the timer.
 */
#define LAPIC_LVT_TIMER     0x320U

/**
 * @macroname `LAPIC_LVT_LINT0`
 *
 * @description
 * The offset of the local vector table entry of LINT0.
 */
#define LAPIC_LVT_LINT0     0x350U

/**
 * @macroname `LAPIC_LVT_LINT1`
 *
 * @description
 * The offset of the local vector table entry of LINT1.
 */
#define LAPIC_LVT_LINT1     0x360U

/**
 * @macroname `LAPIC_LVT_ERROR`
 *
 * @description
 * The offset of the local vector table entry of the error.
 */
#define LAPIC_LVT_ERROR     0x370U

/**
 * @macroname `LAPIC_TIMER_INIT`
 *
 * @description
 * The offset of the initial count register of the timer.
 */
#define LAPIC_TIMER_INIT    0x380U

/**
 * @macroname `LAPIC_TIMER_CURRENT`
 *
 * @description
 * The offset of the current count register of the timer.
 */
#define LAPIC_TIMER_CURRENT 0x390U

/**
 * @macroname `LAPIC_TIMER_DIVIDE`
 *
 * @description
 * The offset of the divide configuration register of the timer.
 */
#define LAPIC_TIMER_DIVIDE  0x3E0U

/** @} */

/**
 * @name Local APIC bits
 * The bits and values of the registers of the local APIC.
 * @{
 */

/**
 * @macroname `LAPIC_SVR_ENABLE`
 *
 * @description
 * The bit of the spurious interrupt vector register that enables the local
 * APIC.
 */
#define LAPIC_SVR_ENABLE        ((uint32_t) 1U << 8)

/**
 * @macroname `LAPIC_LVT_MASKED`
 *
 * @description
 * The bit of a local vector table entry that masks it.
 */
#define LAPIC_LVT_MASKED        ((uint32_t) 1U << 16)

/**
 * @macroname `LAPIC_LVT_NMI`
 *
 * @description
 * The delivery mode of a local vector table entry: NMI.
 */
#define LAPIC_LVT_NMI           ((uint32_t) 4U << 8)

/**
 * @macroname `LAPIC_TIMER_PERIODIC`
 *
 * @description
 * The bit of the local vector table entry of the timer that makes it periodic.
 */
#define LAPIC_TIMER_PERIODIC    ((uint32_t) 1U << 17)

/**
 * @macroname `LAPIC_TIMER_DIVIDE_16`
 *
 * @description
 * The value of the divide configuration register that divides by 16.
 */
#define LAPIC_TIMER_DIVIDE_16   0x3U

/**
 * @macroname `LAPIC_ICR_PENDING`
 *
 * @description
 * The bit of the interrupt command register that is set while the interrupt is
 * being sent.
 */
#define LAPIC_ICR_PENDING       ((uint32_t) 1U << 12)

/**
 * @macroname `LAPIC_ICR_INIT`
 *
 * @description
 * The value of the interrupt command register that sends an INIT (level,
 * assert).
 */
#define LAPIC_ICR_INIT          0x00004500U

/**
 * @macroname `LAPIC_ICR_STARTUP`
 *
 * @description
 * The value of the interrupt command register that sends a startup IPI (level,
 * assert).
 */
#define LAPIC_ICR_STARTUP       0x00004600U

/** @} */

/**
 * @macroname `APIC_BASE_ADDR_MASK`
 *
 * @description
 * The mask of the base address in MSR_APIC_BASE.
 */
#define APIC_BASE_ADDR_MASK     0xFFFFF000UL

/**
 * @macroname `MBI_FLAGS`
 *
 * @description
 * The offset of the flags in the Multiboot information block.
 */
#define MBI_FLAGS           0U

/**
 * @macroname `MBI_MEM_UPPER`
 *
 * @description
 * The offset of the size of the upper memory in the Multiboot information
 * block.
 */
#define MBI_MEM_UPPER       8U

/**
 * @macroname `MBI_FLAG_MEM`
 *
 * @description
 * The flag of the Multiboot information block: the memory sizes are valid.
 */
#define MBI_FLAG_MEM        ((uint32_t) 1U << 0)

/**
 * @macroname `MBI_UPPER_MEM_START`
 *
 * @description
 * The start of the upper memory, in bytes (1 MiB).
 */
#define MBI_UPPER_MEM_START 0x100000UL

/**
 * @macroname `APIC_IDS`
 *
 * @description
 * The number of the local APIC IDs (the size of the table that maps them to
 * CPUs).
 */
#define APIC_IDS            256U

/**
 * @name Multiboot information
 * The offsets, flags and sizes of the Multiboot information block and of the
 * memory of the guest.
 * @{
 */

/**
 * @macroname `MBI_MEM_LOWER`
 *
 * @description
 * The offset of the size of the lower memory in the Multiboot information
 * block.
 */
#define MBI_MEM_LOWER       4U

/**
 * @macroname `MBI_MMAP_LENGTH`
 *
 * @description
 * The offset of the length of the memory map in the Multiboot information
 * block.
 */
#define MBI_MMAP_LENGTH     44U

/**
 * @macroname `MBI_MMAP_ADDR`
 *
 * @description
 * The offset of the address of the memory map in the Multiboot information
 * block.
 */
#define MBI_MMAP_ADDR       48U

/**
 * @macroname `MBI_LOADER_NAME`
 *
 * @description
 * The offset of the address of the boot loader name in the Multiboot
 * information block.
 */
#define MBI_LOADER_NAME     64U

/**
 * @macroname `MBI_FLAG_MMAP`
 *
 * @description
 * The flag of the Multiboot information block: the memory map is valid.
 */
#define MBI_FLAG_MMAP       ((uint32_t) 1U << 6)

/**
 * @macroname `MBI_FLAG_LOADER`
 *
 * @description
 * The flag of the Multiboot information block: the boot loader name is valid.
 */
#define MBI_FLAG_LOADER     ((uint32_t) 1U << 9)

/**
 * @macroname `MBI_MMAP_OFFSET`
 *
 * @description
 * The offset of the memory map from the start of the guest's Multiboot
 * information block.
 */
#define MBI_MMAP_OFFSET     0x100U

/**
 * @macroname `MBI_NAME_OFFSET`
 *
 * @description
 * The offset of the boot loader name from the start of the guest's Multiboot
 * information block.
 */
#define MBI_NAME_OFFSET     0x200U

/**
 * @macroname `MMAP_ENTRY_SIZE`
 *
 * @description
 * The size of an entry of the memory map, in bytes.
 */
#define MMAP_ENTRY_SIZE     24U

/**
 * @macroname `MMAP_RAM`
 *
 * @description
 * The type of a memory map entry of available RAM.
 */
#define MMAP_RAM            1U

/**
 * @macroname `LOW_MEMORY_END`
 *
 * @description
 * The end of the lower memory of the guest, in bytes.
 */
#define LOW_MEMORY_END      0x9FC00U

/**
 * @macroname `HIGH_MEMORY_START`
 *
 * @description
 * The start of the upper memory of the guest, in bytes.
 */
#define HIGH_MEMORY_START   0x100000U

/** @} */

/**
 * @name Nested page tables
 * The sizes and bits of the nested page tables.
 * @{
 */

/**
 * @macroname `NPT_ENTRIES`
 *
 * @description
 * The number of the entries of a nested page table.
 */
#define NPT_ENTRIES     512U

/**
 * @macroname `NPT_PRESENT`
 *
 * @description
 * The present bit of an entry.
 */
#define NPT_PRESENT     0x001UL

/**
 * @macroname `NPT_WRITABLE`
 *
 * @description
 * The writable bit of an entry.
 */
#define NPT_WRITABLE    0x002UL

/**
 * @macroname `NPT_USER`
 *
 * @description
 * The bit of an entry that makes the nested walks user accesses.
 */
#define NPT_USER        0x004UL

/**
 * @macroname `NPT_LARGE`
 *
 * @description
 * The bit of an entry that makes it map a 2 MiB page.
 */
#define NPT_LARGE       0x080UL

/**
 * @macroname `NPT_TABLE`
 *
 * @description
 * The bits of an entry that points to a lower table.
 */
#define NPT_TABLE       (NPT_PRESENT | NPT_WRITABLE | NPT_USER)

/**
 * @macroname `NPT_PAGE`
 *
 * @description
 * The bits of an entry that maps a 2 MiB page.
 */
#define NPT_PAGE        (NPT_TABLE | NPT_LARGE)

/** @} */

/**
 * @typename `npt_tables_t`
 *
 * @description
 * The nested page tables of a VM: one table of each of the levels 4, 3 and 2
 * (the guest RAM is mapped with 2 MiB pages).
 */
typedef struct {
    /**
     * The level 4 table (only its first entry is used).
     */
    uint64_t pml4[NPT_ENTRIES];
    uint64_t pdpt[NPT_ENTRIES]; /**< The level 3 table (one entry per GiB). */
    uint64_t pd[NPT_ENTRIES]; /**< The level 2 table (2 MiB pages). */
} npt_tables_t;

/**
 * @macroname `PIC_MASTER`
 *
 * @description
 * The first I/O port of the master PIC.
 */
#define PIC_MASTER          0x20U

/**
 * @macroname `PIC_SLAVE`
 *
 * @description
 * The first I/O port of the slave PIC.
 */
#define PIC_SLAVE           0xA0U

/**
 * @name PIT and port B
 * The I/O ports and bits of the PIT and of port B.
 * @{
 */

/**
 * @macroname `PIT_CHANNEL2`
 *
 * @description
 * The I/O port of channel 2 of the PIT.
 */
#define PIT_CHANNEL2        0x42U

/**
 * @macroname `PIT_COMMAND`
 *
 * @description
 * The I/O port of the command register of the PIT.
 */
#define PIT_COMMAND         0x43U

/**
 * @macroname `PORT_B`
 *
 * @description
 * The I/O port of the system control port B.
 */
#define PORT_B              0x61U

/**
 * @macroname `PORT_B_GATE2`
 *
 * @description
 * The bit of port B that gates channel 2 of the PIT.
 */
#define PORT_B_GATE2        0x01U

/**
 * @macroname `PORT_B_SPEAKER`
 *
 * @description
 * The bit of port B that connects channel 2 to the speaker.
 */
#define PORT_B_SPEAKER      0x02U

/**
 * @macroname `PORT_B_OUT2`
 *
 * @description
 * The bit of port B that reads the output of channel 2.
 */
#define PORT_B_OUT2         0x20U

/** @} */

/**
 * @macroname `CALIBRATION_TICKS`
 *
 * @description
 * The calibration time, in PIT ticks (at most 0xFFFF): 50 ms.
 */
#define CALIBRATION_TICKS   (HV_PIT_HZ / 20U)

/**
 * @macroname `LAPIC_TIMER_START`
 *
 * @description
 * The initial count of the one-shot local APIC timer that measures the
 * calibration time.
 */
#define LAPIC_TIMER_START   0xFFFFFFFFU

/**
 * @name Other CPUs
 * The start of the other CPUs.
 * @{
 */

/**
 * @macroname `AP_TRAMPOLINE`
 *
 * @description
 * The physical address the trampoline of the other CPUs is copied to.
 */
#define AP_TRAMPOLINE       0x8000UL

/**
 * @macroname `AP_SIPI_VECTOR`
 *
 * @description
 * The vector of the startup IPI: the page number of the trampoline.
 */
#define AP_SIPI_VECTOR      ((uint8_t) (AP_TRAMPOLINE >> 12))

/**
 * @macroname `AP_STACK_SIZE`
 *
 * @description
 * The size of the stack of each other CPU, in bytes.
 */
#define AP_STACK_SIZE       0x4000U

/**
 * @macroname `AP_START_TIMEOUT_US`
 *
 * @description
 * The time to wait for another CPU to start, in microseconds.
 */
#define AP_START_TIMEOUT_US 1000000U

/** @} */

extern const uint8_t hv_ap_trampoline[];
extern const uint8_t hv_ap_trampoline_end[];
extern volatile uint64_t hv_ap_stack_top;
extern volatile uint32_t hv_ap_cpu_index;

/**
 * @name UART registers
 * The offsets of the registers of a UART.
 * @{
 */

/**
 * @macroname `UART_THR`
 *
 * @description
 * The offset of the transmit holding register of a UART (DLAB = 0).
 */
#define UART_THR    0U

/**
 * @macroname `UART_IER`
 *
 * @description
 * The offset of the interrupt enable register of a UART (DLAB = 0).
 */
#define UART_IER    1U

/**
 * @macroname `UART_DLL`
 *
 * @description
 * The offset of the low byte of the divisor latch of a UART (DLAB = 1).
 */
#define UART_DLL    0U

/**
 * @macroname `UART_DLM`
 *
 * @description
 * The offset of the high byte of the divisor latch of a UART (DLAB = 1).
 */
#define UART_DLM    1U

/**
 * @macroname `UART_FCR`
 *
 * @description
 * The offset of the FIFO control register of a UART.
 */
#define UART_FCR    2U

/**
 * @macroname `UART_LCR`
 *
 * @description
 * The offset of the line control register of a UART.
 */
#define UART_LCR    3U

/**
 * @macroname `UART_MCR`
 *
 * @description
 * The offset of the modem control register of a UART.
 */
#define UART_MCR    4U

/**
 * @macroname `UART_LSR`
 *
 * @description
 * The offset of the line status register of a UART.
 */
#define UART_LSR    5U

/** @} */

/**
 * @macroname `UART_LSR_THRE`
 *
 * @description
 * The bit of the line status register: the transmit holding register is empty.
 */
#define UART_LSR_THRE   0x20U

/**
 * @macroname `PIC_CASCADE_IRQ`
 *
 * @description
 * The IRQ of the master PIC that the slave PIC is connected to.
 */
#define PIC_CASCADE_IRQ     2U

/**
 * @macroname `PIC_IRQS`
 *
 * @description
 * The number of the IRQs of a PIC.
 */
#define PIC_IRQS            8U

/**
 * @name PIC commands
 * The bits of the initialization and operation command words of the PIC.
 * @{
 */

/**
 * @macroname `ICW1_INIT`
 *
 * @description
 * The bit of ICW1 that starts the initialization.
 */
#define ICW1_INIT           0x10U

/**
 * @macroname `ICW1_ICW4`
 *
 * @description
 * The bit of ICW1: ICW4 is needed.
 */
#define ICW1_ICW4           0x01U

/**
 * @macroname `ICW1_SINGLE`
 *
 * @description
 * The bit of ICW1: a single PIC, so no ICW3.
 */
#define ICW1_SINGLE         0x02U

/**
 * @macroname `ICW4_AUTO_EOI`
 *
 * @description
 * The bit of ICW4: the end of interrupt is automatic.
 */
#define ICW4_AUTO_EOI       0x02U

/**
 * @macroname `OCW3_SELECT`
 *
 * @description
 * The bits of OCW3 that identify it (bit 3 set, bit 4 clear).
 */
#define OCW3_SELECT         0x08U

/**
 * @macroname `OCW3_MASK`
 *
 * @description
 * The mask of the bits that identify OCW3.
 */
#define OCW3_MASK           0x18U

/**
 * @macroname `OCW3_READ_REGISTER`
 *
 * @description
 * The bit of OCW3 that makes the next read return a register.
 */
#define OCW3_READ_REGISTER  0x02U

/**
 * @macroname `OCW3_READ_ISR`
 *
 * @description
 * The bit of OCW3 that selects the ISR to read (otherwise the IRR).
 */
#define OCW3_READ_ISR       0x01U

/**
 * @macroname `OCW2_EOI`
 *
 * @description
 * The bit of OCW2 for an end of interrupt.
 */
#define OCW2_EOI            0x20U

/**
 * @macroname `OCW2_SPECIFIC`
 *
 * @description
 * The bit of OCW2 for a specific end of interrupt.
 */
#define OCW2_SPECIFIC       0x40U

/**
 * @macroname `OCW2_EOI_MASK`
 *
 * @description
 * The mask of the command bits of OCW2.
 */
#define OCW2_EOI_MASK       0xE0U

/**
 * @macroname `OCW2_LEVEL`
 *
 * @description
 * The mask of the IRQ level of OCW2.
 */
#define OCW2_LEVEL          0x07U

/** @} */

/**
 * @name PIC initialization steps
 * The initialization words a PIC expects.
 * @{
 */

/**
 * @macroname `STEP_NONE`
 *
 * @description
 * No initialization word is expected.
 */
#define STEP_NONE           0U

/**
 * @macroname `STEP_ICW2`
 *
 * @description
 * The next initialization word to be written is ICW2.
 */
#define STEP_ICW2           1U

/**
 * @macroname `STEP_ICW3`
 *
 * @description
 * The next initialization word to be written is ICW3.
 */
#define STEP_ICW3           2U

/**
 * @macroname `STEP_ICW4`
 *
 * @description
 * The next initialization word to be written is ICW4.
 */
#define STEP_ICW4           3U

/** @} */

/**
 * @macroname `PIT_CHANNEL0`
 *
 * @description
 * The I/O port of channel 0 of the PIT.
 */
#define PIT_CHANNEL0        0x40U

/**
 * @name PIT commands
 * The channels, access modes and bits of the PIT.
 * @{
 */

/**
 * @macroname `PIT_CHANNELS`
 *
 * @description
 * The number of the channels of the PIT.
 */
#define PIT_CHANNELS        3U

/**
 * @macroname `PIT_READ_BACK`
 *
 * @description
 * The value of the channel field of a command that is a read-back command.
 */
#define PIT_READ_BACK       3U

/**
 * @macroname `PIT_ACCESS_LATCH`
 *
 * @description
 * The access mode of a counter latch command.
 */
#define PIT_ACCESS_LATCH    0U

/**
 * @macroname `PIT_ACCESS_LSB`
 *
 * @description
 * The access mode: the low byte.
 */
#define PIT_ACCESS_LSB      1U

/**
 * @macroname `PIT_ACCESS_MSB`
 *
 * @description
 * The access mode: the high byte.
 */
#define PIT_ACCESS_MSB      2U

/**
 * @macroname `PIT_ACCESS_WORD`
 *
 * @description
 * The access mode: the low byte, then the high byte.
 */
#define PIT_ACCESS_WORD     3U

/**
 * @macroname `PIT_MAX_COUNT`
 *
 * @description
 * The count that a reload value of 0 means.
 */
#define PIT_MAX_COUNT       0x10000U

/**
 * @macroname `READ_BACK_NO_COUNT`
 *
 * @description
 * The bit of a read-back command: do not latch the count.
 */
#define READ_BACK_NO_COUNT  0x20U

/** @} */

/**
 * @macroname `PORT_B_REFRESH`
 *
 * @description
 * The bit of port B that toggles with the refresh request.
 */
#define PORT_B_REFRESH      0x10U

/**
 * @macroname `REFRESH_PIT_TICKS`
 *
 * @description
 * The period of the toggling of the refresh request, in PIT ticks (about 15
 * us).
 */
#define REFRESH_PIT_TICKS   18U

/**
 * @macroname `LAPIC_MAX_COUNT`
 *
 * @description
 * The largest count of the local APIC timer.
 */
#define LAPIC_MAX_COUNT     0xFFFFFFFFUL

/** \cond */
/* Called from the assembly code of the entry points. */
void hv_main(uint32_t mbi);
void hv_ap_main(uint32_t index);
void hv_interrupt(hv_int_frame_t *frame);

// lapic.c
void lapic_init_base(void);
void lapic_init_cpu(void);
uint32_t lapic_id(void);
void lapic_eoi(void);
void lapic_timer_periodic(uint32_t count);
void lapic_timer_oneshot(uint32_t count);
void lapic_timer_stop(void);
uint32_t lapic_timer_current(void);
void lapic_send_init(uint32_t apic_id);
void lapic_send_sipi(uint32_t apic_id, uint8_t vector);

// pit.c: physical timers
void hv_legacy_pic_disable(void);
void hv_timing_init(void);
uint64_t hv_tsc_hz(void);
uint64_t hv_lapic_hz(void);
void hv_udelay(uint64_t microseconds);
uint64_t hv_tsc_to_pit_ticks(uint64_t tsc_ticks);

// acpi.c
uint32_t acpi_find_cpus(uint32_t apic_ids[], uint32_t max);

// idt.c
void hv_idt_init(void);
void hv_idt_load(void);

// smp.c
void hv_smp_start(uint32_t nb_cpus, const uint32_t apic_ids[]);

// svm.c
bool svm_supported(void);
void svm_init(void);

// npt.c
uint64_t npt_build(uint32_t vm_index, uint64_t ram_base, uint64_t size);

// ioport.c
void io_init_iopm(hv_vm_t *vm);
hv_action_e io_access(hv_vm_t *vm, uint16_t port, uint32_t size,
                      bool is_in, uint32_t *value);

// vpic.c
void vpic_reset(vpic_t *pic);
void vpic_write(vpic_t *pic, uint16_t port, uint8_t value);
uint8_t vpic_read(const vpic_t *pic, uint16_t port);
void vpic_raise(vpic_t *pic, uint32_t irq);
bool vpic_pending(const vpic_t *pic);
bool vpic_next(vpic_t *pic, uint8_t *vector);

// vpit.c
void vpit_reset(vpit_t *pit);
void vpit_write(vpit_t *pit, uint16_t port, uint8_t value);
uint8_t vpit_read(vpit_t *pit, uint16_t port);
void vpit_write_port_b(vpit_t *pit, uint8_t value);
uint8_t vpit_read_port_b(const vpit_t *pit);
bool vpit_timer_running(const vpit_t *pit);

// vuart.c
void vuart_access(uint16_t host_base, uint16_t offset, uint32_t size,
                  bool is_in, uint32_t *value);
/** \endcond */

#endif // HV_ARCH_INTERNAL_H
