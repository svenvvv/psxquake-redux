cmake_minimum_required(VERSION 3.21)

set(CMAKE_C_STANDARD 11)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

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
		src/murmurhash2.c
)

add_executable(quake ${COMMON_SRC})
target_link_libraries(quake PRIVATE m)
target_include_directories(quake PRIVATE include)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	target_compile_options(quake PRIVATE -Wall -Wextra -fstack-protector-all)
	target_compile_options(quake PRIVATE -Wno-missing-field-initializers -Wno-sign-compare -Wno-pointer-sign)
	# target_compile_definitions(quake PRIVATE PARANOID)
else ()
	target_compile_options(quake PRIVATE -Os)
	# target_compile_options(quake PRIVATE -fno-stack-protector)
endif ()

option(GLQUAKE "Enable hardware renderer (GLQuake)" 1)

option(CONSOLE_COMPLETION "Enable console line completion" 0)
if (CONSOLE_COMPLETION)
	target_compile_definitions(quake PRIVATE CONSOLE_COMPLETION=${CONSOLE_COMPLETION})
endif ()

set(PARTICLES_MAX 2048 CACHE STRING "Maximum number of particles")
target_compile_definitions(quake PRIVATE MAX_PARTICLES=${PARTICLES_MAX})
set(PARTICLES_MIN 512 CACHE STRING "Minimum number of particles")
target_compile_definitions(quake PRIVATE ABSOLUTE_MIN_PARTICLES=${PARTICLES_MIN})
set(MAX_EFRAGS 640 CACHE STRING "Maximum number of efrags")
target_compile_definitions(quake PRIVATE MAX_EFRAGS=${MAX_EFRAGS})
set(MAX_EDICTS 600 CACHE STRING "Maximum number of edicts")
target_compile_definitions(quake PRIVATE MAX_EDICTS=${MAX_EDICTS})
set(MAX_LIGHTSTYLES 64 CACHE STRING "Maximum number of lightstyles")
target_compile_definitions(quake PRIVATE MAX_LIGHTSTYLES=${MAX_LIGHTSTYLES})
set(MAX_DLIGHTS 32 CACHE STRING "Maximum number of dynamic lights")
target_compile_definitions(quake PRIVATE MAX_DLIGHTS=${MAX_DLIGHTS})
set(MAX_BEAMS 24 CACHE STRING "Maximum number of beams")
target_compile_definitions(quake PRIVATE MAX_BEAMS=${MAX_BEAMS})
set(MAX_TEMP_ENTITIES 64 CACHE STRING "Maximum number of temporary entities")
target_compile_definitions(quake PRIVATE MAX_TEMP_ENTITIES=${MAX_TEMP_ENTITIES})
set(MAX_STATIC_ENTITIES 128 CACHE STRING "Maximum number of static entities")
target_compile_definitions(quake PRIVATE MAX_STATIC_ENTITIES=${MAX_STATIC_ENTITIES})
set(MAX_MODELS 256 CACHE STRING "Maximum number of models, this is serialized to a byte, so don't increase")
target_compile_definitions(quake PRIVATE MAX_MODELS=${MAX_MODELS})
set(MAX_SOUNDS 256 CACHE STRING "Maximum number of sounds, this is serialized to a byte, so don't increase")
target_compile_definitions(quake PRIVATE MAX_SOUNDS=${MAX_SOUNDS})

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
