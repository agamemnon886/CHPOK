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
 * Created by julien on Fri Jan 30 14:41:34 2009 
 *
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 * Modified by Kirill Brazhnikov, 2026-11-09: Doxygen headers for the file
 * and its functions.
 * Modified by Kirill Brazhnikov, 2026-11-14: The function headers no longer
 * have a step for the declaration of a local variable; the @locals table
 * describes it, and an initialized one is set in a step.
 * The modifications are distributed under the POK licence
 * (BSD-2-Clause, see POK-LICENSE).
 */

/*-
 * Copyright (c) 2003, Steven G. Kargl
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * roundf() of libpok's math library (fdlibm, from Sun Microsystems).
 */
#include <config.h>
#ifdef POK_NEEDS_LIBMATH

#include <libm.h>

/**
 * @funcname `roundf`
 *
 * @description
 * Computes x rounded to the nearest integer, halfway cases away from 0, in
 * float (C standard).
 *
 * @params
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `x` | `float` | [in] The argument. |
 *
 * @return The result.
 *
 * @globals
 * None.
 *
 * @locals
 * | Name | Type | Description |
 * |------|------|-------------|
 * | `i` | `int` | A temporary of the computation |
 * | `t` | `float` | A temporary of the computation |
 *
 * @hlr
 *
 * @llr
 *
 * @design
 * 1. Set `i` to `fpclassify(x)`.
 * 2. If `i` is equal to `FP_INFINITE` or `i` is equal to `FP_NAN`:
 *    1. Return `(x)`.
 * 3. If `x` is greater than or equal to `0.0`:
 *    1. Call `floorf()` with the argument `x`, and store the result in `t`.
 *    2. If `x - t` is greater than or equal to `0.5`:
 *       1. Add `1.0` to `t`.
 *    3. Return `(t)`.
 * 4. Otherwise:
 *    1. Set `x` to `-x`.
 *    2. Call `floorf()` with the argument `x`, and store the result in `t`.
 *    3. If `x - t` is greater than or equal to `0.5`:
 *       1. Add `1.0` to `t`.
 *    4. Return `-t`.
 *
 * @calls
 * - `__fpclassifyf()`
 * - `__fpclassifyd()`
 * - `floorf()`
 */
float
roundf(float x)
{
	float t;
	int i;

	i = fpclassify(x);
	if (i == FP_INFINITE || i == FP_NAN)
		return (x);

	if (x >= 0.0) {
		t = floorf(x);
		if (x - t >= 0.5)
			t += 1.0;
		return (t);
	} else {
		x = -x;
		t = floorf(x);
		if (x - t >= 0.5)
			t += 1.0;
		return (-t);
	}
}

#endif

