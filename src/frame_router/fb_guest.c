/*
 * fb_guest.c - QNX guest-side framebuffer producer.
 *
 * Captures the guest's actual composited Screen display via
 * screen_read_display() and publishes it into the shmem region for the
 * host to pick up and display.
 *
 * Detects the real display resolution at runtime - never needs
 * recompiling for a different panel/mode. The shmem region itself is
 * always sized to a fixed FB_MAX_WIDTH/FB_MAX_HEIGHT ceiling (see
 * shm_proto.h) so guest and host always agree on page count without any
 * negotiation; the actual per-frame width/height/stride are carried
 * dynamically in the header.
 *
 * Build: qcc -Vgcc_ntoaarch64le -o fb_guest fb_guest.c -lscreen
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <screen/screen.h>

#include "shm_proto.h"

/* ---- Must match your .qvmconf `loc` for the shmem vdev ---- */
#define FACTORY_PADDR   0x1c050000ULL
/* ------------------------------------------------------------ */

static volatile struct guest_shm_control *g_ctrl;
static void   *g_region;
static size_t  g_region_size;
static int     g_running = 1;

static screen_context_t g_screen_ctx;
static screen_display_t g_display;
static screen_pixmap_t  g_capture_pixmap;
static screen_buffer_t  g_capture_buffer;
static int      g_capture_stride;
static uint8_t *g_capture_ptr;

static void
on_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static int
screen_query_display_size(uint32_t *out_width, uint32_t *out_height)
{
    if (screen_create_context(&g_screen_ctx, SCREEN_DISPLAY_MANAGER_CONTEXT) != 0) {
        perror("screen_create_context(SCREEN_DISPLAY_MANAGER_CONTEXT)");
        fprintf(stderr,
            "this requires an effective UID of root to succeed - run "
            "fb_guest as root\n");
        return -1;
    }

    int ndisplays = 0;
    screen_get_context_property_iv(g_screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &ndisplays);
    if (ndisplays < 1) {
        fprintf(stderr, "no Screen displays found in this guest\n");
        return -1;
    }

    screen_display_t *displays = calloc((size_t)ndisplays, sizeof(*displays));
    if (screen_get_context_property_pv(g_screen_ctx, SCREEN_PROPERTY_DISPLAYS,
                                        (void **)displays) != 0) {
        perror("screen_get_context_property_pv(DISPLAYS)");
        free(displays);
        return -1;
    }
    g_display = displays[0];
    free(displays);

    int actual_size[2] = { 0, 0 };
    screen_get_display_property_iv(g_display, SCREEN_PROPERTY_SIZE, actual_size);
    if (actual_size[0] <= 0 || actual_size[1] <= 0) {
        fprintf(stderr, "screen_get_display_property_iv returned bogus size %dx%d\n",
                actual_size[0], actual_size[1]);
        return -1;
    }
    *out_width  = (uint32_t)actual_size[0];
    *out_height = (uint32_t)actual_size[1];

    if (*out_width > FB_MAX_WIDTH || *out_height > FB_MAX_HEIGHT) {
        fprintf(stderr,
                "display is %ux%u, which exceeds FB_MAX_WIDTH/HEIGHT (%ux%u) "
                "in shm_proto.h - raise those constants and rebuild both "
                "fb_guest and fb_host\n",
                *out_width, *out_height, FB_MAX_WIDTH, FB_MAX_HEIGHT);
        return -1;
    }

    fprintf(stderr, "detected display size: %ux%u\n", *out_width, *out_height);
    return 0;
}

static int
screen_capture_init(uint32_t width, uint32_t height)
{
    if (screen_create_pixmap(&g_capture_pixmap, g_screen_ctx) != 0) {
        perror("screen_create_pixmap");
        return -1;
    }

    int size[2] = { (int)width, (int)height };
    screen_set_pixmap_property_iv(g_capture_pixmap, SCREEN_PROPERTY_BUFFER_SIZE, size);

    int format = SCREEN_FORMAT_RGBA8888;
    screen_set_pixmap_property_iv(g_capture_pixmap, SCREEN_PROPERTY_FORMAT, &format);

    int usage = SCREEN_USAGE_READ | SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
    screen_set_pixmap_property_iv(g_capture_pixmap, SCREEN_PROPERTY_USAGE, &usage);

    if (screen_create_pixmap_buffer(g_capture_pixmap) != 0) {
        perror("screen_create_pixmap_buffer");
        return -1;
    }

    if (screen_get_pixmap_property_pv(g_capture_pixmap, SCREEN_PROPERTY_RENDER_BUFFERS,
                                       (void **)&g_capture_buffer) != 0) {
        perror("screen_get_pixmap_property_pv(RENDER_BUFFERS)");
        return -1;
    }
    screen_get_buffer_property_iv(g_capture_buffer, SCREEN_PROPERTY_STRIDE, &g_capture_stride);
    screen_get_buffer_property_pv(g_capture_buffer, SCREEN_PROPERTY_POINTER, (void **)&g_capture_ptr);

    return 0;
}

static int
screen_capture_frame(uint8_t *dest,
                     uint32_t width,
                     uint32_t height,
                     uint32_t dest_stride)
{
    static int first_call = 1;
    if (first_call) {
        fprintf(stderr, "screen_capture_frame: calling first screen_read_display "
                        "(if this is the last line printed, it is BLOCKING here - "
                        "likely no scanout framebuffer on this virtual-display "
                        "Screen config)\n");
    }

    errno = 0;

    if (screen_read_display(g_display,
                            g_capture_buffer,
                            0,
                            NULL,
                            0) != 0) {
        fprintf(stderr,
                "screen_read_display failed: errno=%d (%s)\n",
                errno,
                strerror(errno));
        return -1;
    }

    if (first_call) {
        fprintf(stderr, "screen_capture_frame: first screen_read_display "
                        "RETURNED ok - capture path works\n");
        first_call = 0;
    }

    static uint32_t sample_count = 0;
    if (++sample_count % 30 == 0) {
        uint8_t *p0 = g_capture_ptr;
        uint8_t *p1 = g_capture_ptr + (size_t)(height / 2) * (uint32_t)g_capture_stride;
        fprintf(stderr,
            "capture sample: stride=%d px[0,0]=%02x%02x%02x%02x px[mid]=%02x%02x%02x%02x\n",
            g_capture_stride, p0[0], p0[1], p0[2], p0[3],
            p1[0], p1[1], p1[2], p1[3]);
    }

    for (uint32_t y = 0; y < height; y++) {
        memcpy(dest + (size_t)y * dest_stride,
               g_capture_ptr + (size_t)y * (uint32_t)g_capture_stride,
               (size_t)width * FB_BPP);
    }

    return 0;
}

/*
 * Fills a shmem framebuffer slot with a simple animated diagonal stripe
 * pattern - no Screen capture involved at all. Used by --test to check
 * whether the shmem hand-off + host display path works correctly given
 * known-good, guaranteed-non-blank content.
 */
static void
fill_test_pattern(uint8_t *dest, uint32_t width, uint32_t height,
                   uint32_t stride, uint32_t phase)
{
    for (uint32_t y = 0; y < height; y++) {
        uint8_t *row = dest + (size_t)y * stride;
        for (uint32_t x = 0; x < width; x++) {
            uint8_t v = (uint8_t)(((x + y + phase) / 8) % 2 ? 0xFF : 0x00);
            row[x * 4 + 0] = v;                     /* R */
            row[x * 4 + 1] = (uint8_t)(phase & 0xFF); /* G - cycles over time */
            row[x * 4 + 2] = (uint8_t)(0xFF - v);     /* B */
            row[x * 4 + 3] = 0xFF;                    /* A - fully opaque */
        }
    }
}

int
main(int argc, char **argv)
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    int test_mode = (argc > 1 && strcmp(argv[1], "--test") == 0);

    volatile struct guest_shm_factory *factory =
        mmap_device_memory(NULL, SHM_PAGE_SIZE,
                            PROT_READ | PROT_WRITE, 0,
                            (uint64_t)FACTORY_PADDR);
    if (factory == MAP_FAILED) {
        perror("mmap_device_memory(factory)");
        return 1;
    }

    if (factory->signature != GUEST_SHM_SIGNATURE) {
        fprintf(stderr, "shmem vdev signature mismatch (0x%llx) - check loc address\n",
                (unsigned long long)factory->signature);
        return 1;
    }

    uint32_t disp_w = 1024, disp_h = 600; /* defaults for --test */
    if (test_mode && argc > 3) {
        disp_w = (uint32_t)atoi(argv[2]);
        disp_h = (uint32_t)atoi(argv[3]);
    }
    if (!test_mode) {
        if (screen_query_display_size(&disp_w, &disp_h) != 0) {
            fprintf(stderr, "could not determine display size\n");
            return 1;
        }
    } else {
        fprintf(stderr, "--test mode: using %ux%u, no Screen capture involved\n",
                disp_w, disp_h);
    }

    size_t total_size = fb_shm_total_size(FB_MAX_WIDTH, FB_MAX_HEIGHT, FB_BPP);
    uint32_t size_pages = (uint32_t)(total_size / SHM_PAGE_SIZE);

    memset((void *)factory->name, 0, GUEST_SHM_MAX_NAME);
    strncpy((char *)factory->name, FB_REGION_NAME, GUEST_SHM_MAX_NAME - 1);

    __sync_synchronize();
    factory->size = size_pages;
    __sync_synchronize();

    if (factory->status != GSS_OK) {
        fprintf(stderr, "shmem create/attach failed, status=%u\n", factory->status);
        return 1;
    }

    uint64_t region_paddr = factory->shmem;

    fprintf(stderr, "attached: region_paddr=0x%llx size=%zu\n",
            (unsigned long long)region_paddr, total_size);

    munmap_device_memory((void *)factory, SHM_PAGE_SIZE);

    g_region = mmap_device_memory(NULL, total_size,
                                   PROT_READ | PROT_WRITE, 0,
                                   region_paddr);
    if (g_region == MAP_FAILED) {
        perror("mmap_device_memory(region)");
        return 1;
    }
    g_region_size = total_size;

    g_ctrl = (volatile struct guest_shm_control *)g_region;
    volatile struct fb_shm_header *hdr =
        (volatile struct fb_shm_header *)((uint8_t *)g_region + SHM_PAGE_SIZE);

    fprintf(stderr, "our connection index = %u\n", g_ctrl->idx);

    if (hdr->magic != FB_MAGIC) {
        uint32_t stride = disp_w * FB_BPP;
        uint32_t buf_sz = stride * disp_h;

        hdr->width       = disp_w;
        hdr->height      = disp_h;
        hdr->stride      = stride;
        hdr->format      = FB_FMT_RGBA8888;
        hdr->buffer_size = buf_sz;
        for (int i = 0; i < FB_MAX_BUFFERS; i++) {
            hdr->buffer_offset[i] = sizeof(struct fb_shm_header) + (uint32_t)i * buf_sz;
        }
        hdr->frame_seq          = 0;
        hdr->front_index        = 0;
        hdr->host_reading_index = FB_INVALID_INDEX;
        __sync_synchronize();
        hdr->magic = FB_MAGIC;
        __sync_synchronize();
    }

    if (!test_mode) {
        if (screen_capture_init(disp_w, disp_h) != 0) {
            fprintf(stderr, "screen capture init failed\n");
            return 1;
        }
    }

    fprintf(stderr, "capture initialized; entering producer loop\n");
    uint32_t heartbeat = 0;
    while (g_running) {
        uint32_t front = hdr->front_index;
        uint32_t reading = hdr->host_reading_index;

        uint32_t back_index = FB_INVALID_INDEX;
        for (uint32_t i = 0; i < FB_MAX_BUFFERS; i++) {
            if (i != front && i != reading) {
                back_index = i;
                break;
            }
        }
        if (back_index == FB_INVALID_INDEX) {
            usleep(1000);
            continue;
        }

        uint8_t *back_buf = (uint8_t *)hdr + hdr->buffer_offset[back_index];

        int capture_ok;
        if (test_mode) {
            fill_test_pattern(back_buf, hdr->width, hdr->height, hdr->stride, heartbeat);
            capture_ok = 0;
        } else {
            capture_ok = screen_capture_frame(back_buf, hdr->width, hdr->height, hdr->stride);
        }

        if (capture_ok != 0) {
            usleep(33000);
            continue;
        }

        __sync_synchronize();
        hdr->frame_seq++;
        hdr->front_index = back_index;
        __sync_synchronize();

        g_ctrl->notify = ~0u;

        if (++heartbeat % 30 == 0) {
            fprintf(stderr, "guest: published %u frames (frame_seq=%u, front=%u)\n",
                    heartbeat, hdr->frame_seq, hdr->front_index);
        }

        usleep(33000);
    }

    g_ctrl->detach = 1;
    munmap_device_memory(g_region, g_region_size);
    return 0;
}