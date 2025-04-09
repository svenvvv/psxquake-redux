#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <psxgpu.h>

#define PSX_MAX_VRAM_RECTS 2048

#define VID_WIDTH 320 // can't lower it without menus breaking
#define VID_HEIGHT 240

#define VRAM_WIDTH 1024
#define VRAM_HEIGHT 512
#define VRAM_PAGE_WIDTH 128
#define VRAM_PAGE_HEIGHT 256

// 4 pages less than actual since the framebuffer covers the entire four pages, so we can't
// allocate textures on them anyways
#define VRAM_PAGES (((VRAM_WIDTH / VRAM_PAGE_WIDTH) * (VRAM_HEIGHT / VRAM_PAGE_HEIGHT)) - 4)

#define PRIBUF_LEN (10 * 1024U)

#define PSX_MENU_ZLEVEL_END (MENU_OT_LEN - 1)

#include <psxgte.h>
#include <psxgpu.h>

/**
 * Menu ordering table layers, from back-to-front.
 */
enum psx_menu_zlevel_layers {
    PSX_MENU_ZLEVEL_BACKGROUND = 0,
    PSX_MENU_ZLEVEL_IMAGES,
    PSX_MENU_ZLEVEL_MENU,
    PSX_MENU_ZLEVEL_TEXT,
    PSX_MENU_ZLEVEL_ENUM_LAST,
};

#define OT_LEN (5 * 1024U)
#define MENU_OT_LEN (PSX_MENU_ZLEVEL_ENUM_LAST)

typedef struct DISPENV_CACHE_S {
    uint32_t fb_pos;
    uint32_t h_range;
    uint32_t v_range;
    uint32_t mode;
} DISPENV_CACHE;

struct PQRenderBuf {
    DISPENV disp;
    DISPENV_CACHE disp_cache;
    DRAWENV draw;
    uint32_t ot[OT_LEN];
    uint32_t menu_ot[MENU_OT_LEN];
    uint8_t pribuf[PRIBUF_LEN];
};

typedef struct vram_texpage_s {
    /** Page X-coordinate */
    int16_t x;
    /** Page Y-coordinate */
    int16_t y;
    /** Textures allocated on this page */
    struct psx_vram_texture_s *textures[PSX_MAX_VRAM_RECTS / VRAM_PAGES];
    /** Number of textures allocated on this page */
    size_t textures_count;
    /** Unused rectangles on this page */
    RECT available_rects[PSX_MAX_VRAM_RECTS / VRAM_PAGES];
    /** Number of unused rectangles on this page */
    size_t available_rects_count;
} psx_vram_texture_page;

typedef struct psx_vram_texture_s {
    /** Unique identifier of the texture */
    uint32_t ident;
    /** Texture index. TODO remove? */
    uint16_t index;
    /** Texture rectangle in VRAM */
    RECT rect;
    /** Parent texture page */
    struct vram_texpage_s *page;
    /** Scale of the output texture in relation to the VRAM texture (we downscale) */
    int scale;
    /** Whether the texture contains transparent pixels */
    bool is_alpha;
    /** PSX texture page value */
    uint16_t tpage;
} psx_vram_texture;

extern uint8_t *rb_nextpri;

extern uint16_t psx_clut;
extern uint16_t psx_clut_transparent;
extern unsigned psx_zlevel;
extern unsigned psx_menu_zlevel;
extern int psx_db;

void psx_vram_init(void);
psx_vram_texture *psx_vram_get(int index);
psx_vram_texture *psx_vram_pack(char const *ident, int w, int h);
psx_vram_texture *psx_vram_find(char const *ident, int w, int h);
void psx_vram_rect(int x, int y, int w, int h);

void psx_rb_init(void);
void psx_rb_present(void);

// void psx_add_prim(void * prim, int z);

// static inline void psx_add_prim(void * prim, int z)
// {
//     extern int pricount;
//     extern struct PQRenderBuf rb[2];
//
// 	pricount += 1;
// 	if (z < 0) {
// 		z += OT_LEN;
// 	}
// 	addPrim(rb[psx_db].ot + (OT_LEN - z - 1), prim);
// 	// rb_nextpri = prim;
// }

void psx_add_prim_internal(uint32_t * ot, int ot_len, uint32_t * prim, int prim_len, size_t z);
void psx_add_prim_internal_r(uint32_t * ot, int ot_len, uint32_t * prim, int prim_len, size_t z);

extern struct PQRenderBuf rb[2];

#define psx_add_prim(prim, z) psx_add_prim_internal(rb[psx_db].ot, OT_LEN, (uint32_t *)prim, sizeof(*prim), z)
#define menu_add_prim_z(prim, z) \
    psx_add_prim_internal(rb[psx_db].menu_ot, MENU_OT_LEN, (uint32_t *)prim, sizeof(*prim), z)
#define psx_add_prim_z(prim, z)                       \
    psx_add_prim(prim, z)

#if 0
#define psx_add_prim_z(prim, z)                       \
    do {                                              \
        extern int pricount;                          \
        extern struct PQRenderBuf rb[2];              \
        pricount += 1;                                \
        addPrim(rb[psx_db].ot + z, prim);             \
        rb_nextpri = (uint8_t *)prim + sizeof(*prim); \
    } while (0);
#endif

__attribute__((__always_inline__))
static inline uint16_t psx_rgb16(uint8_t r, uint8_t g, uint8_t b, uint8_t stp)
{
    uint16_t p = (stp & 1) << 15;

    p |= (r >> 3) << 0;
    p |= (g >> 3) << 5;
    p |= (b >> 3) << 10;

    return p;
}

__attribute__((__always_inline__))
static inline uint32_t psx_rgb24(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t ret = 0;

    ret |= r << 0;
    ret |= g << 16;
    ret |= b << 24;

    return ret;
}

#define PSX_SCRATCH_SIZE 1024;
static uint8_t *psx_scratch = (uint8_t *)0x1F800000;

void draw_quad_tex_subdiv(SVECTOR const verts[4], SVECTOR const *normal, uint8_t const uv[4 * 2],
                         psx_vram_texture const *tex, int subdiv);

void draw_tri_tex_subdiv(SVECTOR const verts[3], SVECTOR const *normal, uint8_t const uv[3 * 2],
                         psx_vram_texture const *tex, int subdiv);

void draw_tri(SVECTOR const verts[3], CVECTOR const *color);
void draw_tri_tex(SVECTOR const verts[3], SVECTOR const *normal, uint8_t const uv[3 * 2],
                  psx_vram_texture const *tex);
void draw_quad(SVECTOR const verts[4], CVECTOR const *color);
int draw_quad_tex(SVECTOR const verts[4], SVECTOR const *normal, uint8_t const uv[4 * 2],
                  psx_vram_texture const *tex);
