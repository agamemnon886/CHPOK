/*  
 *  Copyright (C) 2014 Maxim Malkov, ISPRAS <malkov@ispras.ru> 
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header
 * guard isn't a reserved identifier.
 *
 * Modified by Kirill Brazhnikov, 2026-11-13: the function declarations are
 * hidden from Doxygen (\cond): only the definitions of the functions are
 * documented.
 *
 * Modified by Kirill Brazhnikov, 2026-11-14: the types are typedefs without a
 * tag, used by their typedef names, and are documented with their macros.
 */
#ifndef POK_KERNEL_VIRTIO_VIRTQUEUE_H
#define POK_KERNEL_VIRTIO_VIRTQUEUE_H

#include "virtio_ring.h"

/**
 * @typename `virtio_virtqueue_t`
 *
 * @description
 * A virtqueue of a device: its ring and the state of its descriptors.
 */
typedef struct {
    vring_t vring; /**< The ring. */

    /**
     * Index of first free descriptor.
     */
    uint16_t free_index;

    /**
     * Count of free descriptors.
     */
    uint16_t num_free;

    /**
     * Last seen used.
     */
    uint16_t last_seen_used;
} virtio_virtqueue_t;

/** \cond */
void* virtio_virtqueue_setup(virtio_virtqueue_t *vq, uint16_t size,
    size_t alignment);
/** \endcond */

#endif // POK_KERNEL_VIRTIO_VIRTQUEUE_H
