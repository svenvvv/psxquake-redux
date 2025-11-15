# PSXQuake

A port of the game Quake 1 to the Playstation 1.
Now also supports running on the PC!

>[!CAUTION]
>This readme might not be up to date and the PSX target might be broken.
>
>If you want to give it a run then please use the [non-redux psxquake repo](https://github.com/svenvvv/psxquake)
>
>The non-redux one is missing the PC target, which annoyed me enough during development to undertake this project.

![Screenshot](images/20241010.jpg)

Currently the software renderer is functional, albeit with horrible performance (<1 fps without emulator overlock).

The hardware renderer can draw menus (at 60 fps!) and even the in-game world at a glorious 3 FPS.

No sound, saving, etc.

## Building

### Prerequisites

>[!NOTE]
> You might have to compile GCC for mipsel target yourself, as the precompiled
> ones linked to by PSn00bSDK don't ship a complete enough C++ standard library.
>
> Currently I'm using self-compiled GCC 15.2.
> I've checked the Arch Linux PKGBUILD I used into this repository (scripts/mipsle-gcc),
> as it required some modifications to support softfloat that I don't want to re-do.
> The binutils package doesn't need any modifications, so you can use the `mipsel-elf-binutils`
> package from AUR.

1. Set up PSn00bSDK following [their installation guide](https://github.com/Lameguy64/PSn00bSDK/blob/master/doc/installation.md);
2. Clone this repo.

### Compiling for PSX

1. Copy the `ID0.pak` to `${PROJECT_SOURCE_DIR}/psx_cd/ID1/PAK0.PAK`;
2. Generate the build files:
```sh
PSN00BSDK_LIBS=/path/to/your/PSn00bSDK/lib/libpsn00b cmake --preset=default .
```
3. Build the project (assuming Ninja generator):
```sh
ninja -C build
```

The resulting PSXQuake CD image (`quake.{bin,que}`) will be output in the `build` directory.

Switching out the renderer can be done by changing the `source` of the exe in the file `psx_cd/iso.xml`:
```xml
<!-- For hardware-rendered PSXQuake -->
<file name="QUAKE.EXE"	type="data" source="glquake.exe" />
<!-- For software-rendered PSXQuake -->
<file name="QUAKE.EXE"	type="data" source="swquake.exe" />
```

### Compiling for PC

Compiling for PC is now also supported!

Only tested under Linux, but since the PC code is implemented using SDL3 then it'll maybe also run on Windows machines.

The aim of the PC target is not to be the greatest Quake port, but to allow for simpler debugging of code that's not plaform specific.

1. Generate the build files:
```sh
cmake -S . -B ./build -G "Ninja" -DPLATFORM=SDL3 -DGLQUAKE=0
```
2. Build:
```sh
ninja -C build
```

## Running

Currently PSXQuake can only run on dev consoles (8 MiB of RAM), so you will have to enable it in the emulator settings.

It's pretty heavy on the CD drive, so enabling CD drive read speedups in your emulator is recommended.

PSXQuake has been tested with the Duckstation emulator, never tried on real hardware.


