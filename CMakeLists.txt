cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 11)

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

option(GLQUAKE "Enable hardware renderer (GLQuake)" 1)

option(CONSOLE_COMPLETION "Enable console line completion" 1)
target_compile_definitions(quake PRIVATE CONSOLE_COMPLETION=${CONSOLE_COMPLETION})

set(PARTICLES_MAX 2048 CACHE STRING "Maximum number of particles")
target_compile_definitions(quake PRIVATE MAX_PARTICLES=${PARTICLES_MAX})
set(PARTICLES_MIN 512 CACHE STRING "Minimum number of particles")
target_compile_definitions(quake PRIVATE ABSOLUTE_MIN_PARTICLES=${PARTICLES_MIN})

set(PLATFORM_VALUES "SDL3" "PSX")
set(PLATFORM "SDL3" CACHE STRING "Minimum number of particles")
set_property(CACHE PLATFORM PROPERTY STRINGS ${PLATFORM_VALUES})

if (${PLATFORM} STREQUAL "SDL3")
	set(PLATFORM_SDL3 1)
elseif (${PLATFORM} STREQUAL "PSX")
	set(PLATFORM_PSX 1)
else ()
	message(FATAL_ERROR "Invalid platform ${PLATFORM}")
endif ()

add_subdirectory(src/platform)
add_subdirectory(src/render)

install(FILES ${PROJECT_BINARY_DIR}/quake TYPE BIN)
