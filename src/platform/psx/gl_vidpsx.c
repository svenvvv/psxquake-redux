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

#include "quakedef.h"
#include <stdbool.h>

#include <psxgpu.h>
#include <psxgte.h>
#include <inline_c.h>
#include "psx/gl.h"

#define stringify(m) { #m, m }

unsigned short d_8to16table[256];
unsigned d_8to24table[256]; //TODO this can be removed for PSX GL

/*-----------------------------------------------------------------------*/

int texture_extension_number = 1;

float gldepthmin, gldepthmax;

cvar_t gl_ztrick = { "gl_ztrick", "1" };

/*-----------------------------------------------------------------------*/
void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect(int x, int y, int width, int height)
{
}

void VID_Shutdown(void)
{
}

void VID_ShiftPalette(unsigned char *p)
{
    VID_SetPalette(p);
}

static RECT psx_clut_rect = {
    .x = 0,
    .y = (2 * VID_HEIGHT),
    .w = 16 * 16,
    .h = 1,
};

static RECT psx_clut_transparent_rect = {
    .x = 0,
    .y = (2 * VID_HEIGHT) + 1,
    .w = 16 * 16,
    .h = 1,
};

uint16_t psx_clut = 0;
uint16_t psx_clut_transparent = 0;

void VID_SetPalette(unsigned char *palette)
{
    for (int i = 0; i < 256; ++i) {
        uint8_t r = palette[i * 3];
        uint8_t g = palette[i * 3 + 1];
        uint8_t b = palette[i * 3 + 2];
        d_8to16table[i] = psx_rgb16(r, g, b, 1);
    }
    LoadImage(&psx_clut_rect, (uint32_t *)d_8to16table);
    psx_clut = getClut(psx_clut_rect.x, psx_clut_rect.y);

    for (int i = 0; i < 256; ++i) {
        uint8_t r = palette[i * 3];
        uint8_t g = palette[i * 3 + 1];
        uint8_t b = palette[i * 3 + 2];
        d_8to16table[i] = psx_rgb16(r, g, b, 0);
    }
    LoadImage(&psx_clut_transparent_rect, (uint32_t *)d_8to16table);
    psx_clut_transparent = getClut(psx_clut_transparent_rect.x, psx_clut_transparent_rect.y);
}

/*
===============
GL_Init
===============
*/

static MATRIX color_mtx = {
    ONE * 3 / 4, 0, 0, /* Red   */
    ONE * 3 / 4, 0, 0, /* Green */
    ONE * 3 / 4, 0, 0 /* Blue  */
};

void GL_Init(void)
{
    psx_rb_init();

    InitGeom();

    gte_SetGeomOffset(VID_WIDTH >> 1, VID_HEIGHT >> 1);
    gte_SetGeomScreen(VID_WIDTH >> 1);

#ifdef DEBUG
    gte_SetBackColor(63, 63, 63);
#else
    gte_SetBackColor(0, 0, 0);
#endif

    gte_SetColorMatrix(&color_mtx);

    // SetDispMask(1);
    SetDispMask(1);
}

/*
=================
GL_BeginRendering

=================
*/
void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
}

void GL_EndRendering(void)
{
    psx_rb_present();
}

void VID_Init(unsigned char *palette)
{
    Cvar_RegisterVariable(&gl_ztrick);

    vid.maxwarpwidth = vid.width = VID_WIDTH;
    vid.maxwarpheight = vid.height = VID_HEIGHT;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));

    vid.conwidth = VRAM_PAGE_WIDTH * 2;
    vid.conheight = VRAM_PAGE_HEIGHT; //vid.conwidth*3 / 4;

    // pick a conheight that matches with correct aspect
    // vid.conheight = vid.conwidth*3 / 4;
    // vid.conheight = 200;

    // vid.width = vid.conwidth;
    // vid.height = vid.conheight;

    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0f / 240.0f);
    vid.numpages = 2;

    GL_Init();

    // Check_Gamma(palette);
    VID_SetPalette(palette);

    vid.recalc_refdef = 1; // force a surface cache flush

    psx_vram_init();
}

void Sys_SendKeyEvents(void)
{
    IN_Commands();
}
