cmake_minimum_required(VERSION 3.21)

project(
	quake1
	LANGUAGES    C ASM
	VERSION      1.0.0
	DESCRIPTION  "Quake 1"
	HOMEPAGE_URL ""
)

set(COMMON_SRC
		src/cl_demo.c
		src/cl_input.c
		src/cl_main.c
		src/cl_parse.c
		src/cl_tent.c
		src/chase.c
		src/cmd.c
		src/common.c
		src/console.c
		src/crc.c
		src/cvar.c
		src/host.c
		src/host_cmd.c
		src/keys.c
		src/menu.c
		src/mathlib.c
		src/network/net_none.c
		src/network/net_loop.c
		src/network/net_main.c
		src/network/net_vcr.c
		src/pr_cmds.c
		src/pr_edict.c
		src/pr_exec.c
		src/r_part.c
		src/sbar.c
		src/sv_main.c
		src/sv_phys.c
		src/sv_move.c
		src/sv_user.c
		src/zone.c
		src/view.c
		src/wad.c
		src/world.c
		src/snd_mem.c
		src/snd_mix.c
)

add_executable(quake ${COMMON_SRC})
target_link_libraries(quake PRIVATE m)
target_include_directories(quake PRIVATE include)

set(GLQUAKE 1)

add_subdirectory(src/platform)
add_subdirectory(src/render)

install(FILES ${PROJECT_BINARY_DIR}/quake TYPE BIN)
