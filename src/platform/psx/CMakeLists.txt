target_sources(quake PRIVATE
        cd_psx.c
        in_psx.c
        snd_psx.c
        sys_psx.c
        sys_psx_fileio.c
)
if (GLQUAKE)
    target_sources(quake PRIVATE
            gl_vidpsx.c
            gl_vidpsx_vram.c
            stub_gl.c
    )
else ()
    target_sources(quake PRIVATE vid_psx.c)
endif ()

target_sources(quake PRIVATE
        std/io.c
        std/lib.c
        std/string.c
        gl.c
)

