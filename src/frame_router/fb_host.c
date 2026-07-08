/*
 * fb_host.c - hypervisor host-side framebuffer consumer.
 *
 * Attaches to the guest's shared framebuffer via <hyp_shm.h>, and posts
 * each received frame into a QNX Screen window so it displays on the
 * host's physical screen.
 *
 * Polls hdr->frame_seq on a timer rather than relying on pulses, since
 * the shmem vdev's notify mechanism appears to fire only on
 * connect/disconnect rather than per-frame.
 *
 * Build: qcc -Vgcc_ntoaarch64le -o fb_host fb_host.c -lhyp -lscreen
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/neutrino.h>
#include <hyp_shm.h>
#include <screen/screen.h>

#include "shm_proto.h"

#define FB_PULSE_CODE   _PULSE_CODE_MINAVAIL

static screen_context_t g_screen_ctx;
static screen_window_t  g_win;
static screen_buffer_t  g_win_buf;
static int      g_win_stride;
static uint8_t *g_win_ptr;

static int
screen_display_init(uint32_t width, uint32_t height)
{
    if (screen_create_context(&g_screen_ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        perror("screen_create_context");
        return -1;
    }
    if (screen_create_window(&g_win, g_screen_ctx) != 0) {
        perror("screen_create_window");
        return -1;
    }

    int format = SCREEN_FORMAT_RGBX8888; /* ignore alpha entirely - always opaque */
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_FORMAT, &format);

    int usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_USAGE, &usage);

    int size[2] = { (int)width, (int)height };
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_BUFFER_SIZE, size);
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_SIZE, size);

    int pos[2] = { 0, 0 };
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_POSITION, pos);

    int zorder = 1000; /* deliberately high, matches host_redscreen */
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_ZORDER, &zorder);

    int visible = 1;
    screen_set_window_property_iv(g_win, SCREEN_PROPERTY_VISIBLE, &visible);

    if (screen_create_window_buffers(g_win, 1) != 0) {
        perror("screen_create_window_buffers");
        return -1;
    }

    if (screen_get_window_property_pv(g_win, SCREEN_PROPERTY_RENDER_BUFFERS,
                                       (void **)&g_win_buf) != 0) {
        perror("screen_get_window_property_pv(RENDER_BUFFERS)");
        return -1;
    }
    screen_get_buffer_property_iv(g_win_buf, SCREEN_PROPERTY_STRIDE, &g_win_stride);
    screen_get_buffer_property_pv(g_win_buf, SCREEN_PROPERTY_POINTER, (void **)&g_win_ptr);

    fprintf(stderr, "window created: %dx%d, stride=%d, ptr=%p\n",
            width, height, g_win_stride, (void *)g_win_ptr);

    return 0;
}

static void
screen_display_frame(const uint8_t *src, uint32_t width, uint32_t height, uint32_t src_stride)
{
    for (uint32_t y = 0; y < height; y++) {
        memcpy(g_win_ptr + (size_t)y * (uint32_t)g_win_stride,
               src + (size_t)y * src_stride,
               (size_t)width * FB_BPP);
    }

    int dirty_rect[4] = { 0, 0, (int)width, (int)height };
    screen_post_window(g_win, g_win_buf, 1, dirty_rect, 0);
}

int
main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--test") == 0) {
        /* Self-test: exercises the exact same window-creation code path
         * as the real pipeline, but skips shmem/hyp_shm entirely. If
         * this doesn't show a solid color on the physical display, the
         * bug is in Screen/window setup, not in fb_guest or the shmem
         * plumbing - which the earlier heartbeat logs already showed
         * working correctly frame-for-frame. */
        uint32_t test_w = 1024, test_h = 600;
        if (argc > 3) {
            test_w = (uint32_t)atoi(argv[2]);
            test_h = (uint32_t)atoi(argv[3]);
        }

        fprintf(stderr, "--test: creating a %ux%u window and filling it "
                        "solid green for 15 seconds...\n", test_w, test_h);

        if (screen_display_init(test_w, test_h) != 0) {
            fprintf(stderr, "--test: screen display init failed\n");
            return 1;
        }

        /* Fill the whole window buffer directly - no memcpy from shmem,
         * no alpha ambiguity (RGBX8888 ignores the byte anyway), just a
         * flat color written straight into the render buffer. */
        for (uint32_t y = 0; y < test_h; y++) {
            uint8_t *row = g_win_ptr + (size_t)y * (uint32_t)g_win_stride;
            for (uint32_t x = 0; x < test_w; x++) {
                row[x * 4 + 0] = 0x00; /* B */
                row[x * 4 + 1] = 0xFF; /* G */
                row[x * 4 + 2] = 0x00; /* R */
                row[x * 4 + 3] = 0xFF; /* X/unused, but keep it 0xFF */
            }
        }

        int dirty[4] = { 0, 0, (int)test_w, (int)test_h };
        screen_post_window(g_win, g_win_buf, 1, dirty, 0);

        fprintf(stderr, "--test: posted. Check the physical display now.\n");
        sleep(15);
        return 0;
    }

    size_t region_size = fb_shm_total_size(FB_MAX_WIDTH, FB_MAX_HEIGHT, FB_BPP);
    unsigned attach_pages = (unsigned)(region_size / SHM_PAGE_SIZE);

    int chid = ChannelCreate(0);
    if (chid == -1) {
        perror("ChannelCreate");
        return 1;
    }

    struct hyp_shm *hsp = hyp_shm_create(0);
    if (hsp == NULL) {
        fprintf(stderr, "hyp_shm_create failed\n");
        return 1;
    }

    int rc = hyp_shm_attach_ext(hsp, FB_REGION_NAME, attach_pages,
                                 chid, -1, FB_PULSE_CODE, NULL,
                                 0600, (gid_t)-1);
    if (rc != 0) {
        fprintf(stderr, "hyp_shm_attach_ext failed: rc=%d\n", rc);
        return 1;
    }

    fprintf(stderr, "attached: name=%s idx=%u size=%u\n",
            hyp_shm_name(hsp), hyp_shm_idx(hsp), hyp_shm_size(hsp));

    volatile struct fb_shm_header *hdr =
        (volatile struct fb_shm_header *)hyp_shm_data(hsp);

    while (hdr->magic != FB_MAGIC) {
        usleep(10000);
    }
    fprintf(stderr, "frame: %ux%u stride=%u format=%u\n",
            hdr->width, hdr->height, hdr->stride, hdr->format);

    if (screen_display_init(hdr->width, hdr->height) != 0) {
        fprintf(stderr, "screen display init failed\n");
        hyp_shm_detach(hsp);
        return 1;
    }

    uint32_t last_seq = (uint32_t)-1;
    uint32_t frame_count = 0;
    for (;;) {
        if (hdr->frame_seq != last_seq) {
            last_seq = hdr->frame_seq;
            uint32_t idx = hdr->front_index;
            hdr->host_reading_index = idx;
            __sync_synchronize();
            uint8_t *frame = (uint8_t *)hdr + hdr->buffer_offset[idx];
            screen_display_frame(frame, hdr->width, hdr->height, hdr->stride);
            __sync_synchronize();
            hdr->host_reading_index = FB_INVALID_INDEX;

            frame_count++;
            if (frame_count % 60 == 0) {
                /* Sample a handful of pixels to check whether this is real
                 * varying image data or just blank/constant bytes. */
                uint32_t mid_row = hdr->height / 2;
                uint8_t *p0 = frame + 0;
                uint8_t *p1 = frame + (size_t)mid_row * hdr->stride;
                uint8_t *p2 = frame + (size_t)(hdr->height - 1) * hdr->stride
                                    + (size_t)(hdr->width - 1) * FB_BPP;
                fprintf(stderr,
                    "displayed %u frames (seq=%u, buf_idx=%u) "
                    "px[0,0]=%02x%02x%02x%02x px[mid]=%02x%02x%02x%02x "
                    "px[last]=%02x%02x%02x%02x\n",
                    frame_count, last_seq, idx,
                    p0[0], p0[1], p0[2], p0[3],
                    p1[0], p1[1], p1[2], p1[3],
                    p2[0], p2[1], p2[2], p2[3]);
            }
        }
        usleep(15000);
    }

    hyp_shm_detach(hsp);
    return 0;
}