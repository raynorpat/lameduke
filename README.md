# LameDuke

A work-in-progress C rebuild of the December 30, 1994 Duke Nukem 3D beta
known as LameDuke. The game and its early Build engine behavior are being
recovered from `D3D.EXE`, with SDL2 providing the modern platform layer and
CMake building for Windows, macOS and Linux.

The goal is to run the beta's original loose data files and reproduce its
classic renderer, demos and local gameplay. This is an early reconstruction:
demo playback and local play work, but behavior and rendering are still being
checked against the DOS executable. Menus, sound effects and music are not yet
complete; multiplayer is stubbed out.

## Quick start

Requirements: a C compiler, CMake 3.24 or newer, and SDL2. CMake uses an
installed SDL2 when it finds one; otherwise it downloads and builds SDL2
2.32.10, which requires network access. The default build also needs OpenGL
development libraries.

| OS | Setup |
|---|---|
| macOS | `brew install cmake sdl2` |
| Linux (Debian/Ubuntu) | `sudo apt install cmake build-essential libsdl2-dev libgl-dev` |
| Windows | Visual Studio with "Desktop development with C++" and CMake. SDL2 is fetched if it is not found. |

From the repository root:

```sh
cmake --preset default
cmake --build --preset default
```

The game is `build/bin/d3d` (`build/bin/d3d.exe` on Windows). Multi-configuration
generators put it under `build/bin/RelWithDebInfo/`. The `debug` preset builds
in `build-debug/` instead.

Run the game from the repository root, where it can find the data files:

```sh
./build/bin/d3d
./build/bin/d3d /v1 /l1
```

The first command starts the attract demos; the second starts episode 1,
level 1. Use `build\bin\d3d.exe` for these commands in Windows Command Prompt.
The original release notes are in [LAMEDUKE.TXT](LAMEDUKE.TXT).

`build/bin/build` is the BUILD map editor for this beta's maps, and
`build/bin/convmap4` converts the version-4 maps `L9.MAP` and `M8.MAP` to
version 5. See [docs/map-editor.md](docs/map-editor.md).

## Game data

The repository contains the beta's loose maps, art, scripts, demos, palette,
tables, sounds and music. The game reads them from its working directory;
it does not use a GRP archive. `SETUP.DAT` supplies video, sound and input
settings. On case-sensitive filesystems, file lookup also tries a
case-insensitive match.

The original DOS executables and IDA databases are not tracked. The game data
and the Duke Nukem name remain subject to their respective copyright and
trademark holders' rights.

## Build options

Pass options with `-D` when configuring.

| Option | Default | Effect |
|---|---|---|
| `LAMEDUKE_FETCH_SDL` | `AUTO` | Use installed SDL2 or fetch it if missing; `ON` always fetches it and `OFF` requires an installed copy. |
| `LAMEDUKE_OPENGL` | `ON` | Require and link OpenGL. The current presentation path uses SDL2; the OpenGL presentation function is a stub. |

## Project layout

| Path | Contents |
|---|---|
| `source/build/` | Reconstructed Build engine, renderer and platform code. See [VENDORED.md](source/build/VENDORED.md). |
| `source/audiolib/` | Audio library interfaces and unfinished implementation. |
| `source/` | Game simulation, CON compiler and interpreter, demos, and menus. |
| `docs/superpowers/` | Rebuild design and implementation plan. |

## Build engine license

Ken Silverman's released Build source has its own terms, reproduced in
[BUILDLIC.TXT](source/build/BUILDLIC.TXT). The engine provenance and local
changes are described in [VENDORED.md](source/build/VENDORED.md). Those terms
are distinct from the rights in the original game and its data files.
