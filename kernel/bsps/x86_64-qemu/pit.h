/*
 *                               POK header
 * 
 * The following file is a part of the POK project. Any modification should
 * made according to the POK licence. You CANNOT use this file or a part of
 * this file is this part of a file for your own project
 *
 * For more information on the POK licence, please see our LICENCE FILE
 *
 * Please follow the coding guidelines described in doc/CODING_GUIDELINES
 *
 *                                      Copyright (c) 2007-2009 POK team 
 *
 * Created by julien on Thu Jan 15 23:34:13 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-10-31: ported from
 * boards/x86-qemu/pit.h to x86_64 (64-bit).
 * Modified by Kirill Brazhnikov, 2026-11-02: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 * Modified by Kirill Brazhnikov, 2026-11-14: the macros of
 * boards/x86_64-qemu/pit.c are moved here.
 * Modified by Kirill Brazhnikov, 2026-11-15: moved from
 * boards/x86_64-qemu/pit.h (the board support packages are in kernel/bsps).
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

#ifndef POK_X86_QEMU_PIT_H
#define POK_X86_QEMU_PIT_H

/** \cond */
void pok_x86_qemu_timer_init (void);
/** \endcond */

/**
 * @macroname `OSCILLATOR_RATE`
 *
 * @description
 * The oscillation rate of x86 clock: the input frequency of the 8254 PIT,
 * in Hz.
 */
#define OSCILLATOR_RATE 1193180U

/**
 * @macroname `PIT_BASE`
 *
 * @description
 * The I/O port of the 8254 PIT's channel 0; channel 2 is at PIT_BASE + 2
 * and the mode/command register at PIT_BASE + 3.
 */
#define PIT_BASE 0x40

/**
 * @name Port B
 * Port 0x61: PIT channel 2 gate (bit 0), speaker (bit 1), channel 2 output
 * (bit 5).
 * @{
 */

/**
 * @macroname `PORT_B`
 *
 * @description
 * The I/O port of port B.
 */
#define PORT_B 0x61

/**
 * @macroname `PORT_B_GATE2`
 *
 * @description
 * The gate of PIT channel 2 (1 lets it count).
 */
#define PORT_B_GATE2 0x01

/**
 * @macroname `PORT_B_SPEAKER`
 *
 * @description
 * Connects PIT channel 2 to the speaker (cleared during the calibration).
 */
#define PORT_B_SPEAKER 0x02

/**
 * @macroname `PORT_B_OUT2`
 *
 * @description
 * The output of PIT channel 2 (set when its count has ended).
 */
#define PORT_B_OUT2 0x20U

/** @} */

/**
 * @macroname `CALIBRATION_PIT_TICKS`
 *
 * @description
 * Duration of TSC calibration (in PIT ticks, at most 0xffff): 50 ms.
 */
#define CALIBRATION_PIT_TICKS (OSCILLATOR_RATE / 20U)

#endif /* !__POK_X86_PIT_H_ */

