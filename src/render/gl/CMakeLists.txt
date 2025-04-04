target_sources(quake PRIVATE
        gl_draw.c
        gl_mesh.c
        gl_model.c
        gl_refrag.c
        gl_rlight.c
        gl_rmain.c
        gl_rmisc.c
        gl_screen.c
        gl_warp.c
        gl_rsurf.c
)
target_compile_definitions(quake PRIVATE GLQUAKE)
target_link_libraries(quake PRIVATE GL)
