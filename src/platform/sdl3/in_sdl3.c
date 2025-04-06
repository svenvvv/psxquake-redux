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
// in_null.c -- for systems without a mouse

#include "quakedef.h"

#include <SDL3/SDL.h>

void IN_Init(void)
{
}

void IN_Shutdown(void)
{
}

void IN_Commands(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type != SDL_EVENT_KEY_UP && event.type != SDL_EVENT_KEY_DOWN) {
            continue;
        }

        SDL_Keycode sdl_key = event.key.key;
        bool pressed = event.key.down;

        Key_Event(K_SHIFT, event.key.mod & SDL_KMOD_SHIFT);

        if (sdl_key == SDLK_ESCAPE) {
            Key_Event(K_ESCAPE, pressed);
        } else if (sdl_key == SDLK_TAB) {
            Key_Event(K_TAB, pressed);
        } else if (sdl_key == SDLK_RETURN) {
            Key_Event(K_ENTER, pressed);
        } else if (sdl_key == SDLK_SPACE) {
            Key_Event(K_SPACE, pressed);
        } else if (sdl_key == SDLK_BACKSPACE) {
            Key_Event(K_BACKSPACE, pressed);
        } else if (sdl_key == SDLK_UP) {
            Key_Event(K_UPARROW, pressed);
        } else if (sdl_key == SDLK_DOWN) {
            Key_Event(K_DOWNARROW, pressed);
        } else if (sdl_key == SDLK_LEFT) {
            Key_Event(K_LEFTARROW, pressed);
        } else if (sdl_key == SDLK_RIGHT) {
            Key_Event(K_RIGHTARROW, pressed);
        } else if (SDLK_0 <= sdl_key && sdl_key <= SDLK_Z) {
            Key_Event('0' + event.key.key - SDLK_0, pressed);
        }
    }
}

void IN_Move(usercmd_t *cmd)
{
    (void)cmd;
}
