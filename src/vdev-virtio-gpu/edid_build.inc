/*
 * build_edid — synthesize a 128-byte EDID 1.3 base block for a w x h @ hz mode.
 *
 * Pure (stdint + string.h only) so it is unit-testable on the host:
 *     cc -DEDID_SELFTEST edid_selftest.c -o /tmp/edid_selftest && /tmp/edid_selftest
 *
 * The guest's drm/virtio driver runs drm_add_edid_modes() on this and creates a
 * mode whose refresh = pixel_clock / (htotal * vtotal). We build ONE Detailed
 * Timing Descriptor with self-consistent totals + pixel clock, so the guest
 * derives exactly `hz`. Blanking porches are generic (CVT-RB-ish); they only
 * feed the CRTC timing, which is moot here — scanout is a CPU readback, not a
 * real display pipe. A monitor-range descriptor widens the accepted vfreq so
 * >60 Hz modes aren't clamped.
 */
#include <stdint.h>
#include <string.h>

/* generic fixed porches — see header comment */
#define EDID_HBLANK 160u   /* hfront 48 + hsync 32 + hback 80 */
#define EDID_HFRONT 48u
#define EDID_HSYNC  32u
#define EDID_VBLANK 35u    /* vfront 3 + vsync 6 + vback 26 */
#define EDID_VFRONT 3u
#define EDID_VSYNC  6u

static void build_edid_dtd(uint8_t d[18], uint32_t w, uint32_t h, uint32_t hz)
{
    uint32_t htotal = w + EDID_HBLANK;
    uint32_t vtotal = h + EDID_VBLANK;
    /* DTD pixel clock is in 10 kHz units, 16-bit.
     * ponytail: caps at 655.35 MHz (e.g. blocks 4K@120); clamp, revisit if a
     * panel ever needs more than the DTD field can hold. */
    uint64_t pclk_10khz = ((uint64_t)htotal * vtotal * hz) / 10000u;
    if (pclk_10khz < 1) pclk_10khz = 1;
    if (pclk_10khz > 0xffff) pclk_10khz = 0xffff;

    memset(d, 0, 18);
    d[0] = (uint8_t)(pclk_10khz & 0xff);
    d[1] = (uint8_t)((pclk_10khz >> 8) & 0xff);
    d[2] = (uint8_t)(w & 0xff);
    d[3] = (uint8_t)(EDID_HBLANK & 0xff);
    d[4] = (uint8_t)(((w >> 8) & 0x0f) << 4 | ((EDID_HBLANK >> 8) & 0x0f));
    d[5] = (uint8_t)(h & 0xff);
    d[6] = (uint8_t)(EDID_VBLANK & 0xff);
    d[7] = (uint8_t)(((h >> 8) & 0x0f) << 4 | ((EDID_VBLANK >> 8) & 0x0f));
    d[8] = (uint8_t)(EDID_HFRONT & 0xff);
    d[9] = (uint8_t)(EDID_HSYNC & 0xff);
    d[10] = (uint8_t)(((EDID_VFRONT & 0x0f) << 4) | (EDID_VSYNC & 0x0f));
    d[11] = 0;                 /* all porch high bits are zero for these sizes */
    /* d[12..16] image size / borders = 0 */
    d[17] = 0x1e;             /* digital separate sync, H+ V+ */
}

/* An 18-byte "display descriptor": tag byte + up to 13 payload bytes. */
static void build_edid_desc(uint8_t d[18], uint8_t tag,
                            const uint8_t *payload, unsigned plen)
{
    memset(d, 0, 18);
    d[3] = tag;               /* d[0,1]=0 (not a DTD), d[2]=0, d[4]=0 */
    for (unsigned i = 0; i < plen && i < 13; ++i)
        d[5 + i] = payload[i];
}

static void build_edid(uint8_t e[128], uint32_t w, uint32_t h, uint32_t hz)
{
    if (!w) w = 1920;
    if (!h) h = 1080;
    if (!hz) hz = 60;

    memset(e, 0, 128);

    /* header */
    static const uint8_t hdr[8] = {0,0xff,0xff,0xff,0xff,0xff,0xff,0};
    memcpy(e, hdr, 8);

    /* manufacturer "QVM" (5-bit packed, A=1), product 0x0001, no serial */
    uint16_t mfg = (uint16_t)((('Q'-'@') << 10) | (('V'-'@') << 5) | ('M'-'@'));
    e[8] = (uint8_t)(mfg >> 8);
    e[9] = (uint8_t)(mfg & 0xff);
    e[10] = 0x01; e[11] = 0x00;         /* product code */
    /* e[12..15] serial = 0 */
    e[16] = 0;                          /* week */
    e[17] = (uint8_t)(2026 - 1990);     /* year */
    e[18] = 1;                          /* EDID version 1 */
    e[19] = 3;                          /* revision 3 (EDID 1.3) */

    e[20] = 0x80;                       /* digital input */
    e[21] = 0; e[22] = 0;               /* screen size cm: undefined */
    e[23] = 0x78;                       /* gamma 2.2 */
    e[24] = 0x02;                       /* feature: preferred timing = native */

    /* e[25..34] chromaticity: generic sRGB-ish; drm doesn't validate strictly */
    static const uint8_t chroma[10] =
        {0xee,0x91,0xa3,0x54,0x4c,0x99,0x26,0x0f,0x50,0x54};
    memcpy(e + 25, chroma, 10);

    /* e[35..37] established timings = none (we drive via the DTD) */
    /* e[38..53] standard timings = unused (0x01 0x01 x8) */
    for (unsigned i = 38; i < 54; ++i) e[i] = 0x01;

    /* descriptor 1 (54): the real mode */
    build_edid_dtd(e + 54, w, h, hz);

    /* descriptor 2 (72): monitor range limits — widen vfreq to cover >60 Hz */
    {
        uint8_t rng[13];
        memset(rng, 0, sizeof(rng));
        rng[0] = 0;                     /* offsets flag */
        rng[1] = 1;                     /* min vertical Hz */
        rng[2] = (uint8_t)(hz > 240 ? 255 : (hz < 60 ? 60 : hz) + 15); /* max vHz */
        rng[3] = 1;                     /* min horizontal kHz */
        rng[4] = 255;                   /* max horizontal kHz */
        rng[5] = 80;                    /* max pixel clock / 10 MHz = 800 MHz */
        rng[6] = 0x0a;                  /* no secondary timing (bare limits) */
        rng[7] = rng[8] = rng[9] = rng[10] = rng[11] = rng[12] = 0x20;
        build_edid_desc(e + 72, 0xfd, rng, 13);
    }

    /* descriptor 3 (90): monitor name */
    {
        const char *name = "QNX vGPU";
        uint8_t nm[13];
        unsigned i = 0;
        for (; name[i] && i < 13; ++i) nm[i] = (uint8_t)name[i];
        if (i < 13) nm[i++] = 0x0a;     /* terminator */
        for (; i < 13; ++i) nm[i] = 0x20;
        build_edid_desc(e + 90, 0xfc, nm, 13);
    }

    /* descriptor 4 (108): dummy (0x10) */
    build_edid_desc(e + 108, 0x10, NULL, 0);

    e[126] = 0;                         /* extension block count */

    /* checksum: whole block must sum to 0 mod 256 */
    unsigned sum = 0;
    for (unsigned i = 0; i < 127; ++i) sum += e[i];
    e[127] = (uint8_t)((256u - (sum & 0xff)) & 0xff);
}
