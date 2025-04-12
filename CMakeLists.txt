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

option(GLQUAKE "Enable hardware renderer (GLQuake)" 1)
option(CONSOLE_COMPLETION "Enable console line completion" 0)
option(USE_MATHLIB "Use in-tree math library" 0)
option(PARANOID "Enable additional run-time validation" 0)

set(PLATFORM_VALUES "SDL3" "PSX")
set(PLATFORM "SDL3" CACHE STRING "Minimum number of particles")
set_property(CACHE PLATFORM PROPERTY STRINGS ${PLATFORM_VALUES})

message("Building for platform ${PLATFORM}")

if (${PLATFORM} STREQUAL "SDL3")
	set(PLATFORM_SDL3 1)
elseif (${PLATFORM} STREQUAL "PSX")
	set(PLATFORM_PSX 1)
else ()
	message(FATAL_ERROR "Invalid platform ${PLATFORM}")
endif ()

if (PLATFORM_PSX)
	set(USE_MATHLIB 1)

	psn00bsdk_add_executable(quake GPREL ${COMMON_SRC})

	include_directories(BEFORE SYSTEM include/psx/std)
	target_include_directories(quake PRIVATE include/psx)
	target_compile_definitions(quake PRIVATE PSXQUAKE)

	set(PSX_DATA_DIR ${CMAKE_SOURCE_DIR}/data/psx)
	set(QUAKE_DATA_DIR "${CMAKE_SOURCE_DIR}/id1" CACHE STRING "Quake data root folder, should contain id1 folder")

	psn00bsdk_add_cd_image(
			quake_iso quake ${PSX_DATA_DIR}/iso.xml
			DEPENDS quake ${PSX_DATA_DIR}/system.cnf ${PSX_DATA_DIR}/iso.xml
	)
else ()
	add_executable(quake ${COMMON_SRC})
endif ()

target_include_directories(quake PRIVATE include)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	target_compile_options(quake PRIVATE -Wall -Wextra -fstack-protector-all)
	target_compile_options(quake PRIVATE -Wno-missing-field-initializers -Wno-sign-compare -Wno-pointer-sign)
	# target_compile_definitions(quake PRIVATE PARANOID)
	target_compile_definitions(quake PRIVATE DEBUG)
else ()
	target_compile_options(quake PRIVATE -Os)
	# target_compile_options(quake PRIVATE -fno-stack-protector)
endif ()

target_compile_options(quake PRIVATE -ffast-math)

if (CONSOLE_COMPLETION)
	target_compile_definitions(quake PRIVATE CONSOLE_COMPLETION=${CONSOLE_COMPLETION})
endif ()

if (USE_MATHLIB)
	message("Using in-tree math library")
	# TODO: add non-MIPS support, currently the in-tree math library assumes MIPS
	include_directories(BEFORE SYSTEM include/mathlib)
	add_subdirectory(src/mathlib)
else ()
	target_link_libraries(quake PRIVATE m)
endif ()

set(MAX_MOD_KNOWN 512 CACHE STRING "Maximum number of cached models")
set(PARTICLES_MAX 2048 CACHE STRING "Maximum number of particles")
set(PARTICLES_MIN 512 CACHE STRING "Minimum number of particles")
set(MAX_EFRAGS 640 CACHE STRING "Maximum number of efrags")
set(MAX_EDICTS 600 CACHE STRING "Maximum number of edicts")
set(MAX_LIGHTSTYLES 64 CACHE STRING "Maximum number of lightstyles")
set(MAX_DLIGHTS 32 CACHE STRING "Maximum number of dynamic lights")
set(MAX_BEAMS 24 CACHE STRING "Maximum number of beams")
set(MAX_TEMP_ENTITIES 64 CACHE STRING "Maximum number of temporary entities")
set(MAX_STATIC_ENTITIES 128 CACHE STRING "Maximum number of static entities")
set(MAX_MODELS 256 CACHE STRING "Maximum number of models, this is serialized to a byte, so don't increase")
set(MAX_SOUNDS 256 CACHE STRING "Maximum number of sounds, this is serialized to a byte, so don't increase")

if (PLATFORM_PSX)
	set(MAX_MOD_KNOWN 256)
	set(MAX_PARTICLES 128)
	set(ABSOLUTE_MIN_PARTICLES 64)
	set(MAX_EFRAGS 300)
	set(MAX_EDICTS 300)
	set(MAX_MODELS 128)
	set(MAX_SOUNDS 128)
endif ()

target_compile_definitions(quake PRIVATE MAX_MOD_KNOWN=${MAX_MOD_KNOWN})
target_compile_definitions(quake PRIVATE MAX_PARTICLES=${PARTICLES_MAX})
target_compile_definitions(quake PRIVATE ABSOLUTE_MIN_PARTICLES=${PARTICLES_MIN})
target_compile_definitions(quake PRIVATE MAX_EFRAGS=${MAX_EFRAGS})
target_compile_definitions(quake PRIVATE MAX_EDICTS=${MAX_EDICTS})
target_compile_definitions(quake PRIVATE MAX_LIGHTSTYLES=${MAX_LIGHTSTYLES})
target_compile_definitions(quake PRIVATE MAX_DLIGHTS=${MAX_DLIGHTS})
target_compile_definitions(quake PRIVATE MAX_BEAMS=${MAX_BEAMS})
target_compile_definitions(quake PRIVATE MAX_TEMP_ENTITIES=${MAX_TEMP_ENTITIES})
target_compile_definitions(quake PRIVATE MAX_STATIC_ENTITIES=${MAX_STATIC_ENTITIES})
target_compile_definitions(quake PRIVATE MAX_MODELS=${MAX_MODELS})
target_compile_definitions(quake PRIVATE MAX_SOUNDS=${MAX_SOUNDS})
if (PARANOID)
	message("Compiling with additional run-time checks")
	target_compile_definitions(quake PRIVATE PSXQUAKE_PARANOID=1)
endif ()

add_subdirectory(src/platform)
add_subdirectory(src/render)

install(FILES ${PROJECT_BINARY_DIR}/quake TYPE BIN)
