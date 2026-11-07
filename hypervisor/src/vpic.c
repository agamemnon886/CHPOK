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
 * Virtual 8259A PIC pair (master at 0x20, slave at 0xA0, cascaded on
 * IRQ 2), as far as JetOS and similar guests use it: initialization
 * (ICW1-ICW4), the mask (OCW1), non-specific and specific EOI (OCW2),
 * reading IRR or ISR (OCW3), and automatic EOI. Interrupts are
 * edge-triggered with fixed priorities (IRQ 0 highest). Rotation, special
 * mask mode and polling are not emulated.
 */

#include <hv/hv.h>

#define PIC_MASTER          0x20U
#define PIC_SLAVE           0xA0U
#define PIC_CASCADE_IRQ     2U
#define PIC_IRQS            8U

#define ICW1_INIT           0x10U
#define ICW1_ICW4           0x01U
#define ICW1_SINGLE         0x02U
#define ICW4_AUTO_EOI       0x02U
#define OCW3_SELECT         0x08U
#define OCW3_MASK           0x18U
#define OCW3_READ_REGISTER  0x02U
#define OCW3_READ_ISR       0x01U
#define OCW2_EOI            0x20U
#define OCW2_SPECIFIC       0x40U
#define OCW2_EOI_MASK       0xE0U
#define OCW2_LEVEL          0x07U

// Expected initialization words.
#define STEP_NONE           0U
#define STEP_ICW2           1U
#define STEP_ICW3           2U
#define STEP_ICW4           3U

// Reset one chip: nothing requested or in service, all masked.
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

// Reset the virtual PIC pair.
void vpic_reset(vpic_t *pic)
{
    // The BIOS's vectors; guests program their own.
    chip_reset(&pic->chip[0], 0x08U);
    chip_reset(&pic->chip[1], 0x70U);
}

// The chip that a port belongs to.
static vpic_chip_t *chip_of(vpic_t *pic, uint16_t port)
{
    return ((port & 0xF0U) == PIC_SLAVE) ? &pic->chip[1] : &pic->chip[0];
}

// Lowest set bit (the highest priority), or PIC_IRQS if none.
static uint32_t first_bit(uint8_t bits)
{
    uint32_t n = 0U;

    while ((n < PIC_IRQS) && ((bits & (1U << n)) == 0U)) {
        n++;
    }
    return n;
}

/*
 * Write to a chip's command port: ICW1 (start of initialization), OCW3
 * (register to read) or OCW2 (end of interrupt).
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

/*
 * Write to a chip's data port: the next initialization word (ICW2-ICW4),
 * otherwise the mask (OCW1).
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

// Write of the guest to a PIC port.
void vpic_write(vpic_t *pic, uint16_t port, uint8_t value)
{
    vpic_chip_t *chip = chip_of(pic, port);

    if ((port & 1U) == 0U) {
        pic_command(chip, value);
    } else {
        pic_data(chip, value);
    }
}

/*
 * Read of the guest from a PIC port: the mask, or IRR or ISR as selected by
 * OCW3.
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

// Request interrupt irq (0-15), edge-triggered.
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

/*
 * Highest-priority request of a chip that is unmasked and not blocked by
 * an interrupt in service at the same or a higher priority.
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

// IRQ 2 bit of the master: set while the slave has a request to deliver.
static uint8_t cascade_bit(const vpic_t *pic)
{
    uint8_t bit = 0U;

    if (chip_request(&pic->chip[1], 0U) < PIC_IRQS) {
        bit = (uint8_t) (1U << PIC_CASCADE_IRQ);
    }
    return bit;
}

// Check whether the PIC has an interrupt to deliver.
bool vpic_pending(const vpic_t *pic)
{
    return chip_request(&pic->chip[0], cascade_bit(pic)) < PIC_IRQS;
}

// Acknowledge the request n of a chip: it moves from IRR to ISR.
static void acknowledge(vpic_chip_t *chip, uint32_t n)
{
    uint8_t bit = (uint8_t) (1U << n);

    chip->irr &= (uint8_t) ~bit;
    if (!chip->auto_eoi) {
        chip->isr |= bit;
    }
}

/*
 * Take the interrupt to deliver next: acknowledge it and give its vector.
 * Returns false if there is none.
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
