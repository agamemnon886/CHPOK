/*
 * Modified by Kirill Brazhnikov, 2026-11-14: the Doxygen comment of the file is
 * removed (header files are not documented); the Doxygen headers of the static
 * inline functions are plain comments (they are not documented).
 */

#ifndef POK_VIRTIO_RING_H
#define POK_VIRTIO_RING_H
/* An interface for efficient virtio implementation, currently for use by KVM
 * and lguest, but hopefully others soon.  Do NOT change this since it will
 * break existing servers and clients.
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright Rusty Russell IBM Corporation 2007.
 * Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
 *
 * Modified by Kirill Brazhnikov, 2026-10-31: built-in integer types
 * (unsigned, unsigned long) are replaced with the fixed-width types
 * from <stdint.h>.
 *
 * Modified by Kirill Brazhnikov, 2026-11-04: MISRA C:2025: the header guard
 * isn't a reserved identifier; U suffixes for unsigned constants; operands
 * of mixed precedence are parenthesized; deviations are documented where a
 * guideline can't be followed; vring_init() doesn't do arithmetic on a void
 * pointer.
 *
 * Modified by Kirill Brazhnikov, 2026-11-08: Doxygen headers for the file
 * and its functions.
 */

#include <types.h>

/**
 * @macroname `VRING_DESC_F_NEXT`
 *
 * @description
 * This marks a buffer as continuing via the next field.
 */
#define VRING_DESC_F_NEXT	1U

/**
 * @macroname `VRING_DESC_F_WRITE`
 *
 * @description
 * This marks a buffer as write-only (otherwise read-only).
 */
#define VRING_DESC_F_WRITE	2

/**
 * @macroname `VRING_DESC_F_INDIRECT`
 *
 * @description
 * This means the buffer contains a list of buffer descriptors.
 */
#define VRING_DESC_F_INDIRECT	4

/**
 * @macroname `VRING_USED_F_NO_NOTIFY`
 *
 * @description
 * The Host uses this in used->flags to advise the Guest: don't kick me when you
 * add a buffer. It's unreliable, so it's simply an optimization. Guest will
 * still kick if it's out of buffers.
 */
#define VRING_USED_F_NO_NOTIFY	1

/**
 * @macroname `VRING_AVAIL_F_NO_INTERRUPT`
 *
 * @description
 * The Guest uses this in avail->flags to advise the Host: don't interrupt me
 * when you consume a buffer. It's unreliable, so it's simply an optimization.
 */
#define VRING_AVAIL_F_NO_INTERRUPT	1

/**
 * @macroname `VIRTIO_RING_F_INDIRECT_DESC`
 *
 * @description
 * We support indirect buffer descriptors.
 */
#define VIRTIO_RING_F_INDIRECT_DESC	28


/**
 * @macroname `VIRTIO_RING_F_EVENT_IDX`
 *
 * @description
 * The event index feature: the guest publishes the used index for which it
 * expects an interrupt at the end of the available ring, and the host publishes
 * the available index for which it expects a kick at the end of the used ring;
 * the flags fields are then ignored.
 */
#define VIRTIO_RING_F_EVENT_IDX		29

/**
 * @typename `vring_desc_t`
 *
 * @description
 * Virtio ring descriptors: 16 bytes. These can chain together via "next".
 */
typedef struct {
	/**
	 * Address (guest-physical).
	 */
	uint64_t addr;
	/**
	 * Length.
	 */
	uint32_t len;
	/**
	 * The flags as indicated above.
	 */
	uint16_t flags;
	/**
	 * We chain unused descriptors via this, too.
	 */
	uint16_t next;
} vring_desc_t;

/**
 * @typename `vring_avail_t`
 *
 * @description
 * The available ring: the buffers the driver offers to the device.
 */
typedef struct {
	uint16_t flags; /**< The flags (VRING_AVAIL_F_NO_INTERRUPT). */
	uint16_t idx; /**< Where the driver puts the next entry. */
	/* MISRA C:2025 Rule 18.7 deviation: the ring has the queue size,
	 * which the device sets. */
	uint16_t ring[]; /**< The heads of the available descriptor chains. */
} vring_avail_t;

/**
 * @typename `vring_used_elem_t`
 *
 * @description
 * An entry of the used ring: a used descriptor chain (32-bit fields for padding
 * reasons).
 */
typedef struct {
	/**
	 * Index of start of used descriptor chain.
	 */
	uint32_t id;
	/**
	 * Total length of the descriptor chain which was used (written to).
	 */
	uint32_t len;
} vring_used_elem_t;

/**
 * @typename `vring_used_t`
 *
 * @description
 * The used ring: the buffers the device has used.
 */
typedef struct {
	uint16_t flags; /**< The flags (VRING_USED_F_NO_NOTIFY). */
	uint16_t idx; /**< Where the device puts the next entry. */
	/* MISRA C:2025 Rule 18.7 deviation: the ring has the queue size,
	 * which the device sets. */
	vring_used_elem_t ring[]; /**< The used buffers. */
} vring_used_t;

/**
 * @typename `vring_t`
 *
 * @description
 * A virtio ring: its descriptor table, its available ring and its used ring.
 */
typedef struct {
	uint32_t num; /**< The number of descriptors. */

	vring_desc_t *desc; /**< The descriptor table. */

	vring_avail_t *avail; /**< The available ring. */

	vring_used_t *used; /**< The used ring. */
} vring_t;

/* The standard layout for the ring is a continuous chunk of memory which looks
 * like this.  We assume num is a power of 2.
 *
 * vring_t
 * {
 *	// The actual descriptors (16 bytes each)
 *	vring_desc_t desc[num];
 *
 *	// A ring of available descriptor heads with free-running index.
 *	uint16_t avail_flags;
 *	uint16_t avail_idx;
 *	uint16_t available[num];
 *	uint16_t used_event_idx;
 *
 *	// Padding to the next align boundary.
 *	char pad[];
 *
 *	// A ring of used descriptor heads with free-running index.
 *	uint16_t used_flags;
 *	uint16_t used_idx;
 *	vring_used_elem_t used[num];
 *	uint16_t avail_event_idx;
 * };
 */

/**
 * @macroname `vring_used_event`
 *
 * @description
 * We publish the used event index at the end of the available ring, and vice
 * versa. They are at the end for backwards compatibility.
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `vr` | The ring (vring_t *). |
 */
#define vring_used_event(vr) ((vr)->avail->ring[(vr)->num])

/**
 * @macroname `vring_avail_event`
 *
 * @description
 * The avail event index, published by the host at the end of the used ring (an
 * lvalue).
 *
 * @params
 * | Name | Description |
 * |------|-------------|
 * | `vr` | The ring (vring_t *). |
 */
#define vring_avail_event(vr) (*(uint16_t *)&(vr)->used->ring[(vr)->num])

/*
 * Lays out the rings of a virtqueue in its memory: the descriptors, the
 * available ring after them, and the used ring at the next multiple of the
 * alignment after the available ring (as the virtio specification lays them
 * out).
 */
static inline void vring_init(vring_t *vr, uint32_t num, void *p,
			      uintptr_t align)
{
	vr->num = num;
	vr->desc = p;
	vr->avail = (void *) ((char *) p + (num * sizeof(vring_desc_t)));
	/* MISRA C:2025 Rule 11.4 and Rule 11.6 deviation: the used ring
	 * starts at the next multiple of align after the available ring, as
	 * the virtio specification lays the rings out. */
	vr->used = (void *)(((uintptr_t)&vr->avail->ring[num] + sizeof(uint16_t)
		+ align-1U) & ~(align - 1U));
}

/*
 * Returns the size of the memory of a virtqueue's rings.
 */
static inline uint32_t vring_size(uint32_t num, uintptr_t align)
{
	return (((sizeof(vring_desc_t) * num)
                 + (sizeof(uint16_t) * (3U + num))
		 + align - 1U) & ~(align - 1U))
		+ (sizeof(uint16_t) * 3U)
		    + (sizeof(vring_used_elem_t) * num);
}

/*
 * Tells whether an event should be triggered: after the index was incremented
 * from old to new_idx, given the event index from the other side
 * (USED_EVENT_IDX and AVAIL_EVENT_IDX).
 */
static inline int32_t vring_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old)
{
	/* Note: Xen has similar logic for notification hold-off
	 * in include/xen/interface/io/ring.h with req_event and req_prod
	 * corresponding to event_idx + 1 and new_idx respectively.
	 * Note also that req_event and req_prod in Xen start at 1,
	 * event indexes in virtio start at 0. */
	return ((uint16_t)(new_idx - event_idx - 1U)
                < (uint16_t)(new_idx - old))
		? 1 : 0;
}

#endif /* POK_VIRTIO_RING_H */
