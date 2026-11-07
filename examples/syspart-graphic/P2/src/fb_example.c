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
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-07: the image size is converted
 * to the type of the loop counters explicitly.
 */

#include <stdio.h>
#include <string.h>
#include <arinc653/time.h>

#include <fb.h>
#define MILLISECOND 1000000

uint32_t rgba_to_argb(uint32_t rgba_color);

struct gimp_image {
  uint32_t      width;
  uint32_t      height;
  uint32_t      bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
  uint8_t pixel_data[];
};

extern const struct gimp_image gimp_image; //Image of airplane


static void draw_image(struct uwrm_scm_direct_fb *fb, int32_t x_start, int32_t y_start)
{
    uint32_t *addr;
    for (int32_t y = 0; y < (int32_t) gimp_image.height; y++) {
        for (int32_t x = 0; x < (int32_t) gimp_image.width; x++) {
            addr = (uint32_t *) fb->back_surface + (y+y_start)*fb->width + x + x_start;
            uint32_t rgba_pixel = (((uint32_t*)gimp_image.pixel_data)[y*gimp_image.width + x]);
            *addr = rgba_to_argb(rgba_pixel);
        }
    }
}

void fb_example_run(void)
{
    RETURN_CODE_TYPE ret;

    struct uwrm_scm_direct_fb fb;
    uwrm_scm_get_direct_fb(&fb);

    int32_t y = fb.height - 100;
    int32_t x = 0;

    while (y > 0) {
        draw_image(&fb, x, y);
        uwrm_scm_fb_swap(&fb);
        y--;
        x++;
        TIMED_WAIT(MILLISECOND, &ret);
    }
}
