#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "quakedef.h"

#include <psxgpu.h>
#include <psxapi.h>
#include <psxcd.h>

#define SCREEN_XRES 320
#define SCREEN_YRES 240

static uint32_t systick_ms;
static uint8_t ms_per_frame;

qboolean isDedicated;

// =======================================================================
// General routines
// =======================================================================

void Sys_Printf(char const *fmt, ...)
{
    va_list argptr;
    char text[256];
    unsigned char *p;

    va_start(argptr, fmt);
    vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    printf("Sys_Printf: ");
    for (p = (unsigned char *)text; *p; p++) {
        *p &= 0x7f;

        if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
            printf("[%02x]", *p);
        else
            printf("%c", *p);
    }
}

void Sys_Quit()
{
    Host_Shutdown();
}

void Sys_Init()
{
}

void Sys_Error(char const *error, ...)
{
    va_list argptr;
    char string[1024];

    va_start(argptr, error);
    vsnprintf(string, sizeof(string), error, argptr);
    va_end(argptr);
    printf("Sys_Error: %s\n", string);

    Host_Shutdown();

    for (;;) {
    }
}

uint32_t Sys_CurrentTicks()
{
    return systick_ms;
}

// =======================================================================
// Sleeps for microseconds
// =======================================================================

char *Sys_ConsoleInput()
{
    // Unsupported on PSX
    return nullptr;
}

void Sys_HighFPPrecision()
{
}

void Sys_LowFPPrecision()
{
}

static void vblank_handler()
{
    // FIXME? systick overflow not handled. Although I don't think anyone will play PSXQuake for over 49 days
    systick_ms += ms_per_frame;
}

static uint8_t membase[5 * 1024 * 1024];
extern uint8_t _end[];

void iterate_files_dir(char * path)
{
    CdlFILE file;
    size_t path_len = strlen(path);

    printf("Opening dir %s\n", path);

    CdlDIR * dir = CdOpenDir(path);
    if (dir == nullptr) {
        printf("Failed to open directory \"%s\" for file iteration\n", path);
        return;
    }

    while (CdReadDir(dir, &file)) {
        std::string_view name{file.name};
        printf("Pre \"%s\", size %u bytes\n", file.name, file.size);
        if (name.starts_with('.')) {
            // Skip "." and ".." (and all other dotfiles, coincidentally)
            continue;
        }
        if (name.ends_with(";1")) {
            printf("File \"%s\", size %u bytes\n", file.name, file.size);
        } else {
            if (path[path_len - 1] != '\\') {
                path[path_len] = '\\';
                path_len += 1;
            }
            memcpy(path + path_len, file.name, name.length());
            path[path_len + name.length()] = '\0';
            iterate_files_dir(path);
        }
    }

    CdCloseDir(dir);
}

void iterate_files()
{
    // Hackish, but most likely fine
    char pathbuf[256];
    strcat(pathbuf, "\\");
    iterate_files_dir(pathbuf);
}

int main(int c, char **v)
{
    uint32_t time, oldtime, newtime;
    quakeparms_t parms;

    void *heap_addr = _end + 4;
#ifndef PSXQUAKE_2MB_RAM
    // 8MiB RAM
    size_t heap_size = (uint8_t *)(0x80000000 + 0x7ffff8) - _end;
#else
    // 2MiB RAM TODO unsupported
    size_t heap_size = (uint8_t *)(0x80000000 + 0x1ffff8) - _end;
#endif
    printf("Allocating heap at %p, size %u\n", heap_addr, heap_size);
    EnterCriticalSection();
    InitHeap(_end + 4, heap_size);
    ExitCriticalSection();

    printf("Starting up Quake, video mode %s...\n", GetVideoMode() == MODE_PAL ? "PAL" : "NTSC");

    srand((int)heap_addr);

    ResetGraph(0);

    EnterCriticalSection();
    if (GetVideoMode() == MODE_PAL) {
        ms_per_frame = 20; // 1000 / 50 = 20.0
    } else {
        ms_per_frame = 17; // 1000 / 60 = 16.6(6)
    }
    VSyncCallback(vblank_handler);
    ExitCriticalSection();

    printf("PSX CD init...");
    CdInit();
    printf("OK\n");

    CdControl(CdlNop, nullptr, nullptr);
    CdStatus();

    // printf("File data map:\n");
    // iterate_files();

    memset(&parms, 0, sizeof(parms));

    printf("Com init...");
    COM_InitArgv(c, v);
    printf("OK\n");
    parms.argc = com_argc;
    parms.argv = com_argv;

    parms.memsize = sizeof(membase);
    parms.membase = membase;

    parms.basedir = ".";
    // Caching is disabled by default, use -cachedir to enable
    // parms.cachedir = cachedir;

    printf("Host init...\n");
    Host_Init(&parms);
    printf("OK\n");

    printf("Sys init...");
    Sys_Init();
    printf("OK\n");

    printf("PSXQuake going into game loop...\n");

    oldtime = Sys_CurrentTicks() - 100;

    int tickrate_ms = (int)sys_ticrate_ms.value * 2;
    while (1) {
        // find time spent rendering last frame
        newtime = Sys_CurrentTicks();
        time = newtime - oldtime;

        if (time > tickrate_ms)
            oldtime = newtime;
        else
            oldtime += time;

        Host_Frame(time);
    }
}
