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
 * Modified by Kirill Brazhnikov, 2026-11-08: moved from hypervisor/src/ to
 * arch/x86_64/. Doxygen headers for the file and its functions.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros, types and external
 * declarations are in arch/internal.h, documented there.
 */

/**
 * @file
 * Virtual 8259A PIC pair (master at 0x20, slave at 0xA0, cascaded on
 * IRQ 2), as far as JetOS and similar guests use it: initialization
 * (ICW1-ICW4), the mask (OCW1), non-specific and specific EOI (OCW2), reading
 * IRR or ISR (OCW3), and automatic EOI. Interrupts are edge-triggered with
 * fixed priorities (IRQ 0 highest). Rotation, special mask mode and polling
 * are not emulated.
 */

#include <arch/internal.h>

/**
 * @funcname `chip_reset`
 *
 * @description
 * Resets one virtual PIC chip: nothing requested or in service, everything
 * masked, not in initialization.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `vpic_chip_t *` | [out] The chip. |
 * | `vector_base` | `uint8_t` | [in] The vector of its IRQ 0. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `chip->irr` to `0U`.
 * 2. Set `chip->isr` to `0U`.
 * 3. Set `chip->imr` to `0xFFU`.
 * 4. Set `chip->vector_base` to `vector_base`.
 * 5. Set `chip->init_step` to `STEP_NONE`.
 * 6. Set `chip->single` to `false`.
 * 7. Set `chip->icw4_needed` to `false`.
 * 8. Set `chip->read_isr` to `false`.
 * 9. Set `chip->auto_eoi` to `false`.
 *
 * @calls
 * None.
 */
static void chip_reset(vpic_chip_t *chip, uint8_t vector_base)
{
    chip->irr = 0U;
    chip->isr = 0U;
    chip->imr = 0xFFU;
    chip->vector_base = vector_base;
    chip->init_step = STEP_NONE;
    chip->single = false;
    chip->icw4_needed = false;
    chip->read_isr = false;
    chip->auto_eoi = false;
}

/**
 * @funcname `vpic_reset`
 *
 * @description
 * Resets the virtual PIC pair of a VM, as at power-on (on load and reset).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `vpic_t *` | [out] The virtual PIC pair. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `chip_reset()` with the arguments the address of `pic->chip[0]` and
 *    `0x08U`.
 * 2. Call `chip_reset()` with the arguments the address of `pic->chip[1]` and
 *    `0x70U`.
 *
 * @calls
 * - `chip_reset()`
 */
void vpic_reset(vpic_t *pic)
{
    // The BIOS's vectors; guests program their own.
    chip_reset(&pic->chip[0], 0x08U);
    chip_reset(&pic->chip[1], 0x70U);
}

/**
 * @funcname `chip_of`
 *
 * @description
 * Returns the chip that a PIC port belongs to.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `vpic_t *` | [in] The virtual PIC pair. |
 * | `port` | `uint16_t` | [in] The port: 0x20-0x21 or 0xA0-0xA1. |
 *
 * @return The slave for 0xAx, the master otherwise.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return the address of `pic->chip[1]` if `(port & 0xF0U)` is equal to
 *    `PIC_SLAVE`, otherwise the address of `pic->chip[0]`.
 *
 * @calls
 * None.
 */
static vpic_chip_t *chip_of(vpic_t *pic, uint16_t port)
{
    return ((port & 0xF0U) == PIC_SLAVE) ? &pic->chip[1] : &pic->chip[0];
}

/**
 * @funcname `first_bit`
 *
 * @description
 * Returns the lowest set bit of an 8-bit register: the IRQ of highest
 * priority.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bits` | `uint8_t` | [in] The register. |
 *
 * @return The bit number, or 8 if no bit is set.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | The bit being checked |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `n` to `0U`.
 * 2. While `n` is less than `PIC_IRQS` and `(bits & (1U << n))` is equal to
 *    `0U`:
 *    1. Increment `n`.
 * 3. Return `n`.
 *
 * @calls
 * None.
 */
static uint32_t first_bit(uint8_t bits)
{
    uint32_t n = 0U;

    while ((n < PIC_IRQS) && ((bits & (1U << n)) == 0U)) {
        n++;
    }
    return n;
}

/**
 * @funcname `pic_command`
 *
 * @description
 * Handles a write to a chip's command port: ICW1 (start of
 * initialization), OCW3 (register to read) or OCW2 (end of interrupt).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `vpic_chip_t *` | [out] The chip. |
 * | `value` | `uint8_t` | [in] The value written. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `n` | `uint32_t` | The in-service IRQ of highest priority |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `(value & ICW1_INIT)` is not equal to `0U`:
 *    1. Set `chip->irr` to `0U`.
 *    2. Set `chip->isr` to `0U`.
 *    3. Set `chip->imr` to `0U`.
 *    4. Set `chip->read_isr` to `false`.
 *    5. Set `chip->auto_eoi` to `false`.
 *    6. Set `chip->single` to `(value & ICW1_SINGLE) != 0U`.
 *    7. Set `chip->icw4_needed` to `(value & ICW1_ICW4) != 0U`.
 *    8. Set `chip->init_step` to `STEP_ICW2`.
 * 2. Otherwise, if `(value & OCW3_MASK)` is equal to `OCW3_SELECT`:
 *    1. If `(value & OCW3_READ_REGISTER)` is not equal to `0U`:
 *       1. Set `chip->read_isr` to `(value & OCW3_READ_ISR) != 0U`.
 * 3. Otherwise, if `(value & OCW2_EOI_MASK)` is equal to `OCW2_EOI`:
 *    1. Call `first_bit()` with the argument `chip->isr`, and store the result
 *       in `n`.
 *    2. If `n` is less than `PIC_IRQS`:
 *       1. Keep only the bits `(uint8_t) ~(1U << n)` of `chip->isr`.
 * 4. Otherwise, if `(value & OCW2_EOI_MASK)` is equal to
 *    `(OCW2_EOI | OCW2_SPECIFIC)`:
 *    1. Keep only the bits `(uint8_t) ~(1U << (value & OCW2_LEVEL))` of
 *       `chip->isr`.
 * 5. Otherwise:
 *    1. Do nothing.
 *
 * @calls
 * - `first_bit()`
 */
static void pic_command(vpic_chip_t *chip, uint8_t value)
{
    if ((value & ICW1_INIT) != 0U) {
        chip->irr = 0U;
        chip->isr = 0U;
        chip->imr = 0U;
        chip->read_isr = false;
        chip->auto_eoi = false;
        chip->single = (value & ICW1_SINGLE) != 0U;
        chip->icw4_needed = (value & ICW1_ICW4) != 0U;
        chip->init_step = STEP_ICW2;
    } else if ((value & OCW3_MASK) == OCW3_SELECT) {
        if ((value & OCW3_READ_REGISTER) != 0U) {
            chip->read_isr = (value & OCW3_READ_ISR) != 0U;
        }
    } else if ((value & OCW2_EOI_MASK) == OCW2_EOI) {
        uint32_t n = first_bit(chip->isr);

        if (n < PIC_IRQS) {
            chip->isr &= (uint8_t) ~(1U << n);
        }
    } else if ((value & OCW2_EOI_MASK) == (OCW2_EOI | OCW2_SPECIFIC)) {
        chip->isr &= (uint8_t) ~(1U << (value & OCW2_LEVEL));
    } else {
        // Rotation and priority commands are ignored.
    }
}

/**
 * @funcname `pic_data`
 *
 * @description
 * Handles a write to a chip's data port: the next initialization word
 * (ICW2-ICW4) during initialization, otherwise the mask (OCW1).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `vpic_chip_t *` | [in,out] The chip. |
 * | `value` | `uint8_t` | [in] The value written. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `step` | `uint8_t` | The initialization word expected |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `step` to `chip->init_step`.
 * 2. If `step` is equal to `STEP_ICW2`:
 *    1. Set `chip->vector_base` to `(uint8_t) (value & 0xF8U)`.
 *    2. If `chip->single` is false:
 *       1. Set `chip->init_step` to `STEP_ICW3`.
 *    3. Otherwise, if `chip->icw4_needed` is true:
 *       1. Set `chip->init_step` to `STEP_ICW4`.
 *    4. Otherwise:
 *       1. Set `chip->init_step` to `STEP_NONE`.
 * 3. Otherwise, if `step` is equal to `STEP_ICW3`:
 *    1. Set `chip->init_step` to `STEP_ICW4` if `chip->icw4_needed` is true,
 *       otherwise `STEP_NONE`.
 * 4. Otherwise, if `step` is equal to `STEP_ICW4`:
 *    1. Set `chip->auto_eoi` to `(value & ICW4_AUTO_EOI) != 0U`.
 *    2. Set `chip->init_step` to `STEP_NONE`.
 * 5. Otherwise:
 *    1. Set `chip->imr` to `value`.
 *
 * @calls
 * None.
 */
static void pic_data(vpic_chip_t *chip, uint8_t value)
{
    uint8_t step = chip->init_step;

    if (step == STEP_ICW2) {
        // Vector of IRQ 0 of the chip (the low 3 bits are the IRQ).
        chip->vector_base = (uint8_t) (value & 0xF8U);
        if (!chip->single) {
            chip->init_step = STEP_ICW3;
        } else if (chip->icw4_needed) {
            chip->init_step = STEP_ICW4;
        } else {
            chip->init_step = STEP_NONE;
        }
    } else if (step == STEP_ICW3) {
        // The cascade wiring is fixed (slave on IRQ 2): nothing to keep.
        chip->init_step = chip->icw4_needed ? STEP_ICW4 : STEP_NONE;
    } else if (step == STEP_ICW4) {
        chip->auto_eoi = (value & ICW4_AUTO_EOI) != 0U;
        chip->init_step = STEP_NONE;
    } else {
        chip->imr = value;  // OCW1
    }
}

/**
 * @funcname `vpic_write`
 *
 * @description
 * Handles a write of the guest to a PIC port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `vpic_t *` | [in] The virtual PIC pair. |
 * | `port` | `uint16_t` | [in] The port. |
 * | `value` | `uint8_t` | [in] The value written. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `vpic_chip_t *` | The chip of the port |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Call `chip_of()` with the arguments `pic` and `port`, and store the result
 *    in `chip`.
 * 2. If `(port & 1U)` is equal to `0U`:
 *    1. Call `pic_command()` with the arguments `chip` and `value`.
 * 3. Otherwise:
 *    1. Call `pic_data()` with the arguments `chip` and `value`.
 *
 * @calls
 * - `chip_of()`
 * - `pic_command()`
 * - `pic_data()`
 */
void vpic_write(vpic_t *pic, uint16_t port, uint8_t value)
{
    vpic_chip_t *chip = chip_of(pic, port);

    if ((port & 1U) == 0U) {
        pic_command(chip, value);
    } else {
        pic_data(chip, value);
    }
}

/**
 * @funcname `vpic_read`
 *
 * @description
 * Handles a read of the guest from a PIC port: the mask from the data port,
 * the IRR or ISR (as selected by OCW3) from the command port.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `const vpic_t *` | [in] The virtual PIC pair. |
 * | `port` | `uint16_t` | [in] The port. |
 *
 * @return The value read.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `const vpic_chip_t *` | The chip of the port |
 * | `value` | `uint8_t` | The value read |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `chip` to the address of `pic->chip[1]` if `(port & 0xF0U)` is equal
 *    to `PIC_SLAVE`, otherwise the address of `pic->chip[0]`.
 * 2. If `(port & 1U)` is not equal to `0U`:
 *    1. Set `value` to `chip->imr`.
 * 3. Otherwise, if `chip->read_isr` is true:
 *    1. Set `value` to `chip->isr`.
 * 4. Otherwise:
 *    1. Set `value` to `chip->irr`.
 * 5. Return `value`.
 *
 * @calls
 * None.
 */
uint8_t vpic_read(const vpic_t *pic, uint16_t port)
{
    const vpic_chip_t *chip = ((port & 0xF0U) == PIC_SLAVE)
                                   ? &pic->chip[1] : &pic->chip[0];
    uint8_t value;

    if ((port & 1U) != 0U) {
        value = chip->imr;
    } else if (chip->read_isr) {
        value = chip->isr;
    } else {
        value = chip->irr;
    }
    return value;
}

/**
 * @funcname `vpic_raise`
 *
 * @description
 * Requests an interrupt (edge-triggered): sets its bit in the request
 * register of its chip.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `vpic_t *` | [in,out] The virtual PIC pair. |
 * | `irq` | `uint32_t` | [in] The IRQ, 0-15. |
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. If `irq` is less than `PIC_IRQS`:
 *    1. Set the bits `(uint8_t) (1U << irq)` in `pic->chip[0].irr`.
 * 2. Otherwise, if `irq` is less than `(2U * PIC_IRQS)`:
 *    1. Set the bits `(uint8_t) (1U << (irq - PIC_IRQS))` in
 *       `pic->chip[1].irr`.
 * 3. Otherwise:
 *    1. Do nothing.
 *
 * @calls
 * None.
 */
void vpic_raise(vpic_t *pic, uint32_t irq)
{
    if (irq < PIC_IRQS) {
        pic->chip[0].irr |= (uint8_t) (1U << irq);
    } else if (irq < (2U * PIC_IRQS)) {
        pic->chip[1].irr |= (uint8_t) (1U << (irq - PIC_IRQS));
    } else {
        // No such IRQ.
    }
}

/**
 * @funcname `chip_request`
 *
 * @description
 * Returns a chip's request of highest priority that is unmasked and not
 * blocked by an interrupt in service at the same or a higher priority.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `const vpic_chip_t *` | [in] The chip. |
 * | `extra` | `uint8_t` | [in] Request bits to add (the slave's request on the master's IRQ 2). |
 *
 * @return The IRQ of the request, or 8 if there is none.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `unmasked` | `uint8_t` | The IRQs not masked |
 * | `request` | `uint32_t` | The unmasked request of highest priority |
 * | `in_service` | `uint32_t` | The in-service IRQ of highest priority |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `unmasked` to `(uint8_t) ~chip->imr`.
 * 2. Call `first_bit()` with the argument
 *    `(uint8_t) ((chip->irr | extra) & unmasked)`, and store the result in
 *    `request`.
 * 3. Call `first_bit()` with the argument `chip->isr`, and store the result in
 *    `in_service`.
 * 4. Return `request` if `request` is less than `in_service`, otherwise
 *    `PIC_IRQS`.
 *
 * @calls
 * - `first_bit()`
 */
static uint32_t chip_request(const vpic_chip_t *chip, uint8_t extra)
{
    uint8_t unmasked = (uint8_t) ~chip->imr;
    uint32_t request = first_bit((uint8_t) ((chip->irr | extra)
                                            & unmasked));
    uint32_t in_service = first_bit(chip->isr);

    // Only a request of higher priority than all in service is delivered.
    return (request < in_service) ? request : PIC_IRQS;
}

/**
 * @funcname `cascade_bit`
 *
 * @description
 * Returns the IRQ 2 request bit of the master: set while the slave has a
 * request to deliver.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `const vpic_t *` | [in] The virtual PIC pair. |
 *
 * @return The IRQ 2 bit, or 0.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bit` | `uint8_t` | The IRQ 2 bit, or 0 |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `bit` to `0U`.
 * 2. If `chip_request(&pic->chip[1], 0U)` is less than `PIC_IRQS`:
 *    1. Set `bit` to `(uint8_t) (1U << PIC_CASCADE_IRQ)`.
 * 3. Return `bit`.
 *
 * @calls
 * - `chip_request()`
 */
static uint8_t cascade_bit(const vpic_t *pic)
{
    uint8_t bit = 0U;

    if (chip_request(&pic->chip[1], 0U) < PIC_IRQS) {
        bit = (uint8_t) (1U << PIC_CASCADE_IRQ);
    }
    return bit;
}

/**
 * @funcname `vpic_pending`
 *
 * @description
 * Tells whether the PIC pair has an interrupt to deliver to the vCPU.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `const vpic_t *` | [in] The virtual PIC pair. |
 *
 * @return true if an interrupt can be delivered.
 *
 * @globals
 * None.
 *
 * @locals
 * None.
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Return `chip_request(&pic->chip[0], cascade_bit(pic)) < PIC_IRQS`.
 *
 * @calls
 * - `chip_request()`
 * - `cascade_bit()`
 */
bool vpic_pending(const vpic_t *pic)
{
    return chip_request(&pic->chip[0], cascade_bit(pic)) < PIC_IRQS;
}

/**
 * @funcname `acknowledge`
 *
 * @description
 * Acknowledges a request of a chip, as the CPU's interrupt acknowledge
 * does: it moves from the IRR to the ISR (not with automatic EOI).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `chip` | `vpic_chip_t *` | [in,out] The chip. |
 * | `n` | `uint32_t` | [in] The IRQ of the chip. |
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `bit` | `uint8_t` | The IRQ's bit |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `bit` to `(uint8_t) (1U << n)`.
 * 2. Keep only the bits `(uint8_t) ~bit` of `chip->irr`.
 * 3. If `chip->auto_eoi` is false:
 *    1. Set the bits `bit` in `chip->isr`.
 *
 * @calls
 * None.
 */
static void acknowledge(vpic_chip_t *chip, uint32_t n)
{
    uint8_t bit = (uint8_t) (1U << n);

    chip->irr &= (uint8_t) ~bit;
    if (!chip->auto_eoi) {
        chip->isr |= bit;
    }
}

/**
 * @funcname `vpic_next`
 *
 * @description
 * Takes the interrupt to deliver next: acknowledges it and gives its
 * vector.
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `pic` | `vpic_t *` | [in] The virtual PIC pair. |
 * | `vector` | `uint8_t *` | [out] Where the vector is stored. |
 *
 * @return false if there is no interrupt to deliver.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `master` | `vpic_chip_t *` | The master chip |
 * | `slave` | `vpic_chip_t *` | The slave chip |
 * | `n` | `uint32_t` | The master's request |
 * | `found` | `bool` | Whether there is an interrupt |
 * | `m` | `uint32_t` | The slave's request |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `master` to the address of `pic->chip[0]`.
 * 2. Set `slave` to the address of `pic->chip[1]`.
 * 3. Call `chip_request()` with the arguments `master` and `cascade_bit(pic)`,
 *    and store the result in `n`.
 * 4. Set `found` to `n < PIC_IRQS`.
 * 5. If `found` is true:
 *    1. If `n` is equal to `PIC_CASCADE_IRQ`:
 *       1. Call `chip_request()` with the arguments `slave` and `0U`, and store
 *          the result in `m`.
 *       2. Call `acknowledge()` with the arguments `master` and `n`.
 *       3. Call `acknowledge()` with the arguments `slave` and `m`.
 *       4. Set `*vector` to `(uint8_t) (slave->vector_base + m)`.
 *    2. Otherwise:
 *       1. Call `acknowledge()` with the arguments `master` and `n`.
 *       2. Set `*vector` to `(uint8_t) (master->vector_base + n)`.
 * 6. Return `found`.
 *
 * @calls
 * - `chip_request()`
 * - `cascade_bit()`
 * - `acknowledge()`
 */
bool vpic_next(vpic_t *pic, uint8_t *vector)
{
    vpic_chip_t *master = &pic->chip[0];
    vpic_chip_t *slave = &pic->chip[1];
    uint32_t n = chip_request(master, cascade_bit(pic));
    bool found = n < PIC_IRQS;

    if (found) {
        if (n == PIC_CASCADE_IRQ) {
            // IRQ 2 is the slave: its request gives the vector.
            uint32_t m = chip_request(slave, 0U);

            acknowledge(master, n);
            acknowledge(slave, m);
            *vector = (uint8_t) (slave->vector_base + m);
        } else {
            acknowledge(master, n);
            *vector = (uint8_t) (master->vector_base + n);
        }
    }
    return found;
}
