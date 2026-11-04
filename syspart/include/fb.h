/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; vga_init() and rgba_to_argb() are
 * declared here.
 */
#ifndef QEMU_VGA_FB_H
#define QEMU_VGA_FB_H
#include <stdint.h>

typedef int32_t uwrm_handle;

enum FB_FORMAT {
    UWRM_FORMAT_RGB888,
    UWRM_FORMAT_RGBA8888,
    UWRM_FORMAT_ARGB8888,
    UWRM_FORMAT_RGB565,
};

enum UWRM_RET {
    UWRM_OK,
    UWRM_ERROR
};

struct uwrm_scm_direct_fb {
    uwrm_handle hfb; // fb descriptor
    void* back_surface;
    void* front_surface;
    int32_t width;
    int32_t height;
    int32_t format; //enum FB_FORMAT
};

int32_t uwrm_scm_get_direct_fb(struct uwrm_scm_direct_fb *fb);

int32_t uwrm_scm_fb_swap(struct uwrm_scm_direct_fb *fb);

/* Initializes the QEMU VGA device (drivers/qemu_vga). */
void vga_init(void);
/* The ARGB value of an RGBA color. */
uint32_t rgba_to_argb(uint32_t rgba_color);

#endif
