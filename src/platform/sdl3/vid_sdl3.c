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
// vid_null.c -- null video driver to aid porting efforts

#include "quakedef.h"
#include "d_local.h"

#include <SDL3/SDL.h>

#define	BASEWIDTH	320
#define	BASEHEIGHT	200

static SDL_Window * sdl_window;
static SDL_Renderer * sdl_renderer;
static SDL_Surface * sdl_surface;
static SDL_Palette * sdl_palette;

// byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	surfcache[256*1024];

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];

void	VID_SetPalette (unsigned char *palette)
{
	static SDL_Color colors[256];

	if (!SDL_SetPaletteColors(sdl_palette, colors, 0, 256)) {
		Sys_Error("Failed to update SDL palette, %s", SDL_GetError());
	}

	for (int i = 0; i < 256; i++)
	{
		colors[i].r = palette[i*3] * 257;
		colors[i].g = palette[i*3+1] * 257;
		colors[i].b = palette[i*3+2] * 257;
		colors[i].a = 0xFF;
	}

	/*
	int i;
	XColor colors[256];

	for (i=0;i<256;i++) {
		d_8to16table[i] = xlib_rgb16(palette[i*3], palette[i*3+1],palette[i*3+2]);
		d_8to24table[i] = // xlib_rgb24(palette[i*3], palette[i*3+1],palette[i*3+2]);
	}

	if (x_visinfo->class == PseudoColor && x_visinfo->depth == 8)
	{
		if (palette != current_palette)
			memcpy(current_palette, palette, 768);
		for (i=0 ; i<256 ; i++)
		{
			colors[i].pixel = i;
			colors[i].flags = DoRed|DoGreen|DoBlue;
			colors[i].red = palette[i*3] * 257;
			colors[i].green = palette[i*3+1] * 257;
			colors[i].blue = palette[i*3+2] * 257;
		}
		XStoreColors(x_disp, x_cmap, colors, 256);
	}
	*/
}

void	VID_ShiftPalette (unsigned char *palette)
{
	VID_SetPalette(palette);
}

void	VID_Init (unsigned char *palette)
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		Sys_Error("Could not initialize SDL, %s", SDL_GetError());
	}

	sdl_window = SDL_CreateWindow ("Quake", 3 * BASEWIDTH, 3 * BASEHEIGHT, 0);
	if (sdl_window == NULL) {
		Sys_Error("Could not create SDL window, %s", SDL_GetError());
	}

	sdl_renderer = SDL_CreateRenderer(sdl_window, NULL);
	if (sdl_renderer == NULL) {
		Sys_Error("Could not create SDL renderer, %s", SDL_GetError());
	}

	sdl_surface = SDL_CreateSurface(BASEWIDTH, BASEHEIGHT, SDL_PIXELFORMAT_INDEX8);
	if (sdl_surface == NULL) {
		Sys_Error("Could not create SDL surface, %s", SDL_GetError());
	}

	sdl_palette = SDL_CreateSurfacePalette(sdl_surface);
	if (sdl_surface == NULL) {
		Sys_Error("Could not create SDL palette, %s", SDL_GetError());
	}

	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));

	vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = 1.0f;
	vid.numpages = 1;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	// vid.buffer = vid.conbuffer = vid_buffer;
	vid.buffer = vid.conbuffer = sdl_surface->pixels;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;
}

void	VID_Shutdown (void)
{
	SDL_DestroySurface(sdl_surface);
	SDL_DestroyRenderer (sdl_renderer);
	SDL_DestroyWindow (sdl_window);
}

void	VID_Update (vrect_t *rects)
{
	SDL_UnlockSurface(sdl_surface);

	SDL_Texture * tex = SDL_CreateTextureFromSurface(sdl_renderer, sdl_surface);
	if (tex == NULL) {
		Sys_Error("Failed to create SDL texture, %s", SDL_GetError());
	}
	SDL_RenderClear (sdl_renderer);
	SDL_RenderTexture(sdl_renderer, tex, NULL, NULL);
	SDL_RenderPresent(sdl_renderer);

	SDL_DestroyTexture(tex);
	SDL_LockSurface(sdl_surface);
}

void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}

void D_EndDirectRect (int x, int y, int width, int height)
{
}