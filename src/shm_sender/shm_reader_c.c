/*
 * shm_reader_c.c
 * ----------------------------------------------------------------------------
 * Implementation of shm_reader_c.h. Compile as C (gnu11) -- this is the only
 * translation unit that touches motor_shm.h / C11 atomics.
 *
 *   gcc -std=gnu11 -O2 -c shm_reader_c.c -o shm_reader_c.o
 * ----------------------------------------------------------------------------
 */
#include "shm_reader_c.h"
#include "motor_wire.h"
#include "motor_shm.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

/* Guard against motor_row_copy_t / motor_block_copy_t drifting from the real
 * contract structs (see comment in shm_reader_c.h). */
_Static_assert(sizeof(motor_row_copy_t) == sizeof(motor_row_t),
               "motor_row_copy_t no longer mirrors motor_row_t -- update shm_reader_c.h");
_Static_assert(SHM_READER_MAX_ROWS_PER_BLOCK == MOTOR_MAX_ROWS_PER_BLOCK,
               "SHM_READER_MAX_ROWS_PER_BLOCK no longer matches MOTOR_MAX_ROWS_PER_BLOCK");

struct shm_reader {
    const shm_region_t *region;  /* MAP_FAILED sentinel not stored past open() */
    uint64_t            read_pos;
};

shm_reader_t *shm_reader_open(void)
{
    shm_reader_t *r = (shm_reader_t *)calloc(1, sizeof(*r));
    if (!r) return NULL;

    r->region = (const shm_region_t *)MAP_FAILED;
    for (int tries = 0; tries < 100 && r->region == MAP_FAILED; ++tries) {
        int fd = shm_open(MOTOR_SHM_NAME, O_RDONLY, 0);
        if (fd != -1) {
            r->region = (const shm_region_t *)mmap(NULL, sizeof(shm_region_t),
                                                     PROT_READ, MAP_SHARED, fd, 0);
            close(fd);
        }
        if (r->region == MAP_FAILED) {
            struct timespec d = {0, 100 * 1000 * 1000L}; /* 100ms */
            nanosleep(&d, NULL);
        }
    }
    if (r->region == MAP_FAILED) {
        free(r);
        return NULL;
    }

    for (int tries = 0; tries < 100 && !motor_shm_region_valid(r->region); ++tries) {
        struct timespec d = {0, 50 * 1000 * 1000L}; /* 50ms */
        nanosleep(&d, NULL);
    }
    if (!motor_shm_region_valid(r->region)) {
        munmap((void *)r->region, sizeof(shm_region_t));
        free(r);
        return NULL;
    }

    /* Start consuming from "now", like motor_monitor.c -- not from the start
     * of the ring's history. */
    r->read_pos = motor_ring_write_pos(&r->region->ring);
    return r;
}

void shm_reader_close(shm_reader_t *r)
{
    if (!r) return;
    if (r->region != (const shm_region_t *)MAP_FAILED)
        munmap((void *)r->region, sizeof(shm_region_t));
    free(r);
}

size_t shm_reader_poll_blocks(shm_reader_t *r,
                               motor_block_copy_t *out_blocks,
                               size_t max_blocks,
                               uint64_t *out_dropped_blocks)
{
    if (!r || !out_blocks || max_blocks == 0) return 0;

    const shm_block_ring_t *ring = &r->region->ring;
    uint64_t wp    = motor_ring_write_pos(ring);
    uint32_t depth = ring->depth;

    if (wp - r->read_pos > depth) {
        uint64_t lapped = (wp - r->read_pos) - depth;
        if (out_dropped_blocks) *out_dropped_blocks += lapped;
        r->read_pos = wp - depth;
    }

    /* shm_block_t is ~4.2KB (300 rows * 14 bytes + header) -- static to avoid
     * repeatedly growing the stack frame; fine since one reader == one thread. */
    static shm_block_t scratch;
    size_t written = 0;

    while (r->read_pos < wp && written < max_blocks) {
        if (motor_ring_read_slot(ring, r->read_pos, &scratch)) {
            motor_block_copy_t *out = &out_blocks[written];
            out->producer_seq = scratch.producer_seq;
            out->timestamp    = scratch.timestamp;
            out->n_rows       = scratch.n_rows;
            out->flags        = scratch.flags;
            memcpy(out->rows, scratch.rows,
                   (size_t)scratch.n_rows * sizeof(motor_row_t));
            written++;
        } else if (out_dropped_blocks) {
            (*out_dropped_blocks)++;  /* torn read that never resolved */
        }
        r->read_pos++;
    }
    return written;
}
