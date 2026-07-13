/* test_producer.c -- creates the shm region and publishes a handful of
 * blocks with varying n_rows, then exits (region stays alive since shm
 * persists in /dev/shm until unlinked). For verifying the reader/chunker only.
 *
 *   gcc -std=gnu11 -O2 test_producer.c -o test_producer
 */
#include "motor_wire.h"
#include "motor_shm.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
    shm_unlink(MOTOR_SHM_NAME); /* clean slate */
    int fd = shm_open(MOTOR_SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, sizeof(shm_region_t));
    shm_region_t *r = mmap(NULL, sizeof(shm_region_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    motor_shm_region_init(r);

    /* publish 5 blocks with different row counts: 30, 30, 10, 40, 5 = 115 rows total */
    uint16_t counts[5] = {30, 30, 10, 40, 5};
    static motor_row_t rows[MOTOR_MAX_ROWS_PER_BLOCK];

    for (int b = 0; b < 5; ++b) {
        for (uint16_t i = 0; i < counts[b]; ++i) {
            rows[i].current[0] = (uint16_t)(1000 + b * 100 + i);
            rows[i].current[1] = (uint16_t)(2000 + b * 100 + i);
            rows[i].current[2] = (uint16_t)(3000 + b * 100 + i);
            rows[i].vib_x = (int16_t)i;
            rows[i].vib_y = (int16_t)(i + 1);
            rows[i].vib_z = (int16_t)(i + 2);
            rows[i].rpm = (uint16_t)(1500 + b);
        }
        frame_header_t hdr;
        memset(&hdr, 0, sizeof hdr);
        hdr.magic     = MOTOR_FRAME_MAGIC;
        hdr.seq       = (uint32_t)(100 + b);
        hdr.timestamp = (uint64_t)(1000000 + b * 10000);
        hdr.version   = MOTOR_CONTRACT_VERSION;
        hdr.n_rows    = counts[b];
        motor_ring_publish(&r->ring, &hdr, rows);
        printf("published block seq=%u n_rows=%u\n", hdr.seq, hdr.n_rows);
        fflush(stdout);
        struct timespec d = {0, 100 * 1000 * 1000L}; /* 100ms between blocks */
        nanosleep(&d, NULL);
    }

    munmap(r, sizeof(shm_region_t));
    printf("test_producer: done, shm left in place for the reader to attach to\n");
    return 0;
}
