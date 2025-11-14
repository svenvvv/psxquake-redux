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

#include <SDL3/SDL.h>

#define BASEWIDTH 320
#define BASEHEIGHT 200

static SDL_Window *sdl_window;
static SDL_GLContext sdl_context;

unsigned d_8to24table[256];
unsigned char d_15to8table[65536];

int texture_mode = GL_LINEAR;
qboolean gl_mtexable = false;
qboolean isPermedia = false;
float gldepthmin, gldepthmax;
int texture_extension_number = 1;

CVAR_REGISTER(gl_ztrick, CVAR_CTOR({ "gl_ztrick", 1 }));

void VID_SetPalette(unsigned char *palette)
{
    byte *pal;
    unsigned r, g, b;
    unsigned v;
    int r1, g1, b1;
    int k;
    unsigned short i;
    unsigned *table;
    int dist, bestdist;

    //
    // 8 8 8 encoding
    //
    pal = palette;
    table = d_8to24table;
    for (i = 0; i < 256; i++) {
        r = pal[0];
        g = pal[1];
        b = pal[2];
        pal += 3;

        v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
        *table++ = v;
    }
    d_8to24table[255] &= 0xffffff; // 255 is transparent

    for (i = 0; i < (1 << 15); i++) {
        /* Maps
		000000000000000
		000000000011111 = Red  = 0x1F
		000001111100000 = Blue = 0x03E0
		111110000000000 = Grn  = 0x7C00
		*/
        r = ((i & 0x1F) << 3) + 4;
        g = ((i & 0x03E0) >> 2) + 4;
        b = ((i & 0x7C00) >> 7) + 4;
        pal = (unsigned char *)d_8to24table;
        for (v = 0, k = 0, bestdist = 10000 * 10000; v < 256; v++, pal += 4) {
            r1 = (int)r - (int)pal[0];
            g1 = (int)g - (int)pal[1];
            b1 = (int)b - (int)pal[2];
            dist = (r1 * r1) + (g1 * g1) + (b1 * b1);
            if (dist < bestdist) {
                k = v;
                bestdist = dist;
            }
        }
        d_15to8table[i] = k;
    }
}

void VID_ShiftPalette(unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_Init(unsigned char *palette)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Sys_Error("Could not initialize SDL, %s", SDL_GetError());
    }

    sdl_window = SDL_CreateWindow("Quake", 3 * BASEWIDTH, 3 * BASEHEIGHT, SDL_WINDOW_OPENGL);
    if (sdl_window == NULL) {
        Sys_Error("Could not create SDL window, %s", SDL_GetError());
    }

    sdl_context = SDL_GL_CreateContext(sdl_window);
    if (sdl_context == NULL) {
        Sys_Error("Could not create SDL GL context, %s", SDL_GetError());
    }

    vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
    vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
    vid.aspect = 1.0f;
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
    // vid.buffer = vid.conbuffer = vid_buffer;
    // vid.buffer = vid.conbuffer = sdl_surface->pixels;
    vid.rowbytes = vid.conrowbytes = BASEWIDTH;

    VID_SetPalette(palette);

    glClearColor(1, 0, 0, 0);
    glCullFace(GL_FRONT);
    glEnable(GL_TEXTURE_2D);

    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.666);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glShadeModel(GL_FLAT);

    texture_mode = GL_NEAREST;
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_mode);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_mode);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

void VID_Shutdown(void)
{
    SDL_GL_DestroyContext(sdl_context);
    SDL_DestroyWindow(sdl_window);
}

void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
    (void)x;
    (void)y;
    (void)pbitmap;
    (void)width;
    (void)height;
}

void D_EndDirectRect(int x, int y, int width, int height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void GL_BeginRendering(int *x, int *y, int *width, int *height)
{
    *x = *y = 0;
    SDL_GetWindowSize(sdl_window, width, height);
}

void GL_EndRendering()
{
    glFlush();
    SDL_GL_SwapWindow(sdl_window);
}
