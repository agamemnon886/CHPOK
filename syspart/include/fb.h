/* Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types (int,
 * unsigned, short, long, ...) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier; vga_init() and rgba_to_argb() are
 * declared here.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the types are typedefs without a
 * tag, used by their typedef names, and are documented with their macros.
 */
#ifndef QEMU_VGA_FB_H
#define QEMU_VGA_FB_H
#include <stdint.h>

/**
 * @typename `uwrm_handle_t`
 *
 * @description
 * The descriptor of a framebuffer.
 */
typedef int32_t uwrm_handle_t;

/**
 * @typename `fb_format_e`
 *
 * @description
 * The pixel format of a framebuffer.
 */
typedef enum {
    UWRM_FORMAT_RGB888, /**< 24-bit RGB. */
    UWRM_FORMAT_RGBA8888, /**< 32-bit RGBA. */
    UWRM_FORMAT_ARGB8888, /**< 32-bit ARGB. */
    UWRM_FORMAT_RGB565, /**< 16-bit RGB. */
} fb_format_e;

/**
 * @typename `uwrm_ret_e`
 *
 * @description
 * The result of a framebuffer function.
 */
typedef enum {
    UWRM_OK, /**< Success. */
    UWRM_ERROR /**< Failure. */
} uwrm_ret_e;

/**
 * @typename `uwrm_scm_direct_fb_t`
 *
 * @description
 * A framebuffer that a partition draws into directly: its two surfaces, its
 * size and its format.
 */
typedef struct {
    uwrm_handle_t hfb; /**< Fb descriptor. */
    void* back_surface; /**< The surface drawn into. */
    void* front_surface; /**< The surface displayed. */
    int32_t width; /**< The width, in pixels. */
    int32_t height; /**< The height, in pixels. */
    int32_t format; /**< fb_format_e. */
} uwrm_scm_direct_fb_t;

/** \cond */
int32_t uwrm_scm_get_direct_fb(uwrm_scm_direct_fb_t *fb);

int32_t uwrm_scm_fb_swap(uwrm_scm_direct_fb_t *fb);

/* Initializes the QEMU VGA device (drivers/qemu_vga). */
void vga_init(void);
/* The ARGB value of an RGBA color. */
uint32_t rgba_to_argb(uint32_t rgba_color);
/** \endcond */

#endif
