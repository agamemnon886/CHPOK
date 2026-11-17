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
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file and
 * its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the enum types are named with _e:
 * hv_action_e (was hv_action_t); the macros, types and declarations are in
 * arch/internal.h, documented there.
 *
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * hypervisor/arch/arm64/mmio.c (the code of the processor architecture is in
 * hypervisor/csps); system registers are accessed with read_sysreg(),
 * write_sysreg() and isb(), shared with the kernel.
 *
 * Modified by Kirill Brazhnikov, 2026-11-16: high and low level requirements of
 * the functions, traced to the system requirements (docs/SRQs), and the steps
 * of their design linked to the LLRs.
 *
 * Modified by Kirill Brazhnikov, 2026-11-17: requirements with more than one
 * "shall" are split into requirements of one "shall" each, and the high level
 * requirements trace to the system requirements split in the same way.
 */

/**
 * @file
 * Data aborts of a guest at stage 2: an access to a guest-physical address
 * that stage 2 doesn't map (stage2.c). Accesses to the emulated device pages
 * (the distributor, the PL011s that aren't passed through, the RTC) are
 * emulated from the syndrome: QEMU gives the register, the size and the
 * direction of single loads and stores (ESR_EL2.ISV). The first access to
 * each register that isn't emulated is reported. Any other access, or one
 * without a syndrome, stops the VM.
 */

#include <arch/internal.h>

static const char *const device_names[HV_MMIO_DEVICES] = {
    "GIC distributor", "UART0", "UART1", "RTC"
};

/**
 * @funcname `log_unhandled`
 *
 * @description
 * Reports the first access of a VM to a register of an emulated device page
 * that isn't emulated (later ones are silent).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in] The VM. |
 * | `device` | `uint32_t` | [in] The emulated device. |
 * | `offset` | `uint32_t` | [in] The register offset in the page. |
 * | `write` | `bool` | [in] true for a write. |
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `device_names` | `const char *const[HV_MMIO_DEVICES]` | The names of the emulated devices |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `word` | `uint32_t` | The index of the register's word |
 * | `bit` | `uint8_t` | The word's bit in its byte of mmio_logged |
 * | `logged` | `uint8_t *` | The byte of mmio_logged |
 *
 * @hlr
 * - \hlrid{HLR_MMIO_LOG_UNHANDLED_01}: `log_unhandled()` shall report the first
 *   access of a VM to a register of an emulated device that is not emulated.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_007}
 *   - \srqref{SRQ_HV_CON_003}
 *
 * @llr
 * - \llrid{LLR_MMIO_LOG_UNHANDLED_01}: If bit `(offset / 4) % 8` of the byte
 *   `(offset / 4) / 8` of `mmio_logged[device]` of the VM is 0 and `write` is
 *   true, `log_unhandled()` shall set that bit and call `hv_printf()` once with
 *   `"[hv] %%s: %%s of %%s register 0x%%x, which isn't emulated (reported
 *   once)\n"`, the name of the VM, `"write"`, `device_names[device]` and
 *   `offset`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOG_UNHANDLED_01}
 * - \llrid{LLR_MMIO_LOG_UNHANDLED_02}: If bit `(offset / 4) % 8` of the byte
 *   `(offset / 4) / 8` of `mmio_logged[device]` of the VM is 0 and `write` is
 *   false, `log_unhandled()` shall set that bit and call `hv_printf()` once
 *   with `"[hv] %%s: %%s of %%s register 0x%%x, which isn't emulated (reported
 *   once)\n"`, the name of the VM, `"read"`, `device_names[device]` and
 *   `offset`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOG_UNHANDLED_01}
 * - \llrid{LLR_MMIO_LOG_UNHANDLED_03}: If bit `(offset / 4) % 8` of the byte
 *   `(offset / 4) / 8` of `mmio_logged[device]` of the VM is 1,
 *   `log_unhandled()` shall leave `mmio_logged` as it is and not call
 *   `hv_printf()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOG_UNHANDLED_01}
 *
 * @design
 * 1. Set `word` to `offset / 4U`. \llrref{LLR_MMIO_LOG_UNHANDLED_01},
 *    \llrref{LLR_MMIO_LOG_UNHANDLED_02}, \llrref{LLR_MMIO_LOG_UNHANDLED_03}
 * 2. Set `bit` to `(uint8_t) (1U << (word % 8U))`.
 *    \llrref{LLR_MMIO_LOG_UNHANDLED_01}, \llrref{LLR_MMIO_LOG_UNHANDLED_02},
 *    \llrref{LLR_MMIO_LOG_UNHANDLED_03}
 * 3. Set `logged` to the address of `vm->arch.mmio_logged[device][word / 8U]`.
 *    \llrref{LLR_MMIO_LOG_UNHANDLED_01}, \llrref{LLR_MMIO_LOG_UNHANDLED_02},
 *    \llrref{LLR_MMIO_LOG_UNHANDLED_03}
 * 4. If `(*logged & bit)` is equal to `0U`: \llrref{LLR_MMIO_LOG_UNHANDLED_01},
 *    \llrref{LLR_MMIO_LOG_UNHANDLED_02}, \llrref{LLR_MMIO_LOG_UNHANDLED_03}
 *    1. Set the bits `bit` in `*logged`.
 *    2. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: %%s of %%s register 0x%%x, which isn't emulated "`
 *       `"(reported once)\n"`, `vm->config->name`, `"write"` if `write` is
 *       true, otherwise `"read"`, `device_names[device]` and `offset`.
 *
 * @calls
 * - `hv_printf()`
 */
static void log_unhandled(hv_vm_t *vm, uint32_t device, uint32_t offset,
                          bool write)
{
    uint32_t word = offset / 4U;
    uint8_t bit = (uint8_t) (1U << (word % 8U));
    uint8_t *logged = &vm->arch.mmio_logged[device][word / 8U];

    if ((*logged & bit) == 0U) {
        *logged |= bit;
        hv_printf("[hv] %s: %s of %s register 0x%x, which isn't emulated "
                  "(reported once)\n", vm->config->name,
                  write ? "write" : "read", device_names[device], offset);
    }
}

/**
 * @funcname `device_of`
 *
 * @description
 * Returns the emulated device page of a guest-physical address.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `const hv_vm_t *` | [in] The VM. |
 * | `ipa` | `uint64_t` | [in] The guest-physical address. |
 *
 * @return The device (HV_MMIO_GICD, _UART0, _UART1, _RTC), or HV_MMIO_DEVICES
 *         if the address isn't in an emulated page.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `device_pages` | `static const uint64_t[HV_MMIO_DEVICES]` | The guest-physical page of each device |
 * | `page` | `uint64_t` | The page of the address |
 * | `device` | `uint32_t` | The device found |
 * | `i` | `uint32_t` | The index of a device |
 *
 * @hlr
 * - \hlrid{HLR_MMIO_DEVICE_OF_01}: `device_of()` shall give the emulated device
 *   of a guest physical address, or none for a page that is not emulated or a
 *   UART that the guest uses directly.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_003}
 *   - \srqref{SRQ_HV_DEV_004}
 *
 * @llr
 * - \llrid{LLR_MMIO_DEVICE_OF_01}: If the page of `ipa` (`ipa` with the bits
 *   `PAGE_OFFSET_MASK` cleared) is `HV_GUEST_GICD` or `HV_GUEST_RTC`,
 *   `device_of()` shall return `HV_MMIO_GICD` or `HV_MMIO_RTC`, in this order.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_DEVICE_OF_01}
 * - \llrid{LLR_MMIO_DEVICE_OF_02}: If the page of `ipa` is `HV_GUEST_UART0` and
 *   `passthrough` of UART 0 of the VM is false, `device_of()` shall return
 *   `HV_MMIO_UART0`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_DEVICE_OF_01}
 * - \llrid{LLR_MMIO_DEVICE_OF_03}: If the page of `ipa` is `HV_GUEST_UART1` and
 *   `passthrough` of UART 1 of the VM is false, `device_of()` shall return
 *   `HV_MMIO_UART1`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_DEVICE_OF_01}
 * - \llrid{LLR_MMIO_DEVICE_OF_04}: If the page of `ipa` is `HV_GUEST_UART0` and
 *   `passthrough` of UART 0 of the VM is true, or is `HV_GUEST_UART1` and
 *   `passthrough` of UART 1 is true, or is none of `HV_GUEST_GICD`,
 *   `HV_GUEST_UART0`, `HV_GUEST_UART1` and `HV_GUEST_RTC`, `device_of()` shall
 *   return `HV_MMIO_DEVICES`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_DEVICE_OF_01}
 *
 * @design
 * 1. Set `page` to `ipa & ~PAGE_OFFSET_MASK`. \llrref{LLR_MMIO_DEVICE_OF_01},
 *    \llrref{LLR_MMIO_DEVICE_OF_02}, \llrref{LLR_MMIO_DEVICE_OF_03},
 *    \llrref{LLR_MMIO_DEVICE_OF_04}
 * 2. Set `device` to `HV_MMIO_DEVICES`. \llrref{LLR_MMIO_DEVICE_OF_04}
 * 3. Set `i` to `0U`; then while `i` is less than `HV_MMIO_DEVICES`, repeat
 *    (after each pass: increment `i`): \llrref{LLR_MMIO_DEVICE_OF_01},
 *    \llrref{LLR_MMIO_DEVICE_OF_02}, \llrref{LLR_MMIO_DEVICE_OF_03},
 *    \llrref{LLR_MMIO_DEVICE_OF_04}
 *    1. If `page` is equal to `device_pages[i]`:
 *       \llrref{LLR_MMIO_DEVICE_OF_01}, \llrref{LLR_MMIO_DEVICE_OF_02},
 *       \llrref{LLR_MMIO_DEVICE_OF_03}
 *       1. Set `device` to `i`.
 * 4. If (`device` is equal to `HV_MMIO_UART0` and
 *    `vm->arch.uart[0].passthrough` is true) or (`device` is equal to
 *    `HV_MMIO_UART1` and `vm->arch.uart[1].passthrough` is true):
 *    \llrref{LLR_MMIO_DEVICE_OF_02}, \llrref{LLR_MMIO_DEVICE_OF_03},
 *    \llrref{LLR_MMIO_DEVICE_OF_04}
 *    1. Set `device` to `HV_MMIO_DEVICES`.
 * 5. Return `device`. \llrref{LLR_MMIO_DEVICE_OF_01},
 *    \llrref{LLR_MMIO_DEVICE_OF_02}, \llrref{LLR_MMIO_DEVICE_OF_03},
 *    \llrref{LLR_MMIO_DEVICE_OF_04}
 *
 * @calls
 * None.
 */
static uint32_t device_of(const hv_vm_t *vm, uint64_t ipa)
{
    // Emulated device pages, by guest-physical address.
    static const uint64_t device_pages[HV_MMIO_DEVICES] = {
        HV_GUEST_GICD, HV_GUEST_UART0, HV_GUEST_UART1, HV_GUEST_RTC
    };
    uint64_t page = ipa & ~PAGE_OFFSET_MASK;
    uint32_t device = HV_MMIO_DEVICES;

    for (uint32_t i = 0U; i < HV_MMIO_DEVICES; i++) {
        if (page == device_pages[i]) {
            device = i;
        }
    }
    // A PL011 that is passed through is mapped and never comes here.
    if (((device == HV_MMIO_UART0) && vm->arch.uart[0].passthrough)
        || ((device == HV_MMIO_UART1) && vm->arch.uart[1].passthrough)) {
        device = HV_MMIO_DEVICES;
    }
    return device;
}

/**
 * @funcname `emulate`
 *
 * @description
 * Emulates an access to an emulated device page.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | [in,out] The VM. |
 * | `device` | `uint32_t` | [in] The emulated device. |
 * | `offset` | `uint32_t` | [in] The register offset. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `write` | `bool` | [in] true for a write. |
 * | `value` | `uint64_t *` | [out] The value written, or where the value read is stored. |
 *
 * @return Whether the register is emulated.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `known` | `bool` | Whether the register is emulated |
 *
 * @hlr
 * - \hlrid{HLR_MMIO_EMULATE_01}: `emulate()` shall emulate an access to an
 *   emulated device and tell whether the register is emulated.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_004}
 *   - \srqref{SRQ_HV_DEV_007}
 * - \hlrid{HLR_MMIO_EMULATE_02}: `emulate()` shall give zero for an access of
 *   the wrong size to a PrimeCell device, as for a register that is not
 *   emulated.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_007}
 *
 * @llr
 * - \llrid{LLR_MMIO_EMULATE_01}: If `device` is `HV_MMIO_GICD`, `emulate()`
 *   shall return the result of `vgicd_access()` for `vm`, `offset`, `size`,
 *   `write` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_EMULATE_01}
 * - \llrid{LLR_MMIO_EMULATE_02}: If `device` is not `HV_MMIO_GICD`, and `size`
 *   is neither 4 nor 1, or `size` is 1 and `offset` is not 0, `emulate()` shall
 *   store 0 at `value` and return false, without calling `vrtc_access()` or
 *   `vuart_access()`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_EMULATE_02}
 * - \llrid{LLR_MMIO_EMULATE_03}: If `device` is `HV_MMIO_RTC`, and `size` is 4,
 *   or `size` is 1 and `offset` is 0, `emulate()` shall return the result of
 *   `vrtc_access()` for `offset`, `write` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_EMULATE_01}
 * - \llrid{LLR_MMIO_EMULATE_04}: If `device` is neither `HV_MMIO_GICD` nor
 *   `HV_MMIO_RTC`, and `size` is 4, or `size` is 1 and `offset` is 0,
 *   `emulate()` shall return the result of `vuart_access()` for the UART
 *   `device` minus `HV_MMIO_UART0` of the VM, `offset`, `write` and `value`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_EMULATE_01}
 *
 * @design
 * 1. If `device` is equal to `HV_MMIO_GICD`: \llrref{LLR_MMIO_EMULATE_01}
 *    1. Call `vgicd_access()` with the arguments `vm`, `offset`, `size`,
 *       `write` and `value`, and store the result in `known`.
 * 2. Otherwise, if `size` is not equal to `4U` and not ((`size` is equal to
 *    `1U` and `offset` is equal to `0U`)): \llrref{LLR_MMIO_EMULATE_02}
 *    1. Set `*value` to `0U`.
 *    2. Set `known` to `false`.
 * 3. Otherwise, if `device` is equal to `HV_MMIO_RTC`:
 *    \llrref{LLR_MMIO_EMULATE_03}
 *    1. Call `vrtc_access()` with the arguments `offset`, `write` and `value`,
 *       and store the result in `known`.
 * 4. Otherwise: \llrref{LLR_MMIO_EMULATE_04}
 *    1. Call `vuart_access()` with the arguments the address of
 *       `vm->arch.uart[device - HV_MMIO_UART0]`, `offset`, `write` and `value`,
 *       and store the result in `known`.
 * 5. Return `known`. \llrref{LLR_MMIO_EMULATE_01},
 *    \llrref{LLR_MMIO_EMULATE_02}, \llrref{LLR_MMIO_EMULATE_03},
 *    \llrref{LLR_MMIO_EMULATE_04}
 *
 * @calls
 * - `vgicd_access()`
 * - `vrtc_access()`
 * - `vuart_access()`
 */
static bool emulate(hv_vm_t *vm, uint32_t device, uint32_t offset,
                    uint32_t size, bool write, uint64_t *value)
{
    bool known;

    if (device == HV_MMIO_GICD) {
        known = vgicd_access(vm, offset, size, write, value);
    } else if ((size != 4U) && !((size == 1U) && (offset == 0U))) {
        // The PrimeCells take word accesses (and byte ones to the data).
        *value = 0U;
        known = false;
    } else if (device == HV_MMIO_RTC) {
        known = vrtc_access(offset, write, value);
    } else {
        known = vuart_access(&vm->arch.uart[device - HV_MMIO_UART0], offset,
                             write, value);
    }
    return known;
}

/**
 * @funcname `load_value`
 *
 * @description
 * Keeps the low bytes of a value, sign-extended or not, as a W or X register
 * value (the result of a load, or the data of a store).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `value` | `uint64_t` | [in] The value. |
 * | `size` | `uint32_t` | [in] The access size in bytes. |
 * | `sign` | `bool` | [in] true to sign-extend. |
 * | `sixty_four` | `bool` | [in] true for an X register, false for a W one. |
 *
 * @return The value.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bits` | `uint32_t` | The access size in bits |
 * | `result` | `uint64_t` | The value |
 * | `top` | `uint32_t` | The top bit's number |
 * | `sign_bit` | `uint64_t` | The top bit |
 * | `mask` | `uint64_t` | The bits of the access |
 *
 * @hlr
 * - \hlrid{HLR_MMIO_LOAD_VALUE_01}: `load_value()` shall give the value of a
 *   load or the data of a store of a size, sign-extended or not, as a 32 or
 *   64-bit register.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_004}
 *
 * @llr
 * - \llrid{LLR_MMIO_LOAD_VALUE_01}: If `size` is less than 8, and `sign` is
 *   false or the top bit of the low `size` bytes of `value` is not set,
 *   `load_value()` shall take as the result the low `size` bytes of `value`,
 *   with the bits above them 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOAD_VALUE_01}
 * - \llrid{LLR_MMIO_LOAD_VALUE_02}: If `size` is less than 8, `sign` is true
 *   and the top bit of the low `size` bytes of `value` is set, `load_value()`
 *   shall take as the result the low `size` bytes of `value`, with the bits
 *   above them 1.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOAD_VALUE_01}
 * - \llrid{LLR_MMIO_LOAD_VALUE_03}: If `size` is 8 or more, `load_value()`
 *   shall take `value` as the result.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOAD_VALUE_01}
 * - \llrid{LLR_MMIO_LOAD_VALUE_04}: `load_value()` shall return the result,
 *   with its bits 63 to 32 cleared if and only if `sixty_four` is false.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_LOAD_VALUE_01}
 *
 * @design
 * 1. Set `bits` to `8U * size`. \llrref{LLR_MMIO_LOAD_VALUE_01},
 *    \llrref{LLR_MMIO_LOAD_VALUE_02}, \llrref{LLR_MMIO_LOAD_VALUE_03}
 * 2. Set `result` to `value`. \llrref{LLR_MMIO_LOAD_VALUE_01},
 *    \llrref{LLR_MMIO_LOAD_VALUE_02}, \llrref{LLR_MMIO_LOAD_VALUE_03}
 * 3. If `bits` is less than `64U`: \llrref{LLR_MMIO_LOAD_VALUE_01},
 *    \llrref{LLR_MMIO_LOAD_VALUE_02}
 *    1. Set `top` to `bits - 1U`.
 *    2. Set `sign_bit` to `1UL << top`.
 *    3. Set `mask` to `(sign_bit << 1) - 1U`.
 *    4. Keep only the bits `mask` of `result`.
 *    5. If `sign` is true and `(result & sign_bit)` is not equal to `0U`:
 *       \llrref{LLR_MMIO_LOAD_VALUE_02}
 *       1. Set the bits `~mask` in `result`.
 * 4. If `sixty_four` is false: \llrref{LLR_MMIO_LOAD_VALUE_04}
 *    1. Keep only the bits `0xFFFFFFFFUL` of `result`.
 * 5. Return `result`. \llrref{LLR_MMIO_LOAD_VALUE_04}
 *
 * @calls
 * None.
 */
static uint64_t load_value(uint64_t value, uint32_t size, bool sign,
                           bool sixty_four)
{
    uint32_t bits = 8U * size;
    uint64_t result = value;

    if (bits < 64U) {
        uint32_t top = bits - 1U;
        uint64_t sign_bit = 1UL << top;
        uint64_t mask = (sign_bit << 1) - 1U;

        result &= mask;
        if (sign && ((result & sign_bit) != 0U)) {
            result |= ~mask;
        }
    }
    if (!sixty_four) {
        result &= 0xFFFFFFFFUL;
    }
    return result;
}

/**
 * @funcname `mmio_abort`
 *
 * @description
 * Handles a data abort of the guest at stage 2: emulates the access to an
 * emulated device page and goes on after the instruction, or stops the VM.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vcpu` | `hv_vcpu_t *` | [in,out] The vCPU. |
 * | `esr` | `uint64_t` | [in] ESR_EL2 of the abort. |
 *
 * @return HV_CONTINUE, or HV_STOP.
 *
 * @globals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `device_names` | `const char *const[HV_MMIO_DEVICES]` | The names of the emulated devices |
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `vm` | `hv_vm_t *` | The vCPU's VM |
 * | `regs` | `hv_guest_regs_t *` | The guest's registers |
 * | `hpfar` | `uint64_t` | HPFAR_EL2: the faulting page |
 * | `far` | `uint64_t` | FAR_EL2: the faulting address |
 * | `ipa` | `uint64_t` | The guest-physical address |
 * | `device` | `uint32_t` | The emulated device |
 * | `action` | `hv_action_e` | What the run loop does next |
 * | `sas` | `uint32_t` | The access size field (log2) |
 * | `size` | `uint32_t` | The access size in bytes |
 * | `reg` | `uint32_t` | The register of the access (31: XZR) |
 * | `write` | `bool` | Whether it is a write |
 * | `offset` | `uint32_t` | The offset in the page |
 * | `value` | `uint64_t` | The value written or read |
 *
 * @hlr
 * - \hlrid{HLR_MMIO_MMIO_ABORT_01}: `mmio_abort()` shall emulate an access of a
 *   guest to an emulated device that has a syndrome and make the guest go on
 *   after the instruction.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_DEV_004}
 * - \hlrid{HLR_MMIO_MMIO_ABORT_02}: `mmio_abort()` shall stop the VM, with a
 *   message and its registers, for an access outside its RAM and devices, or by
 *   an instruction without a syndrome.
 *   <br>Traces to:
 *   - \srqref{SRQ_HV_MEM_002}
 *   - \srqref{SRQ_HV_ERR_001}
 *   - \srqref{SRQ_HV_DEV_008}
 *
 * @llr
 * - \llrid{LLR_MMIO_MMIO_ABORT_01}: If `device_of()` returns `HV_MMIO_DEVICES`
 *   for the guest physical address (the bits of HPFAR_EL2 from
 *   `HPFAR_FIPA_SHIFT` up, shifted left by 12, with the bits `PAGE_OFFSET_MASK`
 *   of FAR_EL2), or `ESR_S1PTW` is set in `esr`, `mmio_abort()` shall call
 *   `hv_printf()` with `"[hv] %%s: access to guest-physical address 0x%%lx
 *   outside its RAM (ESR 0x%%lx)\n"`, the name of the VM, the address and
 *   `esr`, then `hv_report_state()` with `vcpu`, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_02}
 * - \llrid{LLR_MMIO_MMIO_ABORT_02}: If `device_of()` returns a device for the
 *   guest physical address, `ESR_S1PTW` is not set and `ESR_ISV` is not set in
 *   `esr`, `mmio_abort()` shall call `hv_printf()` with `"[hv] %%s: access to
 *   %%s at 0x%%lx by an instruction that can't be emulated (ESR 0x%%lx)\n"`,
 *   the name of the VM, `device_names` of the device, the address and `esr`,
 *   then `hv_report_state()` with `vcpu`, and return `HV_STOP`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_02}
 * - \llrid{LLR_MMIO_MMIO_ABORT_03}: If `device_of()` returns a device,
 *   `ESR_S1PTW` is not set, `ESR_ISV` and `ESR_WNR` are set in `esr` and the
 *   register of the access (the field at `ESR_SRT_SHIFT`) is not `REG_XZR`,
 *   `mmio_abort()` shall call `emulate()` once with the VM, the device, the
 *   bits `PAGE_OFFSET_MASK` of the address, the size (1 shifted left by the
 *   field at `ESR_SAS_SHIFT`), true and the result of `load_value()` for that
 *   register of the guest, the size, false and true.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_01}
 * - \llrid{LLR_MMIO_MMIO_ABORT_04}: If `device_of()` returns a device,
 *   `ESR_S1PTW` is not set, `ESR_ISV` and `ESR_WNR` are set in `esr` and the
 *   register of the access is `REG_XZR`, `mmio_abort()` shall call `emulate()`
 *   once with the VM, the device, the bits `PAGE_OFFSET_MASK` of the address,
 *   the size, true and the value 0.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_01}
 * - \llrid{LLR_MMIO_MMIO_ABORT_05}: If `device_of()` returns a device,
 *   `ESR_S1PTW` is not set, `ESR_ISV` is set and `ESR_WNR` is not set in `esr`,
 *   `mmio_abort()` shall call `emulate()` once with the VM, the device, the
 *   bits `PAGE_OFFSET_MASK` of the address, the size, false and a value of 0 to
 *   replace.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_01}
 * - \llrid{LLR_MMIO_MMIO_ABORT_06}: If and only if `device_of()` returns a
 *   device, `ESR_S1PTW` is not set, `ESR_ISV` is set, `ESR_WNR` is not set in
 *   `esr` and the register of the access is not `REG_XZR`, `mmio_abort()` shall
 *   store in that register of the guest the result of `load_value()` for the
 *   value that `emulate()` gives, the size, whether `ESR_SSE` is set and
 *   whether `ESR_SF` is set in `esr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_01}
 * - \llrid{LLR_MMIO_MMIO_ABORT_07}: If and only if `emulate()` returns false,
 *   `mmio_abort()` shall call `log_unhandled()` once with the VM, the device,
 *   the bits `PAGE_OFFSET_MASK` of the address and whether `ESR_WNR` is set in
 *   `esr`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_01}
 * - \llrid{LLR_MMIO_MMIO_ABORT_08}: If `device_of()` returns a device,
 *   `ESR_S1PTW` is not set and `ESR_ISV` is set in `esr`, `mmio_abort()` shall
 *   add 4 to `elr` of the guest and return `HV_CONTINUE`.
 *   <br>Traces to:
 *   - \hlrref{HLR_MMIO_MMIO_ABORT_01}
 *
 * @design
 * 1. Set `vm` to `vcpu->vm`. \llrref{LLR_MMIO_MMIO_ABORT_01},
 *    \llrref{LLR_MMIO_MMIO_ABORT_02}, \llrref{LLR_MMIO_MMIO_ABORT_03},
 *    \llrref{LLR_MMIO_MMIO_ABORT_04}, \llrref{LLR_MMIO_MMIO_ABORT_05},
 *    \llrref{LLR_MMIO_MMIO_ABORT_06}, \llrref{LLR_MMIO_MMIO_ABORT_07},
 *    \llrref{LLR_MMIO_MMIO_ABORT_08}
 * 2. Set `regs` to the address of `vcpu->arch.regs`.
 *    \llrref{LLR_MMIO_MMIO_ABORT_03}, \llrref{LLR_MMIO_MMIO_ABORT_06},
 *    \llrref{LLR_MMIO_MMIO_ABORT_08}
 * 3. Set `hpfar` to `read_sysreg(hpfar_el2)`. \llrref{LLR_MMIO_MMIO_ABORT_01},
 *    \llrref{LLR_MMIO_MMIO_ABORT_02}, \llrref{LLR_MMIO_MMIO_ABORT_03},
 *    \llrref{LLR_MMIO_MMIO_ABORT_04}, \llrref{LLR_MMIO_MMIO_ABORT_05}
 * 4. Set `far` to `read_sysreg(far_el2)`. \llrref{LLR_MMIO_MMIO_ABORT_01},
 *    \llrref{LLR_MMIO_MMIO_ABORT_02}, \llrref{LLR_MMIO_MMIO_ABORT_03},
 *    \llrref{LLR_MMIO_MMIO_ABORT_04}, \llrref{LLR_MMIO_MMIO_ABORT_05}
 * 5. Set `ipa` to
 *    `((hpfar >> HPFAR_FIPA_SHIFT) << 12) | (far & PAGE_OFFSET_MASK)`.
 *    \llrref{LLR_MMIO_MMIO_ABORT_01}, \llrref{LLR_MMIO_MMIO_ABORT_02},
 *    \llrref{LLR_MMIO_MMIO_ABORT_03}, \llrref{LLR_MMIO_MMIO_ABORT_04},
 *    \llrref{LLR_MMIO_MMIO_ABORT_05}
 * 6. Call `device_of()` with the arguments `vm` and `ipa`, and store the result
 *    in `device`. \llrref{LLR_MMIO_MMIO_ABORT_01},
 *    \llrref{LLR_MMIO_MMIO_ABORT_02}, \llrref{LLR_MMIO_MMIO_ABORT_03},
 *    \llrref{LLR_MMIO_MMIO_ABORT_04}, \llrref{LLR_MMIO_MMIO_ABORT_05},
 *    \llrref{LLR_MMIO_MMIO_ABORT_07}
 * 7. Set `action` to `HV_CONTINUE`. \llrref{LLR_MMIO_MMIO_ABORT_08}
 * 8. If `device` is equal to `HV_MMIO_DEVICES` or `(esr & ESR_S1PTW)` is not
 *    equal to `0U`: \llrref{LLR_MMIO_MMIO_ABORT_01}
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: access to guest-physical address 0x%%lx "`
 *       `"outside its RAM (ESR 0x%%lx)\n"`, `vm->config->name`, `ipa` and
 *       `esr`.
 *    2. Call `hv_report_state()` with the argument `vcpu`.
 *    3. Set `action` to `HV_STOP`.
 * 9. Otherwise, if `(esr & ESR_ISV)` is equal to `0U`:
 *    \llrref{LLR_MMIO_MMIO_ABORT_02}
 *    1. Call `hv_printf()` with the arguments
 *       `"[hv] %%s: access to %%s at 0x%%lx by an instruction "`
 *       `"that can't be emulated (ESR 0x%%lx)\n"`, `vm->config->name`,
 *       `device_names[device]`, `ipa` and `esr`.
 *    2. Call `hv_report_state()` with the argument `vcpu`.
 *    3. Set `action` to `HV_STOP`.
 * 10. Otherwise: \llrref{LLR_MMIO_MMIO_ABORT_03},
 *     \llrref{LLR_MMIO_MMIO_ABORT_04}, \llrref{LLR_MMIO_MMIO_ABORT_05},
 *     \llrref{LLR_MMIO_MMIO_ABORT_06}, \llrref{LLR_MMIO_MMIO_ABORT_07},
 *     \llrref{LLR_MMIO_MMIO_ABORT_08}
 *     1. Set `sas` to `(uint32_t) ((esr >> ESR_SAS_SHIFT) & ESR_SAS_MASK)`.
 *     2. Set `size` to `(uint32_t) 1U << sas`.
 *     3. Set `reg` to `(uint32_t) ((esr >> ESR_SRT_SHIFT) & ESR_SRT_MASK)`.
 *        \llrref{LLR_MMIO_MMIO_ABORT_03}, \llrref{LLR_MMIO_MMIO_ABORT_04},
 *        \llrref{LLR_MMIO_MMIO_ABORT_06}
 *     4. Set `write` to `(esr & ESR_WNR) != 0U`.
 *        \llrref{LLR_MMIO_MMIO_ABORT_03}, \llrref{LLR_MMIO_MMIO_ABORT_04},
 *        \llrref{LLR_MMIO_MMIO_ABORT_05}, \llrref{LLR_MMIO_MMIO_ABORT_06},
 *        \llrref{LLR_MMIO_MMIO_ABORT_07}
 *     5. Set `offset` to `(uint32_t) (ipa & PAGE_OFFSET_MASK)`.
 *     6. Set `value` to `0U`. \llrref{LLR_MMIO_MMIO_ABORT_04},
 *        \llrref{LLR_MMIO_MMIO_ABORT_05}
 *     7. If `write` is true and `reg` is not equal to `REG_XZR`:
 *        \llrref{LLR_MMIO_MMIO_ABORT_03}, \llrref{LLR_MMIO_MMIO_ABORT_04},
 *        \llrref{LLR_MMIO_MMIO_ABORT_05}
 *        1. Call `load_value()` with the arguments `regs->x[reg]`, `size`,
 *           `false` and `true`, and store the result in `value`.
 *     8. If `emulate(vm, device, offset, size, write, &value)` is false:
 *        \llrref{LLR_MMIO_MMIO_ABORT_03}, \llrref{LLR_MMIO_MMIO_ABORT_04},
 *        \llrref{LLR_MMIO_MMIO_ABORT_05}, \llrref{LLR_MMIO_MMIO_ABORT_07}
 *        1. Call `log_unhandled()` with the arguments `vm`, `device`, `offset`
 *           and `write`.
 *     9. If `write` is false and `reg` is not equal to `REG_XZR`:
 *        \llrref{LLR_MMIO_MMIO_ABORT_06}
 *        1. Call `load_value()` with the arguments `value`, `size`,
 *           `(esr & ESR_SSE) != 0U` and `(esr & ESR_SF) != 0U`, and store the
 *           result in `regs->x[reg]`.
 *     10. Add `4U` to `regs->elr`. \llrref{LLR_MMIO_MMIO_ABORT_08}
 * 11. Return `action`. \llrref{LLR_MMIO_MMIO_ABORT_01},
 *     \llrref{LLR_MMIO_MMIO_ABORT_02}, \llrref{LLR_MMIO_MMIO_ABORT_08}
 *
 * @calls
 * - `device_of()`
 * - `hv_printf()`
 * - `hv_report_state()`
 * - `load_value()`
 * - `emulate()`
 * - `log_unhandled()`
 */
hv_action_e mmio_abort(hv_vcpu_t *vcpu, uint64_t esr)
{
    hv_vm_t *vm = vcpu->vm;
    hv_guest_regs_t *regs = &vcpu->arch.regs;
    uint64_t hpfar = read_sysreg(hpfar_el2);
    uint64_t far = read_sysreg(far_el2);
    uint64_t ipa = ((hpfar >> HPFAR_FIPA_SHIFT) << 12)
                   | (far & PAGE_OFFSET_MASK);
    uint32_t device = device_of(vm, ipa);
    hv_action_e action = HV_CONTINUE;

    if ((device == HV_MMIO_DEVICES) || ((esr & ESR_S1PTW) != 0U)) {
        hv_printf("[hv] %s: access to guest-physical address 0x%lx "
                  "outside its RAM (ESR 0x%lx)\n", vm->config->name, ipa,
                  esr);
        hv_report_state(vcpu);
        action = HV_STOP;
    } else if ((esr & ESR_ISV) == 0U) {
        hv_printf("[hv] %s: access to %s at 0x%lx by an instruction "
                  "that can't be emulated (ESR 0x%lx)\n", vm->config->name,
                  device_names[device], ipa, esr);
        hv_report_state(vcpu);
        action = HV_STOP;
    } else {
        uint32_t sas = (uint32_t) ((esr >> ESR_SAS_SHIFT) & ESR_SAS_MASK);
        uint32_t size = (uint32_t) 1U << sas;
        uint32_t reg = (uint32_t) ((esr >> ESR_SRT_SHIFT) & ESR_SRT_MASK);
        bool write = (esr & ESR_WNR) != 0U;
        uint32_t offset = (uint32_t) (ipa & PAGE_OFFSET_MASK);
        uint64_t value = 0U;

        if (write && (reg != REG_XZR)) {
            value = load_value(regs->x[reg], size, false, true);
        }
        if (!emulate(vm, device, offset, size, write, &value)) {
            log_unhandled(vm, device, offset, write);
        }
        if (!write && (reg != REG_XZR)) {
            regs->x[reg] = load_value(value, size, (esr & ESR_SSE) != 0U,
                                      (esr & ESR_SF) != 0U);
        }
        regs->elr += 4U;        // Past the load or store
    }
    return action;
}
