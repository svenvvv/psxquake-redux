target_sources(quake PRIVATE
        psx_draw.c
        psx_mesh.c
        psx_model.c
        psx_refrag.c
        psx_rlight.c
        psx_rmain.c
        psx_rmisc.c
        psx_screen.c
        psx_warp.c
        psx_rsurf.c
)
target_compile_definitions(quake PRIVATE GLQUAKE)
