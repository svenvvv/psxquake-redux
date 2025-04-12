#include <stdio.h>

#include "psx/gl.h"
#include "psx/io.h"
#include "sys.h"

int psx_db = 0;
struct PQRenderBuf rb[2];

uint8_t *rb_nextpri;
unsigned psx_zlevel;
unsigned psx_menu_zlevel;

// Ripped out from PSn00bSDK, lets save some easy cycles.
// We never change the dispenv during runtime.
static void PsxQuakeCalcDispEnv(DISPENV const *env, DISPENV_CACHE *cache)
{
    cache->mode = GetVideoMode() << 3;
    cache->mode |= (env->isrgb24 & 1) << 4;
    cache->mode |= (env->isinter & 1) << 5;
    cache->mode |= (env->reverse & 1) << 7;

    int h_span = env->screen.w ? env->screen.w : 256;
    int v_span = env->screen.h ? env->screen.h : 240;

    // Calculate the horizontal and vertical display range values.
    h_span *= 10;

    if (env->disp.w > 560) {
        // 640 pixels
        cache->mode |= 3 << 0;
        //h_span *= 4;
    } else if (env->disp.w > 400) {
        // 512 pixels
        cache->mode |= 2 << 0;
        //h_span *= 5;
    } else if (env->disp.w > 352) {
        // 368 pixels
        cache->mode |= 1 << 6;
        //h_span *= 7;
    } else if (env->disp.w > 280) {
        // 320 pixels
        cache->mode |= 1 << 0;
        //h_span *= 8;
    } else {
        // 256 pixels
        cache->mode |= 0 << 0;
        //h_span *= 10;
    }

    if (env->disp.h > 256) {
        cache->mode |= 1 << 2;
        //v_span /= 2;
    }

    int x = env->screen.x + 0x760;
    int y = env->screen.y + (GetVideoMode() ? 0xa3 : 0x88);
    h_span /= 2;
    v_span /= 2;

    cache->fb_pos = (env->disp.x & 0x3ff);
    cache->fb_pos |= (env->disp.y & 0x1ff) << 10;
    cache->h_range = ((x - h_span) & 0xfff);
    cache->h_range |= ((x + h_span) & 0xfff) << 12;
    cache->v_range = ((y - v_span) & 0x3ff);
    cache->v_range |= ((y + v_span) & 0x3ff) << 10;
}

static void PqPutDispEnv(DISPENV_CACHE *cache)
{
    GPU_GP1 = 0x05000000 | cache->fb_pos; // Set VRAM location to display
    GPU_GP1 = 0x06000000 | cache->h_range; // Set horizontal display range
    GPU_GP1 = 0x07000000 | cache->v_range; // Set vertical display range
    GPU_GP1 = 0x08000000 | cache->mode; // Set video mode
}

static void renderbuf_init(struct PQRenderBuf *buf)
{
#ifdef GLQUAKE
#ifdef DEBUG
    setRGB0(&buf->draw, 63, 0, 127);
#else
    setRGB0(&buf->draw, 0, 0, 0);
#endif
#else
    // Set bg to black so we don't have to loop through each pixel of each frame replacing black
    // pixels (transparent) with dark red.
    setRGB0(&buf->draw, 0, 0, 0);
#endif
    buf->draw.isbg = 1;
    buf->draw.dtd = 1;

    ClearOTagR(buf->ot, ARRAY_SIZE(buf->ot));
    ClearOTagR(buf->menu_ot, ARRAY_SIZE(buf->menu_ot));

    // Put env's to build the cached packets (at least for drawenv)
    PsxQuakeCalcDispEnv(&buf->disp, &buf->disp_cache);
    PutDrawEnv(&buf->draw);
}

void psx_rb_init(void)
{
    ResetGraph(0);

    SetDefDispEnv(&rb[1].disp, 0, VID_HEIGHT, VID_WIDTH, VID_HEIGHT);
    SetDefDrawEnv(&rb[1].draw, 0, 0, VID_WIDTH, VID_HEIGHT);
    renderbuf_init(&rb[1]);

    SetDefDispEnv(&rb[0].disp, 0, 0, VID_WIDTH, VID_HEIGHT);
    SetDefDrawEnv(&rb[0].draw, 0, VID_HEIGHT, VID_WIDTH, VID_HEIGHT);
    renderbuf_init(&rb[0]);

    rb_nextpri = rb[0].pribuf;
}

void psx_rb_present(void)
{
    struct PQRenderBuf *cur_rb;

#ifdef PSXQUAKE_PARANOID
    if (rb_nextpri >= rb[psx_db].pribuf + sizeof(rb->pribuf)) {
        Sys_Error("Prim buf overflow, %d\n", rb_nextpri - (rb[psx_db].pribuf + sizeof(rb->pribuf)));
    }

    // ptrdiff_t pri_diff = rb_nextpri - rb[psx_db].pribuf + sizeof(rb->pribuf);
    // printf("pribuf len %i\n", pri_diff);

    if (psx_zlevel >= OT_LEN) {
        Sys_Error("psx_zlevel out of bounds, %u\n", psx_zlevel);
    }
    if (psx_menu_zlevel >= MENU_OT_LEN) {
        Sys_Error("psx_menu_zlevel out of bounds, %u\n", psx_menu_zlevel);
    }
#endif

#ifdef PSXQUAKE_PARANOID
    uint32_t pre_sync = Sys_CurrentTicks();
#endif

    DrawSync(0);

#ifdef PSXQUAKE_PARANOID
    int sync_diff = Sys_CurrentTicks() - pre_sync;
    if (sync_diff > 0) {
        printf("GPU bottlenecking, DrawSync took %i\n", sync_diff);
    }
#endif

    VSync(0);

    psx_db = !psx_db;
    cur_rb = &rb[psx_db];

    rb_nextpri = cur_rb->pribuf;
    psx_zlevel = 0;
    psx_menu_zlevel = 0;

    ClearOTagR(cur_rb->ot, ARRAY_SIZE(rb->ot));
    ClearOTagR(cur_rb->menu_ot, ARRAY_SIZE(rb->menu_ot));

    DrawOTag((const uint32_t *)&cur_rb->draw.dr_env);
    PqPutDispEnv(&cur_rb->disp_cache);

    SetDispMask(1);

    DrawOTag(rb[1 - psx_db].ot + (ARRAY_SIZE(rb->ot) - 1));
    DrawOTag(rb[1 - psx_db].menu_ot + (ARRAY_SIZE(rb->menu_ot) - 1));
}

void psx_add_prim_internal(uint32_t * ot, int ot_len, uint32_t * prim, int prim_len, size_t z)
{
    if (z > ot_len) {
        z = z % ot_len;
    }
    addPrim(ot + (ot_len - z - 1), prim);
    rb_nextpri = (uint8_t *)prim + prim_len;
}

void psx_add_prim_internal_r(uint32_t * ot, int ot_len, uint32_t * prim, int prim_len, size_t z)
{
    if (z > ot_len) {
        z = z % ot_len;
    }
    addPrim(ot + (ot_len + z), prim);
    rb_nextpri = (uint8_t *)prim + prim_len;
}
