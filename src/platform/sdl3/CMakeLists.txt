target_sources(quake PRIVATE
        cd_sdl3.c
        in_sdl3.c
        snd_sdl3.c
        sys_sdl3.c
)
if (GLQUAKE)
    target_sources(quake PRIVATE gl_vidsdl3.c)
else ()
    target_sources(quake PRIVATE vid_sdl3.c)
endif ()

target_link_libraries(quake PRIVATE SDL3)
