/*
 * cpu_anim.c - minimal CPU-only animated QNX Screen client.
 *
 * Draws a bouncing colored box directly into a Screen window buffer and
 * posts it every frame - no EGL, no GLES, no GPU/devg driver required.
 * Used to put *something* real on-screen in a guest that has no GPU
 * passthrough, so fb_guest's screen_read_display() has actual changing
 * content to capture (instead of an empty compositor -> constant black).
 *
 * Build:
 *   qcc -Vgcc_ntoaarch64le -o cpu_anim cpu_anim.c -lscreen
 *
 * Run BEFORE starting fb_guest (real mode, no --test):
 *   ./cpu_anim &
 *   /proc/boot/fb_guest &
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <screen/screen.h>

#define WIN_W 1024
#define WIN_H 600
#define BOX_W 120
#define BOX_H 120

static volatile int g_running = 1;

static void on_signal(int sig) { (void)sig; g_running = 0; }

int
main(int argc, char **argv)
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    int width = WIN_W, height = WIN_H;
    if (argc > 2) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);
    }

    screen_context_t ctx;
    if (screen_create_context(&ctx, SCREEN_APPLICATION_CONTEXT) != 0) {
        perror("screen_create_context");
        return 1;
    }

    screen_window_t win;
    if (screen_create_window(&win, ctx) != 0) {
        perror("screen_create_window");
        return 1;
    }

    int format = SCREEN_FORMAT_RGBA8888;
    screen_set_window_property_iv(win, SCREEN_PROPERTY_FORMAT, &format);

    int usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
    screen_set_window_property_iv(win, SCREEN_PROPERTY_USAGE, &usage);

    int size[2] = { width, height };
    screen_set_window_property_iv(win, SCREEN_PROPERTY_BUFFER_SIZE, size);
    screen_set_window_property_iv(win, SCREEN_PROPERTY_SIZE, size);

    int pos[2] = { 0, 0 };
    screen_set_window_property_iv(win, SCREEN_PROPERTY_POSITION, pos);

    int zorder = 0;
    screen_set_window_property_iv(win, SCREEN_PROPERTY_ZORDER, &zorder);

    int visible = 1;
    screen_set_window_property_iv(win, SCREEN_PROPERTY_VISIBLE, &visible);

    if (screen_create_window_buffers(win, 1) != 0) {
        perror("screen_create_window_buffers");
        return 1;
    }

    screen_buffer_t buf;
    if (screen_get_window_property_pv(win, SCREEN_PROPERTY_RENDER_BUFFERS,
                                       (void **)&buf) != 0) {
        perror("screen_get_window_property_pv(RENDER_BUFFERS)");
        return 1;
    }

    int stride;
    uint8_t *ptr;
    screen_get_buffer_property_iv(buf, SCREEN_PROPERTY_STRIDE, &stride);
    screen_get_buffer_property_pv(buf, SCREEN_PROPERTY_POINTER, (void **)&ptr);

    fprintf(stderr, "cpu_anim: window %dx%d stride=%d ptr=%p - animating, no GPU used\n",
            width, height, stride, (void *)ptr);

    int bx = 0, by = 0;
    int dx = 4, dy = 3;
    uint32_t frame = 0;

    while (g_running) {
        /* clear to a slowly-cycling background color so even the
         * "empty" area is visibly non-static in captures */
        uint8_t bg = (uint8_t)((frame / 2) & 0xFF);
        for (int y = 0; y < height; y++) {
            uint8_t *row = ptr + (size_t)y * stride;
            for (int x = 0; x < width; x++) {
                row[x * 4 + 0] = bg;         /* R */
                row[x * 4 + 1] = 0x20;       /* G */
                row[x * 4 + 2] = (uint8_t)(0xFF - bg); /* B */
                row[x * 4 + 3] = 0xFF;       /* A */
            }
        }

        /* bounce a solid box on top */
        bx += dx;
        by += dy;
        if (bx <= 0 || bx + BOX_W >= width)  dx = -dx;
        if (by <= 0 || by + BOX_H >= height) dy = -dy;

        for (int y = by; y < by + BOX_H && y < height; y++) {
            if (y < 0) continue;
            uint8_t *row = ptr + (size_t)y * stride;
            for (int x = bx; x < bx + BOX_W && x < width; x++) {
                if (x < 0) continue;
                row[x * 4 + 0] = 0xFF;
                row[x * 4 + 1] = 0xFF;
                row[x * 4 + 2] = 0x00;
                row[x * 4 + 3] = 0xFF;
            }
        }

        int dirty[4] = { 0, 0, width, height };
        screen_post_window(win, buf, 1, dirty, 0);

        if (++frame % 60 == 0) {
            fprintf(stderr, "cpu_anim: posted %u frames (box at %d,%d)\n", frame, bx, by);
        }

        usleep(33000);
    }

    return 0;
}
