/* Host self-check for build_edid. Not built into the vdev.
 *   cc -DEDID_SELFTEST edid_selftest.c -o /tmp/edid_selftest && /tmp/edid_selftest
 */
#include <assert.h>
#include <stdio.h>
#include "edid_build.inc"

/* recover the DTD's derived refresh, the way the guest's EDID parser does */
static unsigned dtd_refresh(const uint8_t e[128])
{
    const uint8_t *d = e + 54;
    unsigned pclk = (d[0] | (d[1] << 8)) * 10000u;          /* Hz */
    unsigned w = d[2] | ((d[4] & 0xf0) << 4);
    unsigned hbl = d[3] | ((d[4] & 0x0f) << 8);
    unsigned h = d[5] | ((d[7] & 0xf0) << 4);
    unsigned vbl = d[6] | ((d[7] & 0x0f) << 8);
    unsigned htotal = w + hbl, vtotal = h + vbl;
    return (pclk + (htotal * vtotal) / 2) / (htotal * vtotal);
}

static void check(uint32_t w, uint32_t h, uint32_t hz)
{
    uint8_t e[128];
    build_edid(e, w, h, hz);

    /* header */
    assert(e[0] == 0 && e[1] == 0xff && e[6] == 0xff && e[7] == 0);
    /* whole block sums to 0 mod 256 */
    unsigned sum = 0;
    for (int i = 0; i < 128; ++i) sum += e[i];
    assert((sum & 0xff) == 0);
    /* DTD carries the requested resolution */
    unsigned dw = e[56] | ((e[58] & 0xf0) << 4);
    unsigned dh = e[59] | ((e[61] & 0xf0) << 4);
    assert(dw == w && dh == h);
    /* and round-trips to the requested refresh (±1 Hz from integer rounding) */
    unsigned r = dtd_refresh(e);
    assert(r + 1 >= hz && hz + 1 >= r);
    printf("ok  %ux%u@%u  -> DTD %ux%u@%uHz  csum=0\n", w, h, hz, dw, dh, r);
}

int main(void)
{
    check(1024, 600, 60);
    check(1920, 1080, 60);
    check(1920, 1080, 120);
    check(2560, 1440, 144);
    check(3840, 2160, 60);
    check(1920, 1080, 60);     /* what build_edid(0,0,0) must fall back to */
    { uint8_t a[128], b[128]; build_edid(a,0,0,0); build_edid(b,1920,1080,60);
      assert(memcmp(a,b,128) == 0); }   /* 0,0,0 defaults to 1920x1080@60 */
    puts("all edid self-checks passed");
    return 0;
}
