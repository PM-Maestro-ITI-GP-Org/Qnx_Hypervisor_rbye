/*
 * shm_reader_c.h
 * ----------------------------------------------------------------------------
 * C++-safe boundary for reading the motor shm ring.
 *
 * WHY THIS EXISTS: motor_shm.h uses C11 _Atomic / <stdatomic.h>, which is not
 * standard C++ (only very recent compilers support <stdatomic.h> in C++ mode,
 * and motor_wire.h separately uses _Static_assert, which C++ doesn't have
 * either -- it's static_assert there). Rather than touch either contract
 * header, the actual shm attach + ring read lives in shm_reader_c.c, compiled
 * as plain C11. This header is the only thing the C++ side includes: plain
 * structs, extern "C" linkage, safe from both languages.
 *
 * motor_row_copy_t / motor_block_copy_t are byte-for-byte mirrors of
 * motor_row_t / (the data fields of) shm_block_t. If either contract struct
 * changes, update these too -- shm_reader_c.c has compile-time size checks
 * that will fail loudly if they drift.
 * ----------------------------------------------------------------------------
 */
#ifndef SHM_READER_C_H
#define SHM_READER_C_H

#include <stdint.h>
#include <stddef.h>

/* Must match MOTOR_MAX_ROWS_PER_BLOCK in motor_wire.h. */
#define SHM_READER_MAX_ROWS_PER_BLOCK 300u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t current[3];
    int16_t  vib_x;
    int16_t  vib_y;
    int16_t  vib_z;
    uint16_t rpm;
} motor_row_copy_t;

typedef struct {
    uint32_t producer_seq;                          /* block's frame seq            */
    uint64_t timestamp;                              /* block timestamp (STM32 us)   */
    uint16_t n_rows;                                  /* valid rows in this block     */
    uint16_t flags;                                   /* frame flags at publish time  */
    motor_row_copy_t rows[SHM_READER_MAX_ROWS_PER_BLOCK];
} motor_block_copy_t;

typedef struct shm_reader shm_reader_t; /* opaque */

/* Attaches read-only to the shm region (waits, bounded, for a producer to
 * exist and publish a valid contract version). Returns NULL on failure. */
shm_reader_t *shm_reader_open(void);
void shm_reader_close(shm_reader_t *r);

/* Copies up to max_blocks newly-available ring blocks (since the previous
 * call) into out_blocks. Returns the number of blocks written.
 * *out_dropped_blocks (if non-NULL) is incremented by the number of blocks
 * lost this call -- either the producer lapped us (we were too slow and it
 * overwrote slots we hadn't read yet) or a slot was overwritten mid-copy
 * (torn read that didn't resolve after retries). Not thread-safe: use one
 * shm_reader_t from one thread. */
size_t shm_reader_poll_blocks(shm_reader_t *r,
                               motor_block_copy_t *out_blocks,
                               size_t max_blocks,
                               uint64_t *out_dropped_blocks);

#ifdef __cplusplus
}
#endif

#endif /* SHM_READER_C_H */
