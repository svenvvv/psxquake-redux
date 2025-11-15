/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "quakedef.h"
#include "psx/gl.h"
#include "util/hashlib.h"

CVAR_REGISTER(gl_max_size, CVAR_CTOR({ "gl_max_size", 1024 }));

static byte *draw_chars; // 8*8 graphic characters
static qpic_t *draw_backtile;
qpic_t *draw_disc;

typedef struct {
    int texnum;
} glpic_t;
static_assert(sizeof((qpic_t *)0)->data == sizeof(glpic_t));

static byte conback_buffer[sizeof(qpic_t)];
static qpic_t *conback = (qpic_t *)&conback_buffer;

static psx_vram_texture const *char_texture;

int gl_lightmap_format = 4;
int gl_solid_format = 3;
int gl_alpha_format = 4;

void GL_Bind(int texnum)
{
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s {
    uint32_t name_hash;
    qpic_t pic;
} cachepic_t;

#define MAX_CACHED_PICS 128
static cachepic_t menu_cachepics[MAX_CACHED_PICS];
static int menu_numcachepics;

static byte menuplyr_pixels[4096];

qpic_t *Draw_PicFromWad(char const *name)
{
    qpic_t *p;
    glpic_t *gl;

    p = W_GetLumpName(name);
    gl = (glpic_t *)p->data;
    gl->texnum = GL_LoadPicTexture(name, p);
    return p;
}

/*
================
Draw_CachePic
================
*/
qpic_t *Draw_CachePic(char const *path)
{
    cachepic_t *pic;
    int i;
    qpic_t *dat;
    glpic_t *gl;
    uint32_t name_hash = pq_hash(path, strlen(path));

    for (pic = menu_cachepics, i = 0; i < menu_numcachepics; pic++, i++)
        if (pic->name_hash == name_hash)
            return &pic->pic;

    if (menu_numcachepics == MAX_CACHED_PICS)
        Sys_Error("menu_numcachepics == MAX_CACHED_PICS");
    menu_numcachepics++;
    pic->name_hash = name_hash;

    //
    // load the pic from disk
    //
    dat = (qpic_t *)COM_LoadTempFile(path);
    if (!dat)
        Sys_Error("Draw_CachePic: failed to load %s", path);
    SwapPic(dat);

    // HACK HACK HACK --- we need to keep the bytes for
    // the translatable player picture just for the menu
    // configuration dialog
    if (!strcmp(path, "gfx/menuplyr.lmp"))
        memcpy(menuplyr_pixels, dat->data, dat->width * dat->height);

    pic->pic.width = dat->width;
    pic->pic.height = dat->height;

    gl = (glpic_t *)pic->pic.data;
    gl->texnum = GL_LoadPicTexture(path, dat);

    return &pic->pic;
}

void Draw_CharToConback(int num, byte *dest)
{
    int row, col;
    byte *source;
    int drawline;
    int x;

    row = num >> 4;
    col = num & 15;
    source = draw_chars + (row << 10) + (col << 3);

    drawline = 8;

    while (drawline--) {
        for (x = 0; x < 8; x++)
            if (source[x] != 255)
                dest[x] = 0x60 + source[x];
        source += 128;
        dest += 320;
    }
}

/*
===============
Draw_Init
===============
*/
void Draw_Init(void)
{
    byte *dest;
    int x, y;
    char ver[40];
    int start;
    byte *ncdata;

    // load the console background and the charset
    // by hand, because we need to write the version
    // string into the background before turning
    // it into a texture
    draw_chars = W_GetLumpName("conchars");
    for (int i = 0; i < 256 * 64; i++) {
        if (draw_chars[i] == 0) {
            draw_chars[i] = 255; // proper transparent color
        }
    }

    // now turn them into textures
    char_texture = psx_LoadTexture("charset", 128, 128, draw_chars, false, true, true);

    start = Hunk_LowMark();

    qpic_t * cb = (qpic_t *)COM_LoadTempFile("gfx/conback.lmp");
    if (!cb) {
        Sys_Error("Couldn't load gfx/conback.lmp");
    }
    SwapPic(cb);

    // hack the version number directly into the pic
    y = sprintf(ver, "(PSXQuake-redux) %4u.%02u", VERSION_MAJOR, VERSION_MINOR);
    dest = cb->data + 320 * 186 + 320 - 11 - 8 * y;
    for (x = 0; x < y; x++)
        Draw_CharToConback(ver[x], dest + (x << 3));

    conback->width = cb->width;
    conback->height = cb->height;
    ncdata = cb->data;

    glpic_t * gl = (glpic_t *)conback->data;
    gl->texnum = GL_LoadTexture("conback", conback->width, conback->height, ncdata, false, false);
    conback->width = vid.width;
    conback->height = vid.height;

    // free loaded console
    Hunk_FreeToLowMark(start);

    //
    // get the other pics we need
    //
    draw_disc = Draw_PicFromWad("disc");
    draw_backtile = Draw_PicFromWad("backtile");
}

/**
 * Actual internal Draw_Character implementation.
 * This does not set the zlevel, you must set it before calling this function.
 */
static void psx_Draw_Character(int x, int y, int num)
{
    int row, col;

    // Skip space
    if (num == 32) {
        return;
    }
    // Totally off-screen
    if (y <= -8) {
        return;
    }

    row = num >> 4;
    col = num & 15;

    SPRT_8 *sprt = (SPRT_8 *)rb_nextpri;

    setSprt8(sprt);
    setXY0(sprt, x, y);
    setUV0(sprt, col * 8, row * 8);
    setRGB0(sprt, 128, 128, 128);
    sprt->clut = psx_clut_transparent;

    menu_add_prim_z(sprt, psx_menu_zlevel);

    DR_TPAGE *tp = (DR_TPAGE *)rb_nextpri;

    setDrawTPage(tp, 0, 1, char_texture->tpage);

    menu_add_prim_z(tp, psx_menu_zlevel);
}

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character(int x, int y, int num)
{
    psx_menu_zlevel = PSX_MENU_ZLEVEL_TEXT;
    psx_Draw_Character(x, y, num);
}

/*
================
Draw_String
================
*/
void Draw_String(int x, int y, char const *str)
{
    psx_menu_zlevel = PSX_MENU_ZLEVEL_TEXT;
    while (*str) {
        psx_Draw_Character(x, y, *str);
        str++;
        x += 8;
    }
}

void psx_Draw_Pic(int x, int y, qpic_t const *pic)
{
    glpic_t *gl = (glpic_t *)pic->data;
    psx_vram_texture const *tex = psx_vram_get(gl->texnum);
#ifdef PSXQUAKE_PARANOID
    if (tex == NULL) {
        Sys_Error("psx_Draw_Pic: null texture\n");
    }
#endif

    POLY_FT4 *poly = (POLY_FT4 *)rb_nextpri;

    setPolyFT4(poly);
    setXYWH(poly, x, y, pic->width, pic->height);
    setUVWH(poly, tex->rect.x * 2, tex->rect.y, tex->rect.w - 1, tex->rect.h);
    setRGB0(poly, 128, 128, 128);
    poly->clut = tex->is_alpha ? psx_clut_transparent : psx_clut;
    poly->tpage = tex->tpage;

    menu_add_prim_z(poly, psx_menu_zlevel);
}

void Draw_Pic(int x, int y, qpic_t const *pic)
{
    psx_menu_zlevel = PSX_MENU_ZLEVEL_IMAGES;
    psx_Draw_Pic(x, y, pic);
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic(int x, int y, qpic_t const *pic)
{
#ifdef PSXQUAKE_PARANOID
    if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 || (unsigned)(y + pic->height) > vid.height) {
        Sys_Error("Draw_TransPic: bad coordinates");
    }
#endif

    psx_menu_zlevel = PSX_MENU_ZLEVEL_MENU;
    psx_Draw_Pic(x, y, pic);
}

/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate(int x, int y, qpic_t const *pic, byte const *translation)
{
    int v, u;
    unsigned trans[64 * 64], *dest;
    byte *src;
    int p;

    // GL_Bind(translate_texture);

    dest = trans;
    for (v = 0; v < 64; v++, dest += 64) {
        src = &menuplyr_pixels[((v * pic->height) >> 6) * pic->width];
        for (u = 0; u < 64; u++) {
            p = src[(u * pic->width) >> 6];
            if (p == 255)
                dest[u] = p;
            else
                dest[u] = d_8to24table[translation[p]];
        }
    }

    psx_Draw_Pic(x, y, pic);

#if 0
    glTexImage2D(GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(x, y);
    glTexCoord2f(1, 0);
    glVertex2f(x + pic->width, y);
    glTexCoord2f(1, 1);
    glVertex2f(x + pic->width, y + pic->height);
    glTexCoord2f(0, 1);
    glVertex2f(x, y + pic->height);
    glEnd();
#endif
}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground(int lines)
{
    psx_menu_zlevel = PSX_MENU_ZLEVEL_BACKGROUND;
    psx_Draw_Pic(0, lines - vid.height, conback);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear(int x, int y, int w, int h)
{
    glpic_t *gl = (glpic_t *)draw_backtile->data;
    psx_vram_texture const *tex = psx_vram_get(gl->texnum);
#ifdef PSXQUAKE_PARANOID
    if (tex == NULL) {
        Sys_Error("Draw_TileClear: null tex");
    }
    if (tex->rect.x % 8 != 0 || tex->rect.y % 8 != 0) {
        // TODO: currently it's up to chance whether the texture is correctly aligned.
        // Should add support to VRAM packer to force alignment.
        Sys_Error("Draw_TileClear: texture not aligned on grid");
    }
#endif
    psx_menu_zlevel = PSX_MENU_ZLEVEL_IMAGES;

    RECT twin = { 0 };

    // Clear texture window
    DR_TWIN * ptwin = (DR_TWIN*) rb_nextpri;
    setTexWindow(ptwin, &twin);
    menu_add_prim_z(ptwin, psx_menu_zlevel);

    // Polygon
    POLY_FT4 *poly = (POLY_FT4 *) rb_nextpri;

    setPolyFT4(poly);
    setXYWH(poly, x, y, w, h);

    if (w > UINT8_MAX) {
        w = UINT8_MAX;
    }
    if (h > UINT8_MAX) {
        h = UINT8_MAX;
    }

    setUVWH(poly, 0, 0, w, h);
    setRGB0(poly, 128, 128, 128);
    poly->clut = psx_clut;
    poly->tpage = tex->tpage;

    menu_add_prim_z(poly, psx_menu_zlevel);

    // Set texture window
    twin = tex->rect;
    twin.x *= 2;
    twin.x >>= 3;
    twin.y >>= 3;
    // Texture window width/height are weird values
    // According to PSn00bSDK samples they should be normal pixel dimensions divided by 3, but that doesn't work?
    // According to https://psx-spx.consoledev.net/graphicsprocessingunitgpu/ it should be 0b11000 for 64, which does
    // work?? 8: 0b11111, 16: 0b11110, 32: 0b11100, 64: 0b11000, 128: 0b10000, 256: 0b00000
    twin.w = 28;
    twin.h = 24;
    ptwin = (DR_TWIN*) rb_nextpri;
    setTexWindow(ptwin, &twin);
    menu_add_prim_z(ptwin, psx_menu_zlevel);
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill(int x, int y, int w, int h, int c)
{
    FILL *fill = (FILL *)rb_nextpri;

    setFill(fill);
    setXY0(fill, x, y);
    setWH(fill, w, h);

    int r = host_basepal[c * 3];
    int g = host_basepal[c * 3 + 1];
    int b = host_basepal[c * 3 + 2];
    setRGB0(fill, r, g, b);

    psx_add_prim(fill, 0);
}
//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen(void)
{
    POLY_F4 *poly = (POLY_F4 *)rb_nextpri;

    setPolyF4(poly);
    setXYWH(poly, 0, 0, VID_WIDTH, VID_HEIGHT);
    setRGB0(poly, 0, 0, 0);
    setSemiTrans(poly, 1);

    menu_add_prim_z(poly, PSX_MENU_ZLEVEL_MENU - 1);

    Sbar_Changed();
}

//=============================================================================

/*
================
Draw_BeginDisc

Draws the little blue disc in the corner of the screen.
Call before beginning any disc IO.
================
*/
void Draw_BeginDisc(void)
{
    if (!draw_disc)
        return;
    Draw_Pic(vid.width - 24, 0, draw_disc);
}

/*
================
Draw_EndDisc

Erases the disc icon.
Call after completing any disc IO
================
*/
void Draw_EndDisc(void)
{
}

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D(void)
{
}

//====================================================================

/*
================
GL_LoadTexture
================
*/
psx_vram_texture *psx_LoadTexture(char const *identifier, int width, int height, byte *data, qboolean mipmap,
                                  qboolean alpha, qboolean shouldCache)
{
    int div = 1;
    psx_vram_texture *tex;

    // see if the texture is already present
    tex = psx_vram_find(identifier, width, height);
    if (tex) {
        printf("GL_LoadTexture \"%s\" already loaded, returning cached\n", identifier);
        return tex;
    }

    if (width > 2 * VRAM_PAGE_WIDTH || height > VRAM_PAGE_HEIGHT) {
        int divw = (width) / VRAM_PAGE_WIDTH;
        int divh = (height) / VRAM_PAGE_HEIGHT;
        div = divw > divh ? divw : divh;
    } else if (mipmap && width > 32 && height > 32) {
        // Mipmapping is enabled on world textures, and we don't have enough VRAM for those,
        // so let's use the mipmap arg to tell which textures to downsize.
        div = 2;
    }

    if (div > 1) {
        int neww = width / div;
        int newh = height / div;

        printf("Scaling tex %s %ix%i down to %ix%i (div %d)\n", identifier, width, height, neww, newh, div);

        for (int y = 0; y < newh; ++y) {
            for (int x = 0; x < neww; ++x) {
                uint8_t val = data[(y * width * div) + (x * div)];
                if (alpha && val == 0xFF) {
                    val = 0;
                }
                data[y * neww + x] = val;
            }
        }

        width = neww;
        height = newh;
    } else if (alpha) {
        for (int i = 0; i < width * height; ++i) {
            if (data[i] == 0xFF) {
                data[i] = 0;
            }
        }
    }

    // For some reason textures not divisible by 16 break DMA
    // Just copy some random data into VRAM to make DMA happy
    int height_dma_hack = height % 4;
    if (height_dma_hack != 0) {
        height += height_dma_hack;
    }
    // TODO unsure why this breaks load? even on emulators??
    // if ((width * height / 2) % 16) {
    //     printf("DMA-breaking tex %s (%dx%d), returning null\n", identifier, width, height);
    //     return NULL;
    // }


    tex = psx_vram_pack(identifier, width, height);
    if (tex == NULL) {
        printf("Failed to pack image %ix%i, ident %s\n", width, height, identifier);
        return NULL;
    }

    RECT load_rect = tex->rect;
    load_rect.x += tex->page->x;
    load_rect.y += tex->page->y;
    load_rect.w /= 2;

    printf("GL_LoadTexture \"%s\" load rect %ix%i\n", identifier, load_rect.w, load_rect.h);

    LoadImage(&load_rect, (uint32_t *)data);
    tex->scale = div;
    tex->is_alpha = alpha;

    if (height_dma_hack != 0) {
        tex->rect.h -= height_dma_hack;
    }

    // printf("GL_LoadTexture \"%s\" (%04x) %ix%i\n", identifier, tex->ident, width, height);

    // if (tex->rect.x + tex->rect.w >= UINT8_MAX) {
    //     printf("XXX %d %d = %d\n", tex->rect.x, tex->rect.w, tex->rect.x + tex->rect.w);
    //     // tex->rect.w = UINT8_MAX - tex->rect.x;
    //     tex->rect.w += UINT8_MAX - (tex->rect.x + tex->rect.w);
    // }
    // if (tex->rect.y + tex->rect.h >= UINT8_MAX) {
    //     printf("YYY %d %d = %d\n", tex->rect.y, tex->rect.h, tex->rect.y + tex->rect.h);
    //     // tex->rect.h = UINT8_MAX - tex->rect.y;
    //     tex->rect.h += UINT8_MAX - (tex->rect.y + tex->rect.h);
    // }
    printf("GL_LoadTexture \"%s\" load rect %ix%i\n", identifier, tex->rect.w, tex->rect.h);

    return tex;
}
