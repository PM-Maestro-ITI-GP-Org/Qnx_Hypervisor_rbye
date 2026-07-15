/*
 * vdev-virtio-gpu — QNX hypervisor (qvm) backend for a paravirtual GPU with virgl 3D.
 *
 * The Linux guest runs mainline drm/virtio + Mesa virgl; this host-side vdev forwards
 * the guest's virtio-gpu commands into virglrenderer, which renders on QNX's V3D
 * (EGL/GLES) and scans out through QNX Screen.
 *
 * Threading: virglrenderer + its EGL context are thread-affine. All virgl calls run on
 * a single dedicated RENDER THREAD that owns the context (screen_create_context +
 * virgl_renderer_init USE_EGL|USE_SURFACELESS|USE_GLES). gpu_vwrite (guest queue-kick)
 * just signals the render thread; it never touches virgl.
 *
 * Prereqs on the host (before qvm launches this guest): QNX Screen must be running
 * (graphics_start.sh) so eglGetDisplay(EGL_DEFAULT_DISPLAY) connects; and the process
 * must load the libdrm.so.2-patched libvirglrenderer (see patches/README.md).
 *
 * Fencing is synchronous (create_fence + poll before responding) and the render thread
 * serializes on render_mtx; async fences + pipelining are a later optimization (see the
 * README's "Deliberate simplifications"). Scanout (present_scanout) is a per-frame GPU
 * readback into a QNX Screen client window + screen_post_window — not raw WFD, since
 * Screen owns both display pipelines.
 */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <sys/uio.h>

#include <qvm/vdev-core.h>
#include <qvm/guest.h>
#include <qvm/gasp.h>
#include <qvm/vio.h>
#include <qvm/log.h>
#include <virtio.h>
#include <vq.h>

#include <screen/screen.h>
#include "virglrenderer.h"
#include "virgl_hw.h"      /* full struct virgl_box (virglrenderer.h only fwd-declares it) */
#include "virtio_gpu.h"
#include "edid_build.inc"  /* build_edid() — pure, host-testable (see edid_selftest.c) */

#ifndef VIRTQ_DESC_F_WRITE
#define VIRTQ_DESC_F_WRITE 2
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define GPU_DEFAULT_W  1920
#define GPU_DEFAULT_H  1080
#define GPU_CTRLQ_SIZE 256
#define GPU_CURSORQ_SIZE 16
#define GPU_MAX_RESOURCES 4096
/* SUBMIT_3D command streams from Mesa can be large; gather buffer sized generously. */
#define GPU_CMD_MAX  (512 * 1024)
#define GPU_RESP_MAX (16 * 1024)     /* capset blobs are the largest response; virgl_caps_v2 can exceed 4K */

/* A guest resource: its virgl handle + the guest backing pages mapped into our
 * address space (iov, kept alive until DETACH_BACKING/UNREF). */
struct gpu_resource {
    uint32_t          id;            /* 0 == free slot */
    struct iovec     *iov;
    unsigned          niov;
};

struct gpu_state {
    struct vio_state          vs;
    struct vio_description    desc;
    struct virtio_gpu_config  cfg;
    enum vio_version          ver;
    uint32_t                  disp_w;
    uint32_t                  disp_h;
    uint32_t                  disp_hz;      /* live Screen refresh; drives pacing + EDID */
    uint32_t                  scanout_resource;

    /* render thread */
    pthread_t                 render_thr;
    pthread_mutex_t           render_mtx;
    pthread_cond_t            work_cv;
    pthread_cond_t            idle_cv;      /* render thread signals it has left process_ctrlq */
    int                       notified;
    int                       running;
    int                       virgl_ok;
    int                       quiescing;    /* reset in progress: render thread must re-park */
    int                       in_ctrlq;     /* render thread is currently inside process_ctrlq */
    uint64_t                  fence_done;   /* highest fence id write_fence has reported */
    screen_context_t          screen_ctx;
    screen_window_t           screen_win;   /* scanout window (lazy, render thread) */
    int                       scanout_display; /* 1-based Screen display index; 0 = last */

    struct gpu_resource       res[GPU_MAX_RESOURCES];
};

enum { OPT_WIDTH_IDX = 0, OPT_HEIGHT_IDX, OPT_DISPLAY_IDX, OPT_NUM_OPTS };

/* ---- resource table ------------------------------------------------------- */

static struct gpu_resource *res_find(struct gpu_state *s, uint32_t id)
{
    if (id == 0) return NULL;
    for (unsigned i = 0; i < GPU_MAX_RESOURCES; ++i)
        if (s->res[i].id == id) return &s->res[i];
    return NULL;
}

static struct gpu_resource *res_alloc(struct gpu_state *s, uint32_t id)
{
    struct gpu_resource *r = res_find(s, id);
    if (r) return r;
    for (unsigned i = 0; i < GPU_MAX_RESOURCES; ++i)
        if (s->res[i].id == 0) { s->res[i].id = id; return &s->res[i]; }
    return NULL;
}

/* unmap + free a resource's guest backing */
static void res_drop_backing(struct gpu_resource *r)
{
    for (unsigned i = 0; i < r->niov; ++i)
        if (r->iov[i].iov_base)
            gasp_unmap(r->iov[i].iov_base, r->iov[i].iov_len, 0);
    free(r->iov);
    r->iov = NULL;  r->niov = 0;
}

/* Map the guest mem_entry array (trailing @ent for @n entries) into an iov. */
static int res_attach_backing(vdev_t *vdp, struct gpu_resource *r,
                              const struct virtio_gpu_mem_entry *ent, unsigned n)
{
    res_drop_backing(r);
    if (n == 0) return 0;
    r->iov = calloc(n, sizeof(*r->iov));
    if (!r->iov) return ENOMEM;
    for (unsigned i = 0; i < n; ++i) {
        void *va = gasp_map_vdma(vdp, ent[i].addr, ent[i].length, GMF_READ | GMF_WRITE);
        if (!va) {
            /* A partial map = a shorter iov list than the guest's mem_entry list,
             * which silently misaligns every later transfer offset. Fail the whole
             * attach instead of handing virgl a truncated backing. */
            r->niov = i;              /* unmap only what we already mapped */
            res_drop_backing(r);
            return EIO;
        }
        r->iov[i].iov_base = va;
        r->iov[i].iov_len  = ent[i].length;
    }
    r->niov = n;
    return 0;
}

/* ---- scanout -------------------------------------------------------------- */

/*
 * Scanout: a fullscreen QNX Screen window on the target display. Screen (not us)
 * owns WFD/both display pipelines, so the vdev is a plain Screen client — no
 * ownership fight with the host cluster UI on the other display.
 * per-flush GPU readback (transfer_read_iov straight into the Screen
 * buffer) — simple, format-agnostic, covers fbcon(2D) and virgl(3D) uniformly.
 * Upgrade path if fps matters: export the virgl resource as dmabuf
 * (resource_get_info.fd) and import zero-copy into Screen.
 */
/* Resolve the scanout-display option (1-based; <=0 = LAST display, HDMI1 in the
 * 2-display graphics.conf) to a Screen display handle. NULL if none. */
static screen_display_t gpu_target_display(struct gpu_state *s)
{
    int n = 0;
    (void)screen_get_context_property_iv(s->screen_ctx, SCREEN_PROPERTY_DISPLAY_COUNT, &n);
    if (n <= 0) return NULL;
    screen_display_t *disps = calloc((size_t)n, sizeof(*disps));
    if (!disps) return NULL;
    screen_display_t d = NULL;
    if (screen_get_context_property_pv(s->screen_ctx, SCREEN_PROPERTY_DISPLAYS,
                                       (void **)disps) == 0) {
        int idx = (s->scanout_display > 0) ? s->scanout_display - 1 : n - 1;
        if (idx >= n) idx = n - 1;
        d = disps[idx];
    }
    free(disps);
    return d;
}

/* Read the target display's LIVE mode from Screen (single source of truth) into
 * disp_w/h/hz. scanout-width/height options, if given, win over the queried size;
 * refresh always comes from the panel. This is what makes a different-Hz screen
 * plug-and-play — swap the panel + graphics.conf, no vdev rebuild. Render thread. */
static void gpu_query_display_mode(vdev_t *vdp, struct gpu_state *s)
{
    screen_display_t d = gpu_target_display(s);
    if (!d) return;
    screen_display_mode_t mode;
    memset(&mode, 0, sizeof(mode));
    if (screen_get_display_property_pv(d, SCREEN_PROPERTY_MODE, (void **)&mode) != 0)
        return;
    if (mode.refresh)                  s->disp_hz = mode.refresh;
    if (s->disp_w == 0 && mode.width)  s->disp_w  = mode.width;
    if (s->disp_h == 0 && mode.height) s->disp_h  = mode.height;
    fprintf(stderr, "virtio-gpu vdev: Screen mode %ux%u@%uHz (disp %s)\n",
            mode.width, mode.height, mode.refresh,
            s->scanout_display ? "option" : "last");
}

static int scanout_window_create(vdev_t *vdp, struct gpu_state *s)
{
    if (screen_create_window(&s->screen_win, s->screen_ctx) != 0) {
        vdev_logf(QL_QVM_ERR, vdp, "virtio-gpu: screen_create_window failed");
        return 0;
    }

    screen_display_t d = gpu_target_display(s);
    if (d)
        screen_set_window_property_pv(s->screen_win, SCREEN_PROPERTY_DISPLAY,
                                      (void **)&d);

    /* Qt eglfs on the guest scans out DRM_FORMAT_XRGB/ARGB8888 (little-endian =
     * B,G,R,A byte order). Screen's BGRA8888 label read those bytes R/B-swapped
     * (yellow->ice-blue, blue-purple->reddish on real UI colors), so use RGBA8888. */
    int fmt = SCREEN_FORMAT_RGBA8888;
    int usage = SCREEN_USAGE_READ | SCREEN_USAGE_WRITE;   /* CPU-writable buffer */
    int size[2] = { (int)s->disp_w, (int)s->disp_h };
    int vis = 1;
    int swap = 1;   /* post to vblank: this is what paces the guest to the panel */
    screen_set_window_property_iv(s->screen_win, SCREEN_PROPERTY_FORMAT, &fmt);
    screen_set_window_property_iv(s->screen_win, SCREEN_PROPERTY_USAGE, &usage);
    screen_set_window_property_iv(s->screen_win, SCREEN_PROPERTY_SIZE, size);
    screen_set_window_property_iv(s->screen_win, SCREEN_PROPERTY_SWAP_INTERVAL, &swap);
    screen_set_window_property_iv(s->screen_win, SCREEN_PROPERTY_VISIBLE, &vis);

    /* DOUBLE buffer + SWAP_INTERVAL=1: screen_post blocks to vblank and the two
     * buffers alternate, so the guest locks to the real refresh (60 on 60 Hz,
     * 120 on 120 Hz) with no fixed software timer. The earlier single-buffer
     * strobe came from posting a never-filled 2nd buffer; present now reads back
     * into the CURRENT render buffer each frame and posts THAT, so both stay live. */
    if (screen_create_window_buffers(s->screen_win, 2) != 0) {
        vdev_logf(QL_QVM_ERR, vdp, "virtio-gpu: screen window buffers failed");
        screen_destroy_window(s->screen_win);
        s->screen_win = NULL;
        return 0;
    }
    fprintf(stderr, "virtio-gpu vdev: scanout window %ux%u@%uHz up (display %s)\n",
            s->disp_w, s->disp_h, s->disp_hz, s->scanout_display ? "option" : "last");
    return 1;
}

/* capped early-return logger so a silent bail can't hide */
#define PRESENT_BAIL(fmt, ...) do {                                         \
        static unsigned nbail;                                              \
        if (nbail < 8) {                                                    \
            fprintf(stderr, "virtio-gpu: present bail: " fmt "\n", ##__VA_ARGS__); \
            nbail++;                                                        \
        }                                                                   \
        return;                                                             \
    } while (0)

static void present_scanout(vdev_t *vdp, struct gpu_state *s)
{
    /* scanout-display -1 = headless: render normally but never touch Screen.
     * Diagnostic toggle to separate "present path destabilizes the display"
     * from system-level causes (PSU brownout / thermal). */
    if (s->scanout_display < 0)
        return;

    if (!s->scanout_resource)
        PRESENT_BAIL("no scanout resource");

    /* Anti-runaway backstop only: the vsync'd double-buffered post (below) is the
     * real governor. Drop presents that arrive faster than one refresh interval so
     * a burst can't spin the full-frame readback and wedge the V3D. Derived from the
     * live panel refresh, so a higher-Hz screen needs no change here. Guest rendering
     * is NOT throttled by this, only display updates. */
    long floor_us = 1000000L / (s->disp_hz ? s->disp_hz : 60);
    static struct timespec last;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long us = (now.tv_sec - last.tv_sec) * 1000000L + (now.tv_nsec - last.tv_nsec) / 1000L;
    if (last.tv_sec && us < floor_us)
        return;
    last = now;

    struct virgl_renderer_resource_info info;
    int gi = virgl_renderer_resource_get_info((int)s->scanout_resource, &info);
    if (gi != 0)
        PRESENT_BAIL("get_info(res=%u) = %d", s->scanout_resource, gi);

    if (!s->screen_win && !scanout_window_create(vdp, s))
        PRESENT_BAIL("window create failed");

    screen_buffer_t bufs[2] = { NULL, NULL };
    if (screen_get_window_property_pv(s->screen_win, SCREEN_PROPERTY_RENDER_BUFFERS,
                                      (void **)bufs) != 0 || !bufs[0])
        PRESENT_BAIL("no render buffers");

    void *ptr = NULL;
    int stride = 0;
    screen_get_buffer_property_pv(bufs[0], SCREEN_PROPERTY_POINTER, &ptr);
    screen_get_buffer_property_iv(bufs[0], SCREEN_PROPERTY_STRIDE, &stride);
    if (!ptr || stride <= 0)
        PRESENT_BAIL("buffer ptr=%p stride=%d", ptr, stride);

    uint32_t w = min(info.width, s->disp_w);
    uint32_t h = min(info.height, s->disp_h);
    struct virgl_box box = { .x = 0, .y = 0, .z = 0, .w = w, .h = h, .d = 1 };
    struct iovec iov = { .iov_base = ptr, .iov_len = (size_t)stride * h };

    /* GPU -> Screen buffer readback; stride = destination row pitch. */
    int tr = virgl_renderer_transfer_read_iov((int)s->scanout_resource, 0, 0,
                                              (uint32_t)stride, 0, &box, 0, &iov, 1);

    (void)tr;   /* readback errors surface as a frozen guest, not worth a per-frame log */

    int rect[4] = { 0, 0, (int)w, (int)h };
    screen_post_window(s->screen_win, bufs[0], 1, rect, 0);
}

/* ---- virgl fence (synchronous) ------------------------------------------- */

static void cb_write_fence(void *cookie, uint32_t fence)
{
    vdev_t *vdp = cookie;
    struct gpu_state *s = vdp->v_device;
    if (fence > s->fence_done) s->fence_done = fence;
}

/* Block until virgl has retired @fence (drives write_fence via poll).
 * MUST NOT busy-spin: at qvm thread priority a hot loop starves the whole host
 * (incl. the UART console) if the GPU wedges. Sleep between polls + hard 2s cap. */
static void fence_wait(struct gpu_state *s, uint32_t fence)
{
    for (int ms = 0; s->fence_done < fence && ms < 2000; ++ms) {
        virgl_renderer_poll();
        if (s->fence_done >= fence)
            break;
        usleep(1000);
    }
}

/* ---- command dispatch (RENDER THREAD ONLY — EGL context is current here) --- */

/* Guest is untrusted: every case below casts `cmd` to a struct larger than the
 * ctrl_hdr already validated. Reject a command shorter than the struct it claims
 * to be, else we'd read stale bytes left in the reused static command buffer by
 * the previous command. Bare-if (NOT do/while) so `break` targets the switch. */
#define NEED(sz) if (cmdlen < (sz)) { rh->type = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER; break; }

static unsigned
handle_cmd(vdev_t *vdp, const uint8_t *cmd, unsigned cmdlen,
           uint8_t *resp, unsigned resp_cap)
{
    struct gpu_state *s = vdp->v_device;
    if (cmdlen < sizeof(struct virtio_gpu_ctrl_hdr)) {
        /* Runt command — no field is trustworthy. Still hand the guest a proper
         * ERR header when it gave us room, rather than a zero-length used buffer
         * it then reads as stale bytes. */
        if (resp_cap >= sizeof(struct virtio_gpu_ctrl_hdr)) {
            struct virtio_gpu_ctrl_hdr *erh = (void *)resp;
            memset(erh, 0, sizeof(*erh));
            erh->type = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
            return sizeof(*erh);
        }
        return 0;
    }
    const struct virtio_gpu_ctrl_hdr *h = (const void *)cmd;

    struct virtio_gpu_ctrl_hdr *rh = (void *)resp;
    memset(rh, 0, sizeof(*rh));
    rh->type = VIRTIO_GPU_RESP_OK_NODATA;
    rh->fence_id = h->fence_id;
    rh->ctx_id = h->ctx_id;
    unsigned resplen = sizeof(*rh);

    switch (h->type) {
    /* ---- display / 2D ---- */
    case VIRTIO_GPU_CMD_GET_DISPLAY_INFO: {
        struct virtio_gpu_resp_display_info *di = (void *)resp;
        if (resp_cap < sizeof(*di)) { rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC; break; }
        memset(di, 0, sizeof(*di));
        di->hdr.type = VIRTIO_GPU_RESP_OK_DISPLAY_INFO;
        di->hdr.fence_id = h->fence_id;
        di->pmodes[0].r.width = s->disp_w;
        di->pmodes[0].r.height = s->disp_h;
        di->pmodes[0].enabled = 1;
        resplen = sizeof(*di);
        break;
    }
    case VIRTIO_GPU_CMD_GET_EDID: {
        const struct virtio_gpu_get_edid *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct virtio_gpu_resp_edid *re = (void *)resp;
        if (resp_cap < sizeof(*re)) { rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC; break; }
        memset(re, 0, sizeof(*re));
        re->hdr.type = VIRTIO_GPU_RESP_OK_EDID;
        re->hdr.fence_id = h->fence_id;
        re->size = 128;                    /* one base block, no extensions */
        build_edid(re->edid, s->disp_w, s->disp_h, s->disp_hz);
        resplen = sizeof(*re);
        break;
    }
    case VIRTIO_GPU_CMD_GET_CAPSET_INFO: {
        const struct virtio_gpu_get_capset_info *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct virtio_gpu_resp_capset_info *ri = (void *)resp;
        memset(ri, 0, sizeof(*ri));
        ri->hdr.type = VIRTIO_GPU_RESP_OK_CAPSET_INFO;
        ri->hdr.fence_id = h->fence_id;
        ri->capset_id = (c->capset_index == 0) ? VIRTIO_GPU_CAPSET_VIRGL
                                               : VIRTIO_GPU_CAPSET_VIRGL2;
        virgl_renderer_get_cap_set(ri->capset_id, &ri->capset_max_version,
                                   &ri->capset_max_size);
        resplen = sizeof(*ri);
        break;
    }
    case VIRTIO_GPU_CMD_GET_CAPSET: {
        const struct virtio_gpu_get_capset *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct virtio_gpu_resp_capset *rc = (void *)resp;
        uint32_t max_ver, max_size;
        virgl_renderer_get_cap_set(c->capset_id, &max_ver, &max_size);
        if (sizeof(*rc) + max_size > resp_cap) { rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC; break; }
        memset(rc, 0, sizeof(*rc) + max_size);
        rc->hdr.type = VIRTIO_GPU_RESP_OK_CAPSET;
        rc->hdr.fence_id = h->fence_id;
        virgl_renderer_fill_caps(c->capset_id, c->capset_version, rc->capset_data);
        resplen = sizeof(*rc) + max_size;
        break;
    }
    case VIRTIO_GPU_CMD_RESOURCE_CREATE_2D: {
        const struct virtio_gpu_resource_create_2d *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_alloc(s, c->resource_id);
        if (!r) { rh->type = VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY; break; }
        /* map a 2D resource into virgl as a simple BGRA texture */
        struct virgl_renderer_resource_create_args a = {
            .handle = c->resource_id, .target = 2 /*PIPE_TEXTURE_2D*/,
            .format = c->format, .bind = 2 /*PIPE_BIND_RENDER_TARGET*/,
            .width = c->width, .height = c->height, .depth = 1, .array_size = 1,
        };
        if (virgl_renderer_resource_create(&a, NULL, 0) != 0) {
            r->id = 0;                 /* release the slot we just claimed */
            rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC;
        }
        break;
    }
    case VIRTIO_GPU_CMD_RESOURCE_CREATE_3D: {
        const struct virtio_gpu_resource_create_3d *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_alloc(s, c->resource_id);
        if (!r) { rh->type = VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY; break; }
        struct virgl_renderer_resource_create_args a = {
            .handle = c->resource_id, .target = c->target, .format = c->format,
            .bind = c->bind, .width = c->width, .height = c->height, .depth = c->depth,
            .array_size = c->array_size, .last_level = c->last_level,
            .nr_samples = c->nr_samples, .flags = c->flags,
        };
        if (virgl_renderer_resource_create(&a, NULL, 0) != 0) {
            r->id = 0;
            rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC;
        }
        break;
    }
    case VIRTIO_GPU_CMD_RESOURCE_UNREF: {
        const struct virtio_gpu_resource_unref *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_find(s, c->resource_id);
        if (!r) { rh->type = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID; break; }
        /* Detach our iov from virgl BEFORE freeing it: attach_iov handed virgl a
         * pointer into r->iov; unref alone doesn't release it, so res_drop_backing
         * would free memory virgl still references (UAF). */
        struct iovec *iov = NULL; int niov = 0;
        virgl_renderer_resource_detach_iov((int)c->resource_id, &iov, &niov);
        virgl_renderer_resource_unref(c->resource_id);
        res_drop_backing(r);
        memset(r, 0, sizeof(*r));
        break;
    }
    case VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING: {
        const struct virtio_gpu_resource_attach_backing *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_find(s, c->resource_id);
        if (!r) { rh->type = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID; break; }
        const struct virtio_gpu_mem_entry *ent = (const void *)(c + 1);
        unsigned avail = (cmdlen - sizeof(*c)) / sizeof(*ent);
        if (res_attach_backing(vdp, r, ent, min(c->nr_entries, avail)) != 0) {
            rh->type = VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY; break;
        }
        if (virgl_renderer_resource_attach_iov(c->resource_id, r->iov, r->niov) != 0) {
            /* virgl rejected the backing — don't leave the guest with OK_NODATA and
             * no backing (silent no-op transfers). Drop our mapping and report the error. */
            res_drop_backing(r);
            rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC;
        }
        break;
    }
    case VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING: {
        const struct virtio_gpu_resource_unref *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_find(s, c->resource_id);
        if (r) {
            /* Was calling ctx_detach_resource — the wrong op (and ctx_id==0 on the
             * 2D path). Release the iov virgl holds, THEN free it (see UNREF). */
            struct iovec *iov = NULL; int niov = 0;
            virgl_renderer_resource_detach_iov((int)c->resource_id, &iov, &niov);
            res_drop_backing(r);
        }
        break;
    }
    case VIRTIO_GPU_CMD_SET_SCANOUT: {
        const struct virtio_gpu_set_scanout *c = (const void *)cmd;
        NEED(sizeof(*c));
        if (c->scanout_id != 0) { rh->type = VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID; break; }
        s->scanout_resource = c->resource_id;
        break;
    }
    case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D: {
        const struct virtio_gpu_transfer_to_host_2d *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_find(s, c->resource_id);
        if (!r) { rh->type = VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID; break; }
        struct virgl_box box = { .x = c->r.x, .y = c->r.y, .w = c->r.width, .h = c->r.height, .d = 1 };
        virgl_renderer_transfer_write_iov(c->resource_id, 0, 0, 0, 0, &box,
                                          c->offset, r->iov, r->niov);
        break;
    }
    case VIRTIO_GPU_CMD_RESOURCE_FLUSH: {
        const struct virtio_gpu_resource_flush *c = (const void *)cmd;
        NEED(sizeof(*c));
        /* Only the resource actually on scanout: fbcon keeps flushing its hidden
         * framebuffer (cursor blink damage) while a GL app owns the display.
         * Pacing now lives in the vsync'd post inside present_scanout — no fixed
         * software timer, so the guest tracks whatever refresh the panel reports. */
        if (c->resource_id == s->scanout_resource)
            present_scanout(vdp, s);
        break;
    }

    /* ---- virgl 3D ---- */
    case VIRTIO_GPU_CMD_CTX_CREATE: {
        const struct virtio_gpu_ctx_create *c = (const void *)cmd;
        NEED(sizeof(*c));
        virgl_renderer_context_create(h->ctx_id, c->nlen, c->debug_name);
        break;
    }
    case VIRTIO_GPU_CMD_CTX_DESTROY:
        virgl_renderer_context_destroy(h->ctx_id);
        break;
    case VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE: {
        const struct virtio_gpu_ctx_resource *c = (const void *)cmd;
        NEED(sizeof(*c));
        virgl_renderer_ctx_attach_resource(h->ctx_id, c->resource_id);
        break;
    }
    case VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE: {
        const struct virtio_gpu_ctx_resource *c = (const void *)cmd;
        NEED(sizeof(*c));
        virgl_renderer_ctx_detach_resource(h->ctx_id, c->resource_id);
        break;
    }
    case VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D: {
        const struct virtio_gpu_transfer_host_3d *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_find(s, c->resource_id);
        struct virgl_box box = { c->box.x, c->box.y, c->box.z, c->box.w, c->box.h, c->box.d };
        virgl_renderer_transfer_write_iov(c->resource_id, h->ctx_id, c->level, c->stride,
                                          c->layer_stride, &box, c->offset,
                                          r ? r->iov : NULL, r ? r->niov : 0);
        break;
    }
    case VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D: {
        const struct virtio_gpu_transfer_host_3d *c = (const void *)cmd;
        NEED(sizeof(*c));
        struct gpu_resource *r = res_find(s, c->resource_id);
        struct virgl_box box = { c->box.x, c->box.y, c->box.z, c->box.w, c->box.h, c->box.d };
        virgl_renderer_transfer_read_iov(c->resource_id, h->ctx_id, c->level, c->stride,
                                         c->layer_stride, &box, c->offset,
                                         r ? r->iov : NULL, r ? r->niov : 0);
        break;
    }
    case VIRTIO_GPU_CMD_SUBMIT_3D: {
        const struct virtio_gpu_cmd_submit *c = (const void *)cmd;
        NEED(sizeof(*c));
        void *stream = (void *)(c + 1);
        if (sizeof(*c) + c->size <= cmdlen)
            virgl_renderer_submit_cmd(stream, h->ctx_id, c->size / 4);
        else
            rh->type = VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER;
        break;
    }

    default:
        vdev_logf(QL_QVM_DBG, vdp, "virtio-gpu: unhandled cmd 0x%x", h->type);
        rh->type = VIRTIO_GPU_RESP_ERR_UNSPEC;
        break;
    }

    /* Fenced command: make sure virgl has retired the work before we answer, and
     * echo FLAG_FENCE + fence_id in the response — the virtio-gpu spec REQUIRES it;
     * the guest signals its dma_fences off the response flags. Without this every
     * guest fence stays pending forever and the pipeline freezes after frame 1
     * (rh is the ctrl_hdr at the start of every response type). */
    if (h->flags & VIRTIO_GPU_FLAG_FENCE) {
        rh->flags |= VIRTIO_GPU_FLAG_FENCE;
        rh->fence_id = h->fence_id;
        virgl_renderer_create_fence((int)h->fence_id, h->ctx_id);
        fence_wait(s, h->fence_id);
        if (s->fence_done < h->fence_id)   /* only a stuck fence is worth logging */
            fprintf(stderr, "virtio-gpu: fence %llu wait TIMED OUT (done=%llu, cmd 0x%x)\n",
                    (unsigned long long)h->fence_id,
                    (unsigned long long)s->fence_done, h->type);
    }
    return min(resplen, resp_cap);
}
#undef NEED

/* Drain the control queue (render thread). */
static void process_ctrlq(vdev_t *vdp)
{
    struct gpu_state *s = vdp->v_device;
    vq_t *vq = s->vs.qinfo[VIRTIO_GPU_CONTROLQ].pvq;
    if (!vq) return;                   /* guest kicked before DRIVER_OK set the queue up */
    static uint8_t cmd[GPU_CMD_MAX];   /* render thread only */
    static uint8_t resp[GPU_RESP_MAX];
    unsigned first, bufid;

    /* !quiescing: abandon an in-progress drain the moment a reset is requested, so
     * the RESET handshake completes within one in-flight command. */
    while (!s->quiescing && (bufid = vq_get_from_driver(vq, &first)) != VQ_INVALID_ID) {
        unsigned cmdlen = 0;
        uint64_t resp_addr = 0;
        uint32_t resp_cap = 0;
        unsigned idx = first;
        for (;;) {
            uint64_t daddr; uint32_t dlen; unsigned dflags;
            unsigned dnext = vq_desc_get(vq, idx, &daddr, &dlen, &dflags);
            if (dflags & VIRTQ_DESC_F_WRITE) {
                /* Assume the response fits the FIRST writable descriptor. Holds for
                 * mainline virtio_gpu.c (one cmd+resp 2-descriptor chain); the spec
                 * permits the response to scatter across several, but the Linux
                 * driver never does — revisit if a different guest driver appears. */
                if (resp_cap == 0) { resp_addr = daddr; resp_cap = dlen; }
                } else if (cmdlen < sizeof(cmd)) {
                void *va = gasp_map_vdma(vdp, daddr, dlen, GMF_READ);
                if (va) {
                    unsigned c = min(dlen, (uint32_t)(sizeof(cmd) - cmdlen));
                    memcpy(cmd + cmdlen, va, c);
                    cmdlen += c;
                    gasp_unmap(va, dlen, 0);
                }
            }
            if (dnext == VQ_INVALID_ID) break;
            idx = dnext;
        }

        unsigned resplen = handle_cmd(vdp, cmd, cmdlen, resp, min(resp_cap, (uint32_t)sizeof(resp)));
        if (resp_cap && resplen) {
            void *va = gasp_map_vdma(vdp, resp_addr, resp_cap, GMF_WRITE);
            if (va) {
                memcpy(va, resp, min(resplen, resp_cap));
                gasp_unmap(va, resp_cap, 1); /* flush=1: host wrote data guest must read */
            }
        }
        vq_put_to_driver(vq, bufid, resplen, true);
    }
}

/* Render thread: owns the EGL context + virgl; processes the control queue. */
static void *render_thread(void *arg)
{
    vdev_t *vdp = arg;
    struct gpu_state *s = vdp->v_device;

    /* Run BELOW the console/drivers: GPU work must never starve the host if the
     * V3D wedges (a hung render thread then just idles at low priority). */
    struct sched_param sp = { .sched_priority = 8 };
    (void)pthread_setschedparam(pthread_self(), SCHED_RR, &sp);

    /* QNX EGL needs a per-process Screen connection before eglInitialize.
     * CRITICAL ordering: if Screen isn't up, libkhronos exit()s the WHOLE qvm process
     * on the first EGL call ("Failed to open configuration file"). So do not touch
     * EGL/virgl until screen_create_context succeeds — it's a safe probe that just
     * returns an error while Screen is down. Retry while the host brings Screen up. */
    int scr = -1;
    for (int attempt = 0; attempt < 60 && scr != 0; ++attempt) {
        scr = screen_create_context(&s->screen_ctx, SCREEN_APPLICATION_CONTEXT);
        if (scr != 0) sleep(1);
    }
    if (scr != 0) {
        fprintf(stderr, "virtio-gpu vdev: QNX Screen not available after 60s — "
                        "virgl 3D disabled (run graphics_start.sh, then restart qvm)\n");
        return NULL;
    }

    /* Screen is up: adopt the live display mode as the source of truth (options,
     * if set, already pinned disp_w/h; refresh always comes from the panel). Fill
     * any gap with the compiled defaults so GET_DISPLAY_INFO/EDID are never 0. */
    gpu_query_display_mode(vdp, s);
    if (!s->disp_w)  s->disp_w  = GPU_DEFAULT_W;
    if (!s->disp_h)  s->disp_h  = GPU_DEFAULT_H;
    if (!s->disp_hz) s->disp_hz = 60;

    struct virgl_renderer_callbacks cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.version = 1;
    cbs.write_fence = cb_write_fence;

    int flags = VIRGL_RENDERER_USE_EGL | VIRGL_RENDERER_USE_SURFACELESS | VIRGL_RENDERER_USE_GLES;
    int r = virgl_renderer_init(vdp, flags, &cbs);   /* cookie=vdp (non-NULL) */
    s->virgl_ok = (r == 0);
    fprintf(stderr, "virtio-gpu vdev: virgl_renderer_init = %d (%s)\n",
            r, r == 0 ? "OK" : "FAIL");

    pthread_mutex_lock(&s->render_mtx);
    while (s->running) {
        while (s->running && !s->notified)
            pthread_cond_wait(&s->work_cv, &s->render_mtx);
        s->notified = 0;
        if (!s->running) break;
        if (s->quiescing) continue;          /* re-park until the reset completes */
        s->in_ctrlq = 1;
        pthread_mutex_unlock(&s->render_mtx);
        if (s->virgl_ok) process_ctrlq(vdp);
        pthread_mutex_lock(&s->render_mtx);
        s->in_ctrlq = 0;
        pthread_cond_broadcast(&s->idle_cv);  /* let a waiting reset proceed */
    }
    pthread_mutex_unlock(&s->render_mtx);
    return NULL;
}

static void render_kick(struct gpu_state *s)
{
    pthread_mutex_lock(&s->render_mtx);
    s->notified = 1;
    pthread_cond_signal(&s->work_cv);
    pthread_mutex_unlock(&s->render_mtx);
}

/* Cursor queue: we render no host cursor (guest is fullscreen eglfs; a pointer
 * device is the only source of cursor traffic). But eglfs_kms enables hwcursor
 * by default, and the guest's virtio_gpu_queue_cursor() blocks the atomic commit
 * on cursorq's ack_queue once the 16-entry ring fills — an ack we'd otherwise
 * never send, which presents as a silent wedge indistinguishable from a V3D hang.
 * Drain and complete each buffer as a no-op so the guest keeps flowing. Runs on
 * the MMIO thread (no virgl, no render thread); len-0 completion is fine — the
 * guest driver doesn't inspect cursor responses, it just needs the ring reclaimed. */
static void drain_cursorq(struct gpu_state *s)
{
    vq_t *vq = s->vs.qinfo[VIRTIO_GPU_CURSORQ].pvq;
    if (!vq) return;                   /* guest kicked before DRIVER_OK / mid-reset */
    unsigned first, bufid;
    while ((bufid = vq_get_from_driver(vq, &first)) != VQ_INVALID_ID)
        vq_put_to_driver(vq, bufid, 0, true);
}

/* ---- vio transport glue (guest MMIO; never calls virgl) ------------------- */

static vdev_ref_status_t
gpu_vread(vdev_t *vdp, unsigned cookie, const struct qvm_state_block *vopnd,
          const struct qvm_state_block *oopnd, struct guest_cpu *gcp)
{
    struct gpu_state *s = vdp->v_device;
    vdev_ref_status_t ret = VRS_NORMAL;
    pthread_mutex_lock(&vdp->v_mtx);
    unsigned res = vio_read(&s->vs, cookie, vopnd, oopnd, gcp);

    /* qvm's vio helper doesn't implement the virtio-mmio SHM registers (0xac..0xbc)
     * and returns 0 for SHM_LEN — which the guest virtio-gpu driver reads as a
     * zero-size host-visible region and then fails to reserve (probe -EBUSY). We
     * support no SHM regions (no blob/host-visible), so report SHM_LEN = ~0 = "no
     * region" for any selected id; the guest then skips host-visible cleanly. */
    /* Keys off the low 12 bits of the register offset, which is where SHM_LEN sits
     * in the virtio-mmio register layout. Under virtio-pci (factory now advertises
     * VFF_VDEV_PCI) shared-memory regions are discovered via a
     * VIRTIO_PCI_CAP_SHARED_MEMORY_CFG capability instead, so this should be inert
     * there — verify nothing in vio_ctrl_pci_bars()'s BAR layout aliases 0xb0/0xb4. */
    unsigned reg = (unsigned)(vopnd->location & 0xfffU);
    if (reg == 0x0b0 || reg == 0x0b4) {          /* VIRTIO_MMIO_SHM_LEN_LOW/HIGH */
        uint32_t ones = 0xffffffffU;
        int wr = guest_cpu_write(gcp, GXF_NONE, oopnd, 1, &ones, vopnd->length);
        pthread_mutex_unlock(&vdp->v_mtx);
        return (wr != EOK) ? guest_cpu_to_vrs(wr) : VRS_NORMAL;
    }

    switch (res & VIOREF_CLASS_MASK) {
    case VIOREF_CLASS_DEVCFG: {
        unsigned off = res & ~VIOREF_CLASS_MASK;
        int r = guest_cpu_write(gcp, GXF_NONE, oopnd, 1, (uint8_t *)&s->cfg + off, vopnd->length);
        ret = (r != EOK) ? guest_cpu_to_vrs(r) : VRS_NORMAL;
        break;
    }
    case VIOREF_CLASS_BUS_ERROR: ret = VRS_BUS_ERROR; break;
    case VIOREF_CLASS_ERRNO:     ret = VRS_ERRNO + (res & ~VIOREF_CLASS_MASK); break;
    default: break;
    }
    pthread_mutex_unlock(&vdp->v_mtx);
    return ret;
}

static vdev_ref_status_t
gpu_vwrite(vdev_t *vdp, unsigned cookie, const struct qvm_state_block *vopnd,
           const struct qvm_state_block *oopnd, struct guest_cpu *gcp)
{
    struct gpu_state *s = vdp->v_device;
    vdev_ref_status_t ret = VRS_NORMAL;
    pthread_mutex_lock(&vdp->v_mtx);
    unsigned res = vio_write(&s->vs, cookie, vopnd, oopnd, gcp);
    unsigned op = res & ~VIOREF_CLASS_MASK;
    switch (res & VIOREF_CLASS_MASK) {
    case VIOREF_CLASS_RESET:
        /* Quiesce the render thread before touching shared virtio state: it may be
         * mid-process_ctrlq on the same vq. We hold v_mtx across the wait, which is
         * fine — the render thread never takes v_mtx (no deadlock) and this serializes
         * any other vCPU's MMIO against the half-reset device. Worst case is one
         * in-flight command (2s fence cap + pacing), which a rebooting guest tolerates. */
        pthread_mutex_lock(&s->render_mtx);
        s->quiescing = 1;
        pthread_cond_signal(&s->work_cv);
        while (s->in_ctrlq)
            pthread_cond_wait(&s->idle_cv, &s->render_mtx);
        vio_reset(&s->vs);
        s->scanout_resource = 0;
        s->fence_done = 0;                    /* stale high value = post-reboot fences retire early */
        s->quiescing = 0;
        pthread_mutex_unlock(&s->render_mtx);
        break;
    case VIOREF_CLASS_NOTIFY:
        if (op == VIRTIO_GPU_CONTROLQ) render_kick(s);       /* hand off to render thread */
        else if (op == VIRTIO_GPU_CURSORQ) drain_cursorq(s); /* no-op drain, unblock guest */
        break;
    case VIOREF_CLASS_STATUS:
        if (op & VIRTIO_DS_DRIVER_OK) vio_setup_queues(&s->vs);
        break;
    case VIOREF_CLASS_BUS_ERROR: ret = VRS_BUS_ERROR; break;
    case VIOREF_CLASS_ERRNO:     ret = VRS_ERRNO + op; break;
    default: break;
    }
    pthread_mutex_unlock(&vdp->v_mtx);
    return ret;
}

/* Process-global: qvm loads every vdev instance into ONE process, so the render
 * thread's static command/response buffers and present-pacing statics are shared.
 * We support exactly one virtio-gpu per guest; the guard below fails a 2nd loudly
 * rather than letting two render threads corrupt the same buffers. */
static int gpu_instances;

static int
gpu_control(vdev_t *vdp, vdev_ctrl_options_t ctrl, const char *arg)
{
    struct gpu_state *s = vdp->v_device;

    switch (ctrl) {
    case VDEV_CTRL_OPTIONS_START:
        if (gpu_instances) {
            vdev_logf(QL_QVM_ERR, vdp, "virtio-gpu: only one instance supported per qvm "
                                       "(process-global render buffers)");
            return EBUSY;
        }
        gpu_instances = 1;
        vdp->v_block.qst_type = QST_MEMORY;
        vdp->v_block.location = QSL_NO_LOCATION;
        s->ver = VIOVER_100;
        s->disp_w = 0;                     /* 0 = auto: adopt the live Screen mode */
        s->disp_h = 0;                     /* (scanout-width/height options override) */
        s->disp_hz = 0;                    /* filled from Screen on the render thread */
        s->cfg.num_scanouts = 1;
        s->cfg.num_capsets = 2;            /* VIRGL + VIRGL2 */
        s->desc = (struct vio_description){
            .device_type = VIRTIO_DT_GPU,
            .pci_class = PCI_CLASS_DISPLAY | PCI_SUBCLASS_DISPLAY_OTHER,
            .num_queues = 2,
            .device_config_size = sizeof(struct virtio_gpu_config),
            .device_features = {
                /* F_EDID: GET_EDID now synthesizes an EDID from the live Screen
                 * mode, so the guest learns the panel's real resolution AND refresh
                 * (the only virtio-gpu channel that carries refresh) — a >60 Hz
                 * screen works with no vdev change. F_VIRGL + F_EDID share word 0. */
                VIO_F_BIT(VIRTIO_GPU_F_VIRGL) | VIO_F_BIT(VIRTIO_GPU_F_EDID),
                VIO_F_BIT(VIRTIO_F_VERSION_1),
                0, 0
            },
        };
        break;

    case VDEV_CTRL_FIRST_OPTIDX + OPT_WIDTH_IDX:
        if (arg) s->disp_w = (uint32_t)strtoul(arg, NULL, 0);
        break;
    case VDEV_CTRL_FIRST_OPTIDX + OPT_HEIGHT_IDX:
        if (arg) s->disp_h = (uint32_t)strtoul(arg, NULL, 0);
        break;
    case VDEV_CTRL_FIRST_OPTIDX + OPT_DISPLAY_IDX:
        if (arg) s->scanout_display = (int)strtol(arg, NULL, 0);
        break;

    case VDEV_CTRL_OPTIONS_END: {
        vio_init(vdp, s->ver, &s->desc, &s->vs);
        int r = vio_init_queue(&s->vs, VIRTIO_GPU_CONTROLQ, GPU_CTRLQ_SIZE);
        if (r != EOK) return r;
        r = vio_init_queue(&s->vs, VIRTIO_GPU_CURSORQ, GPU_CURSORQ_SIZE);
        if (r != EOK) return r;
        pthread_mutex_init(&s->render_mtx, NULL);
        pthread_cond_init(&s->work_cv, NULL);
        pthread_cond_init(&s->idle_cv, NULL);
        s->running = 1;
        if (pthread_create(&s->render_thr, NULL, render_thread, vdp) != 0) {
            s->running = 0;            /* so TERMINATE won't join a garbage tid */
            vdev_logf(QL_QVM_ERR, vdp, "virtio-gpu: render thread create failed");
            return EAGAIN;
        }
        if (s->disp_w)
            fprintf(stderr, "virtio-gpu vdev: init (%ux%u override, virgl 3D)\n",
                    s->disp_w, s->disp_h);
        else
            fprintf(stderr, "virtio-gpu vdev: init (auto mode from Screen, virgl 3D)\n");
        break;
    }

    case VDEV_CTRL_TERMINATE:
        if (s->running) {
            pthread_mutex_lock(&s->render_mtx);
            s->running = 0;
            pthread_cond_signal(&s->work_cv);
            pthread_mutex_unlock(&s->render_mtx);
            pthread_join(s->render_thr, NULL);
        }
        gpu_instances = 0;             /* allow a fresh instance after teardown */
        break;

    case VDEV_CTRL_GEN_FDT:
        vio_ctrl_gen_fdt(&s->vs, "virtio_gpu", (int)(uintptr_t)arg);
        break;
    case VDEV_CTRL_PCI_BARS:
        return vio_ctrl_pci_bars(&s->vs, arg);
    default:
        break;
    }
    return EOK;
}

static const char *const gpu_options[OPT_NUM_OPTS + 1] = {
    [OPT_WIDTH_IDX]   = "scanout-width",
    [OPT_HEIGHT_IDX]  = "scanout-height",
    [OPT_DISPLAY_IDX] = "scanout-display",   /* 1-based Screen display; default last */
    [OPT_NUM_OPTS]    = NULL,
};

static struct vdev_factory gpu_factory = {
    .control = gpu_control,
    .vread = gpu_vread,
    .vwrite = gpu_vwrite,
    .option_list = gpu_options,
    .name = "virtio-gpu",
    .factory_flags = VFF_INTR_NONPCI | VFF_VDEV_MEM | VFF_VDEV_PCI | VFF_MUTEX | VFF_MUTEX_RECURSIVE,
    .acc_sizes = (1u << 1) | (1u << 2) | (1u << 4) | (1u << 8),
    .extra_space = sizeof(struct gpu_state),
};

__attribute__((constructor))
static void vdev_virtio_gpu_init(void)
{
    vdev_register_factory(&gpu_factory, QVM_VDEV_ABI);
}
