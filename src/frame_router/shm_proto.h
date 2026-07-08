#ifndef SHM_PROTO_H
#define SHM_PROTO_H

#include <stdint.h>

/* ---------------------------------------------------------------------
 * Section A: shmem vdev register layout.
 * Field names/types match QNX's public <guest_shm.h> so the guest driver
 * stays binary-compatible with the vdev. Reproduced here (BSD-style
 * licensed header from QNX Software Systems) so this file is self
 * contained; if your SDK ships guest_shm.h, prefer including that
 * instead of this section.
 * --------------------------------------------------------------------- */

#define GUEST_SHM_MAX_CLIENTS 16
#define GUEST_SHM_MAX_NAME    32
#define GUEST_SHM_SIGNATURE   0x4d534732474d5651ULL

enum guest_shm_status {
    GSS_OK,
    GSS_UNKNOWN_FAILURE,
    GSS_NOMEM,
    GSS_CLIENT_MAX,
    GSS_ILLEGAL_NAME,
    GSS_NO_PERMISSION,
    GSS_DOES_NOT_EXIST,
};

/* Factory page register layout - mapped at the vdev's `loc` address */
struct guest_shm_factory {
    uint64_t signature;               /* == GUEST_SHM_SIGNATURE (R/O) */
    uint64_t shmem;                   /* guest-phys addr of control page (R/O) */
    uint32_t vector;                  /* interrupt vector to attach to (R/O) */
    uint32_t status;                  /* status of last creation (R/O) */
    uint32_t size;                    /* WRITE requested size in 4K pages to create/attach */
    char     name[GUEST_SHM_MAX_NAME];/* WRITE name of region before writing size */
    uint32_t find;                    /* alternative: find existing region by number */
};

/* Control page register layout - lives at offset 0 of the mapped region */
struct guest_shm_control {
    uint32_t status; /* low 16 bits: pending-notify bitset, high 16: active clients (R/O) */
    uint32_t idx;    /* our connection index (R/O) */
    uint32_t notify; /* WRITE a bitset of client indices to notify */
    uint32_t detach; /* WRITE (any value) to detach from the region */
};

#define SHM_PAGE_SIZE 4096u

/* ---------------------------------------------------------------------
 * Section B: framebuffer application protocol.
 * This lives in the data area *after* the control page, i.e. at
 * (control_page_base + SHM_PAGE_SIZE).
 * --------------------------------------------------------------------- */

#define FB_REGION_NAME   "fb_shared"
#define FB_MAGIC         0x46425348u   /* 'FBSH' */
#define FB_MAX_BUFFERS   3             /* triple buffer: lets the guest always
                                        * find a slot that's neither the
                                        * current front nor the one the host
                                        * is actively reading */
#define FB_INVALID_INDEX 0xFFFFFFFFu

/* -------------------------------------------------------------------
 * These are NOT the real display resolution - they're just a generous
 * upper bound used ONLY to size the shared memory region itself, so
 * guest and host always agree on how many pages to create/attach
 * without any runtime negotiation. The *actual* per-frame width/height/
 * stride are detected at runtime by the guest (via Screen) and carried
 * dynamically in fb_shm_header below, which the host reads at attach
 * time. Raise these if you ever run at a resolution above 1920x1080;
 * otherwise you never need to touch them or recompile for a new panel.
 * ------------------------------------------------------------------- */
#define FB_MAX_WIDTH   1920u
#define FB_MAX_HEIGHT  1080u
#define FB_BPP         4u

enum fb_pixel_format {
    FB_FMT_RGBA8888 = 1,
    FB_FMT_BGRA8888 = 2,
    FB_FMT_RGB565   = 3,
};

/* Fixed-size header at the start of the data area (after control page) */
struct fb_shm_header {
    uint32_t magic;        /* FB_MAGIC, sanity check */
    uint32_t width;
    uint32_t height;
    uint32_t stride;       /* bytes per row */
    uint32_t format;       /* enum fb_pixel_format */
    uint32_t frame_seq;    /* incremented every time a new frame is published */
    uint32_t front_index;  /* which of FB_MAX_BUFFERS is currently valid/complete */
    uint32_t host_reading_index; /* FB_INVALID_INDEX, or the buffer the host is
                                  * currently mid-copy on - guest must not pick
                                  * this one as its next write target */
    uint32_t buffer_size;  /* bytes per single framebuffer (stride * height) */
    uint32_t buffer_offset[FB_MAX_BUFFERS]; /* byte offset from start of header to each buffer */
};

static inline size_t
fb_shm_total_size(uint32_t width, uint32_t height, uint32_t bytes_per_pixel)
{
    uint32_t stride = width * bytes_per_pixel;
    uint32_t buf_sz = stride * height;
    size_t   data_sz = sizeof(struct fb_shm_header) + (size_t)buf_sz * FB_MAX_BUFFERS;
    /* + control page, rounded up to page size */
    size_t   total = SHM_PAGE_SIZE + data_sz;
    return (total + (SHM_PAGE_SIZE - 1)) & ~((size_t)SHM_PAGE_SIZE - 1);
}

#endif /* SHM_PROTO_H */