# Project structure and source code

How the PC version is put together: the decompiled Nintendo DS game runs unchanged as far as possible, and a native layer (`native/`) stands in for the DS hardware and its SDKs. For playing and building, see the [README](../README.md).

## Top level

| Path | Contents |
| --- | --- |
| `game/src`, `game/include` | the decompiled game (C++), one folder per DS code module: `main` and the overlays `ov000`…`ov037` |
| `game/include/FilePtr.hpp`, `globaldefs.h`, `GameInfo.hpp` | shared definitions: 32-bit pointer fields of game files, ARM/SDK macros, game enums |
| `game/libs/NitroSDK` | the parts of Nintendo's DS SDK the game calls (C): math (`fx`), 2D/3D registers (`gx`), sound driver (`snd`), OS, wireless |
| `game/libs/NitroSystem` | Nintendo's middleware (C): 3D models (`g3d`), 2D cells (`g2d`), heaps (`fnd`), sound archives (`snd`), file formats (`bin`, `gfd`) |
| `native/` | the PC layer: DS hardware replacements, rendering, sound, window, files, settings, PC features |
| `native/compat/` | Metrowerks C library headers mapped to the host's |
| `addons/` | PC additions packed into the executable: party chat texts (`data/MESS/<lang>`), achievement badges |
| `cmake/` | overlay placement (`NativeOverlays.cmake`, `overlay-object.sh`), 32-bit Linux toolchain, version stamping |
| `CMakeLists.txt`, `CMakePresets.json` | the build (presets `windows-game`, `linux-game`, `linux64-game`) |
| `build.sh`, `build.ps1`, `build.bat` | build, extract the ROM/`.obb` into `assets/` once, pack `dist/DragonQuestIV` |
| `tests/cheats` | host tests of the cheat code, without ROM ([cheats-testing.md](cheats-testing.md)) |
| `tests/screen` | drive a test copy of the game on a hidden X display: keys and screenshots |
| `.github/workflows/build.yml` | CI: linux32, linux64 and win32 builds; draft release on a version tag |
| `icon/`, `docs/images/` | icons and README pictures |
| `assets/`, `ROMandOBBfileHere/`, `build32/`, `build64/`, `dist/` | generated or user-supplied, never in git |

## Build targets

Defined in `CMakeLists.txt`:

| Target | What it holds |
| --- | --- |
| `dqiv_vorbis` | stb_vorbis (downloaded at configure time) for the mobile version's music |
| `dqiv_core` | platform core: memory, fixed-point math, 3D geometry/rasterizer/textures, display composition, save, settings, assets, achievements, a few game files that everything needs (`Random`, `Pad`, `GlobalDQ4`…) |
| `dqiv_audio` | SDL audio output |
| `dqiv_platform` | DS filesystem, backup memory, power, threads, overlays, wireless, console identity |
| `dqiv_sound` | the DS sound driver (SDK `snd`) and its native sequencer/voices |
| `dqiv_nitro_system` | all of NitroSystem plus the native model submitter |
| `dqiv_game` | every `game/src/**/*.cpp` (except the files already in `dqiv_core`) |
| `DragonQuestIV` | the executable: boot, window, GPU renderer, presenter, crash handler, cheats, debug tools |
| `dqiv_pack` | build tool: extracts the ROM / `.obb` into `assets/`, packs folders into the executable |

The game is built 32-bit (its data files hold 32-bit pointers) or, on x86-64 Linux, 64-bit with every game address kept below 2 GiB (`native/LowMemory.*`, `game/include/FilePtr.hpp`). GCC flags (`-fno-strict-aliasing -fwrapv -fno-pie`…) keep the decompiled code's assumptions valid.

### Overlays

On the DS, `main` is always resident and the overlays are loaded into shared RAM on demand. Here all code is linked in, but each module's writable data, zeroed data and static constructors go into their own sections, so loading an overlay restores its original data image and runs its constructors, and unloading destroys its objects (`native/NitroOverlays.*`, `FS_LoadOverlay`). With GCC, `cmake/overlay-object.sh` renames each object's `.data`/`.bss`/`.init_array` to `dqiv_d<id>`/`dqiv_b<id>`/`dqiv_i<id>`; with MSVC, `#pragma init_seg`/`data_seg` do the same. `main` is module 38 in that registry.

## The game (`game/src`)

Names follow the original symbols where known; unknown ones keep their DS address (`unkfunc_0205810c`).

### `main` (always resident)

| Folder | Role |
| --- | --- |
| `global` | the application object (`GameApplication`), the global game state (`GlobalDQ4`: start, per-frame step, scene number), start maps, stage links |
| `part` | the scenes (“parts”): logo, title, game start, town, field, battle, battle menu, menu, casino, book (bestiary), wireless exchange, game end |
| `task` | per-frame task execution and the part task manager |
| `dss` | the in-house engine library: camera, rendering, fonts, text conversion, VRAM management, sprites, billboards, fog, pad, random, fixed-point vectors |
| `cmn` | common game logic: game/party/player managers, party chat (`PartyTalk`), Zoom data, walk damage, effects |
| `status` | party and monster status, items, actions, battle history |
| `param` | data tables (items, spells, monsters…) read from the game's files |
| `script` | the event script engine (`sys/ScriptEngine`, trees, groups, objects) |
| `menu`, `window` | menu framework and message/text windows |
| `fld`, `object`, `CameraControl` | field/world objects and cameras shared across scenes |
| `btl`, `encount`, `formation` | battle data, encounters and monster formations shared with the battle overlays |
| `effect`, `sound`, `text`, `data`, `profile` | effects, sound requests, message texts, data objects, save profile |
| `achievement` | PC addition: triggers for the 80 achievements |
| `debug`, `Commands`, `CountDown`, `SpecialParty` | debug parts, command queue, timers, special party setups |

### Overlays

| Overlay | Contents |
| --- | --- |
| `ov000` | towns and dungeons: town scene, characters, furniture, rising/falling objects (`riseup`) |
| `ov001` | the world map (field), its windows and commands |
| `ov002` | wireless exchange (`mp`: tag mode) |
| `ov003` | battle engine: actors, actions, monster drawing, battle tasks |
| `ov006` | the monster book (bestiary) |
| `ov009` | casino: slot machines, poker |
| `ov015` | battle menus: commands, targets, tactics, monster name plates |
| `ov016` | town menus (items, spells, status, operations), shop, name entry, load screen, and the PC menus (`MenuSettings`, `MenuAchievements`, `MenuDebug`) |
| `ov024`…`ov036` | one menu each: inn, side jobs, Surechigai (tag), church, shop/gift exchange, coin sales, bank, medal king, present, colosseum, extra chapter title, Nene, hostage exchange |
| `ov037` | chapter title screens (`PlayerTitleChapter1`…`5`) |

Each overlay has an empty `ovNNN_entry.cpp`, called after it loads, as on the DS.

## The PC layer (`native/`)

### Startup and frame loop

`GameBoot.cpp` holds `main`/`runGame`: it opens the journal (`saves/journal.txt`), loads settings and achievements, mounts the assets, creates the window and every hardware replacement, loads module 38 (`main`), and runs `GlobalDQ4::nativeStep()` once per frame. The game's own threads are fibers (`NitroThreads`); when they all wait for the next VBlank, the idle handler polls the window, mixes the frame's sound, advances power/wireless, and paces to 60 Hz. Fast forward and faster battles skip presenting frames.

### DS hardware and SDK replacements

| Files | Replaces |
| --- | --- |
| `NitroOs`, `NitroThreads`, `NitroMemory`, `NitroAlgorithms`, `Entropy`, `NitroSymbols`, `DssRuntime` | OS calls, threads (as fibers), BIOS copies/fills, ARM-exact sort, entropy, ABI adapters, engine hooks |
| `NitroMath`, `FixedMath` | the ARM matrix routines and the math coprocessor, with the DS's rounding |
| `NitroFilesystem`, `NitroFileApi`, `NitroOverlays` | the ROM filesystem and overlay loading |
| `NitroBackup`, `SaveStore`, `NitroIdentity` | cartridge save memory (`saves/dqiv_eur.sav`, written atomically), the console's identity |
| `NitroPower`, `NitroWireless` | power/VBlank counter, a radio with nobody in range |
| `NitroVideo` | VRAM banks and video registers |
| `NitroGeometry`, `GxGeometry`, `NitroModel` | the 3D geometry engine: command lists, clip coordinates, lighting |
| `GxRasterizer`, `GxTextures`, `Texture` | software reference rasterizer, texture decoding (cached by content hash) |
| `NitroSound`, `SoundSequence`, `SoundVoices`, `SoundWave`, `SoundMixer` | the ARM7 sound driver: sequences, voices/envelopes, ADPCM, 16-channel mixing on the game thread |
| `LowMemory` | 64-bit only: heap and stacks below 2 GiB (restarts once without ASLR) |
| `GameServices` | the game's heaps (freed blocks stay readable a while, as the game relies on it) and file access |

### Rendering and presentation

| Files | Role |
| --- | --- |
| `Display` | the DS 2D engines: backgrounds, sprites, blending; outputs per-pixel “recipes” so the GPU can finish the blend over a high-resolution 3D layer |
| `GpuRenderer`, `GlLoader` | OpenGL 3.3: the DS 3D pipeline (toon, highlight, fog, alpha test) at window resolution, supersampling, Scale2x textures |
| `DualScreen` | joins the two 3D screens into one picture through the bottom camera; wide battle projection and zoom |
| `FramePresenter` | builds each presented frame: joined, field (world map + inset), battle, side-by-side two-screen scenes, interpolated frames above 60 fps |
| `HdPictures` | swaps registered menu icons/portraits for high-resolution pictures |
| `TextCanvas`, `ErrorWindow` | interface text drawn with a system font (Linux), error box |
| `Parallel` | splits per-frame work across cores |

### Window, input and audio

`GameWindow` (SDL3 window, OpenGL context, vsync, frame pacing, settings overlay, achievement notification), `Input` and `InputBindings` (keyboard/gamepad mapped to DS buttons and touch), `AudioOutput` (SDL audio queue with drift control).

### Data and assets

| Files | Role |
| --- | --- |
| `GameInstall` | reads the ROM's file tables and the `.obb` zip in place |
| `Assets`, `AssetPack`, `Archive`, `Compression`, `Binary` | the asset set packed into the executable (FNV-1a checked), DS archives and compressed files, offset helpers |
| `MobileSound`, `VorbisDecoder.c` | the Android version's music (Ogg) and effects (ADPCM) in place of the DS sequences |
| `PackMain` | the `dqiv_pack` tool |

### PC features

| Files | Role |
| --- | --- |
| `Settings` | `saves/settings.ini`, settings texts in six languages |
| `Achievements`, `AchievementTexts` | achievement list, unlock storage (`saves/achievements.ini`), translations |
| `Cheats` | Misc > Cheats switches and actions |
| `DebugTools` | the debug tabs (warp, battle, party, story, tools); built with `DQIV_DEBUG_MENU` |
| `CrashHandler` | symbolized stack in the journal; emulates the DS's division by zero; makes read-only data writable |
| `Profile` | stage timings when `DQIV_PROFILE` is set |
| `Version.hpp` | the git version, generated by `cmake/version.cmake` |

## Conventions worth knowing

- Keep the decompiled code's behaviour, including its bugs, unless a fix is deliberate; comments in `native/` explain each DS detail that is reproduced.
- UTF-16 string literals (`"\xff\xfe..."`) need an explicit trailing `\0`: GCC packs strings, so a single NUL is not a 16-bit terminator.
- A pointer stored in a game file or in a DS-sized structure is `FilePtr<T>` (C++) or `PTR_TO_U32`/`U32_TO_PTR` (C), so the 64-bit build keeps it 32 bits wide.
- Game output and crash reports go to `saves/journal.txt`; the previous run's copy is `journal-precedent.txt`.
