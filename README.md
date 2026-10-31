<p align="center"><img src="icon/DragonQuestIV.png" width="160" alt="Dragon Quest IV PC icon"></p>

# Dragon Quest IV: native PC version

The original Nintendo DS game (built from its decompilation) runs natively on Windows and Linux, without an emulator. It has HD rendering, both screens joined into one wide picture that takes the shape of your screen (16:9, 16:10, 21:9…) without stretching, the mobile version's sound and the party chat back in every language.

![The town in HD, 16:9](docs/images/town.jpg)

## Playing

Double-click `dist\DragonQuestIV.exe` (Windows) or run `dist/DragonQuestIV` (Linux). Every game resource is packed inside it, so it can be moved on its own.

**Linux release** (no building needed): download the archive from the releases page and extract it. Put your European Dragon Quest IV ROM (`.nds`, game code `YIVP`) in the `ROMandOBBfileHere` folder beside `DragonQuestIV`, and optionally the Android version's `.obb` for the HD sound, then run `DragonQuestIV`. The game reads them from there each time it starts; it contains none of the game's data. The same happens with a `dist/DragonQuestIV` built without a ROM (see [Building](#building)).

The game keeps its files in a `saves` folder next to the executable:

| File | Contents |
| --- | --- |
| `dqiv_eur.sav` | the game save |
| `settings.ini` | the PC settings |
| `achievements.ini` | the achievements unlocked |
| `journal.txt` | the log (details of an error, if the game stops) |

Moving the game together with its `saves` folder keeps your progress. On Windows, on the first start, a save from an earlier version (`%LOCALAPPDATA%\DQIV-PC`) is copied into `saves` automatically.

### Default controls

All keys and gamepad buttons can be changed in the settings.

| Action | Keyboard | Gamepad |
| --- | --- | --- |
| Move / choose | Arrow keys | D-pad or left stick |
| A (confirm) | Z or Space | A |
| B (cancel) | X, Backspace or Escape | B |
| X / Y | S / A | Y / X |
| L / R | Q / W | LB / RB |
| Start / Select | Enter / Right Shift | Start / Back |
| Party chat (when walking, outside menus) | E | B |
| Fast forward (hold: up to 4x, silent) | Tab | RT |
| Camera zoom | Mouse wheel, numpad + / - | Right stick |
| DS touch screen | Left click on the bottom view | |

Letters follow the active keyboard layout (AZERTY included).

Function keys:
- **F8**: wide picture or the DS screens' width.
- **F9**: window size.
- **F10**: stretch or keep proportions.
- **F11**: fullscreen.
- **F12**: PNG screenshot, saved in `%AppData\Roaming\DQIV-PC` (Windows) or `~/.local/share/DQIV-PC` (Linux).

## What's new

### Both screens become one picture

On the DS, towns and dungeons are drawn on two screens, each at 30 frames per second (the bottom screen on odd frames, the top one on even frames). Here every frame is drawn whole by the bottom camera, extended over the top screen: one seamless picture at 60 frames per second. In the wide picture the scene widens at the sides.

![The DS screens beside the PC picture, same moment](docs/images/ds-vs-pc.jpg)

### HD rendering

The 3D is drawn by the graphics card at the window's resolution:
- 2x supersampling, which lowers itself automatically if the card can't hold 60 fps;
- textures upscaled (Scale2x) and filtered;
- backgrounds, sprites and text keep sharp pixels at any size.

Menu icons and the party's portraits are drawn in high resolution. Above 60 fps (120, 144, 165, 240 Hz or unlimited), the 3D is redrawn between the game's frames for smoother movement.

The world map fills the whole picture and scrolls at 60 frames per second. The top screen's map becomes an inset.

![The world map in full screen](docs/images/world-map.jpg)

The opening's cloud sky is widened to fill 16:9.

![The opening sky in 16:9](docs/images/intro.jpg)

### Camera zoom

The camera zooms from 50% to 200% in towns, dungeons and on the world map. Use the mouse wheel, numpad + / -, the right stick, or Settings > Display.

![Camera zoom, 70% and 170%](docs/images/camera-zoom.jpg)

### Wide battles

Battles fill the wide picture (the Large layout needs at least 16:10; narrower screens use the normal one): the party's windows are drawn a little smaller at the top, the commands keep their place at the bottom, and the scene is zoomed so that the monsters fill the room between them, groups of four included (up to twice their DS size; 1.15 to 1.5 times for the tallest bosses such as Estark, seen whole). The monsters' names, the damage numbers and the light on the targeted monster follow the monsters outside the DS bottom screen, above it and beside it; monsters and names never touch the windows or the menus. Settings > Display > Battle display also offers **Panorama** (the scene over the whole picture, with the party's windows on top; Battle zoom brings the monsters closer, bosses are fitted under the windows) and **DS screens** (the original two screens).

![A 16:9 battle](docs/images/battle.jpg)

### Monster names colored by HP

As in the HD-2D remakes, a monster group's name turns yellow when one of them is at 25% of its HP or less, and red under 10%.

![Master Kung's name in red](docs/images/monster-hp-colors.jpg)

### Party chat is back

The Japanese and mobile versions let you talk to your party; the European DS version had removed it. Press **E** (or **B** on a gamepad) while walking, or use "Talk" with nobody in front of you, and a party member says something. Lines depend on the place, the story so far, the time of day and who is in the party.

The English text comes from the mobile version, with the fixes of the dq4-partychat-patcher.

![Party chat in English](docs/images/party-chat.jpg)

All 7,796 lines were translated into French, German, Italian and Spanish, in the style of the official DS translations: official names, and characters keep their accents (Alena, Kiryl and Borya keep their Zamoksvan accent). Japanese uses the original text of the Japanese DS version. The files are in `addons/data/MESS/<language>/b05xxxxx.mpt`.

![Party chat in French, German, Italian and Spanish](docs/images/party-chat-translations.jpg)

### Japanese

The game can be played in Japanese: texts, menus and party chat, taken from the original Japanese version, whose texts are already in the European game data. Choose "Japanese" (日本語) in Settings > Game > Language. The language changes at once, even in the middle of a game. In Japanese, the party windows have no portrait, so the names use the full width.

![The game in Japanese](docs/images/japanese.jpg)

### HD sound from the mobile version

Settings > Game > HD sound:
- **Mobile** (the default) plays the smartphone version's recordings instead of the DS synthesizer: music in Ogg Vorbis 44.1 kHz stereo, sound effects in ADPCM. Loops, volumes and fades stay the same.
- **DS** brings back the original sound.

They come from the Android version's data file (`.obb`), extracted when the game is built (see [Building](#building)).

### PC settings

The same screen, in the game's style, opens from "Settings" in the adventure log menu and, during a game, from **Misc > Settings** (it replaces the DS's three sliders). L / R switch between the four pages. Left / right change the selected line.

![The settings in the Misc menu](docs/images/settings.jpg)

- **Game**:
  - text speed: Slow, Normal, Fast, Instant;
  - battle speed, x1 to x4: the whole battle runs faster, the music keeps its tempo;
  - battle messages, music and sound effects (during a game);
  - volume, HD sound, language;
  - FPS display.
- **Display**:
  - fullscreen and window size;
  - frames per second: 60, 120, 144, 165, 240 or unlimited;
  - camera zoom;
  - wide (the screen's or window's shape, from 4:3 to 21:9 — 2.4:1 — with bars beyond) or DS screens, stretching;
  - battle display: Large (default: monsters zoomed to fill the battle area), Panorama (zoom set below), or the DS's two original screens;
  - battle zoom (Panorama battles): 100%, 125%, 150% or 175% (default), the monsters seen closer;
  - vertical sync (Windows; on Linux the compositor syncs the picture), 3D quality, smooth textures (No: the original DS pixels, without upscaling or filtering).
- **Keyboard** and **Gamepad**: the key or button for each action (A, B, X, Y, L, R, Start, Select, directions, party chat); the gamepad's triggers (LT / RT) can be bound too. Press A on a line, then the new key or button; Escape cancels. "Default controls" restores the page.

On laptops with two graphics chips, the game asks Windows for the dedicated one (NVIDIA / AMD). On Linux it uses the system's default; to run it on an NVIDIA chip: `__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ./dist/DragonQuestIV`.

### Achievements

The 80 achievements of [RetroAchievements' set](https://retroachievements.org/game/9770) are earned in the game itself: when one is unlocked, a notification with its badge, its title and its description slides in at the bottom right of the screen. **Misc > Achievements** during a game, or "Achievements" in the adventure log menu, lists them on two pages (L / R): those unlocked and those still missing, with their badges and descriptions. Titles and descriptions follow the game's language (French, German, Italian and Spanish use the names of the official translations; Japanese shows the English ones). They are kept in `saves\achievements.ini`, for every adventure log. The badges are in `addons\achievements`.

### More

- **Quitting**: "Quit game" in the adventure log menu, or at the bottom of Settings > Game, closes the game (keyboard or gamepad). After "End adventure" at a church, the "turn the power off" screen closes the game on A, B or Start.
- **Cheats**: **Misc > Cheats** during a game (its first tab; the debug tools follow) (from [OMGitsMatt45's Debug Addon](https://github.com/OMGitsMatt45/DQIV-Recompiled-Debug-Addon)). Left / right change the switches: invincible in battle, no random battles, guaranteed critical hits, EXP and gold multipliers (x1 to x64); they are kept in `settings.ini`, not in the save, and start off. A runs the actions: maximum gold, bank balance, casino coins or mini medals; heal and revive the whole party; add an item (left / right picks it) in a chosen quantity to the bag; reset the cheats. Gold, items and recovery stay in the adventure once it is saved. `docs/cheats-testing.md` and `tests/cheats` hold its checks.
- **Quick save anywhere**: Misc > Quick save works everywhere, dungeons included, except in the middle of an action (rope, fall, stairs). The game then goes on; on the DS it stopped. Resuming the interrupted adventure from the adventure log menu brings you back to the exact same spot.
- **Shops**: the buy list has tabs at the top: every item first, then the categories the shop sells (weapons, armour, shields, helmets, accessories, items); L / R switch tabs. Every item has its HD icon next to its name.
- **Menus over the world**: the scenery stays on the whole wide picture, frozen as on the DS, and the windows float over it. There are no black bars or stacked DS screens.
- **Two-screen scenes** (logos, casino, monster book, adventure log menus): both screens are placed side by side and enlarged. When one screen stays empty, the other is shown alone at twice the size.
- **Transitions**: the shattering effect when a battle starts covers the whole wide picture. The ending's staff roll scrolls from one screen to the other without a break.
- **Wireless**: the DS radio is emulated with no other console in range (tag mode, exchanges). The game behaves like a single DS and stays fully playable.


## Building

The project contains no game data: it comes from your own copies of the game. Put them in the `ROMandOBBfileHere` folder:

| File | |
| --- | --- |
| Your **European Dragon Quest IV ROM** for Nintendo DS (game code `YIVP`): the `.nds` file, or a `.zip` that contains it | required to play (on Linux, the game can be compiled without it) |
| The **Android version's data file** (`main.xxxxx.com.square_enix.android_googleplay.dq4.obb`) | optional: HD music and sound effects; without it, the game uses the DS sound |

### Windows

You also need:
- Visual Studio 2026 or 2022 (the free Community edition is enough) with the **Desktop development with C++** workload, which brings the Windows SDK and CMake (with Visual Studio 2026, CMake 4.2 or later: the one it comes with is);
- Internet access on the first build: SDL3 and stb_vorbis are downloaded and checked.

Then double-click **`build.bat`**, or run in a terminal:

```bash
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

The window stays open at the end: it shows `Created dist\DragonQuestIV.exe`, or an `ERROR:` line saying what is missing (Visual Studio's C++ workload, CMake, the ROM). The first build takes a few minutes.

The script:
1. compiles the game;
2. if the `assets` folder is missing the game's data, extracts it from the ROM (the ROM's own file system, unchanged);
3. if it is missing the HD sound and an `.obb` is there, extracts its sounds;
4. packs everything into `dist\DragonQuestIV.exe`.

The extraction is done only once: later builds reuse `assets`. To redo it, delete `assets`.

The build is 32-bit (Win32), because the original game resources contain 32-bit pointers; it runs on 64-bit Windows. Intermediate files go to `build32`, which can be deleted afterwards: only `dist\DragonQuestIV.exe` matters.

### Linux

The game is built as a 32-bit Linux program (for the same reason: the game resources contain 32-bit pointers) with GCC, CMake and Ninja; it runs on 64-bit Linux. You need:
- CMake 3.24 or later, Ninja, GCC with 32-bit (multilib) support, and `unzip` or `bsdtar` if your ROM is in a `.zip`;
- the 32-bit versions of the libraries SDL3 uses for the window, OpenGL, input and sound (X11 and Wayland, Mesa/GLVND, PulseAudio/PipeWire and ALSA, udev, D-Bus);
- fontconfig and a TrueType font (the interface text: FPS display);
- Internet access on the first build: SDL3, stb_vorbis and stb_truetype are downloaded and checked.

On **Arch Linux** (tested), with the `[multilib]` repository enabled:

```bash
sudo pacman -S --needed cmake ninja gcc unzip fontconfig \
    lib32-glibc lib32-gcc-libs lib32-mesa lib32-libglvnd \
    lib32-libx11 lib32-libxext lib32-libxrandr lib32-libxcursor lib32-libxi lib32-libxfixes lib32-libxss lib32-libxtst \
    lib32-wayland lib32-libxkbcommon lib32-libdecor lib32-libpulse lib32-alsa-lib lib32-systemd lib32-dbus
```

The headers come from the matching 64-bit packages (`libx11`, `wayland`, ...), which a desktop system already has. Other distributions (not tested): install the equivalent 32-bit (i386 / i686) development packages, for example `g++-multilib` and the `:i386` `-dev` packages on Debian and Ubuntu, or the `.i686` `-devel` packages on Fedora. A library missing at build time only leaves its feature out of SDL (for example Wayland or PulseAudio).

Then run, from the project folder:

```bash
./build.sh
```

It ends with `Created dist/DragonQuestIV`, or an `ERROR:` line saying what is missing (CMake, Ninja, 32-bit compiler support). It does the same steps as `build.ps1` (compile, extract `assets` once from the ROM and `.obb`, pack), with the `linux-game` CMake preset and `cmake/linux-i686.cmake`. Intermediate files go to `build32`; only `dist/DragonQuestIV` matters.

Without a ROM in `ROMandOBBfileHere` (and no `assets` extracted earlier), `dist/DragonQuestIV` holds only the PC version's additions, with an empty `dist/ROMandOBBfileHere` folder beside it: the layout of a release. The game then reads the ROM and `.obb` put in that folder each time it starts (an `.nds`, not a `.zip`). To pack the data into the executable instead, put the ROM (and the `.obb`) in the project's `ROMandOBBfileHere` and run `build.sh` again.

To play, the computer needs the same 32-bit libraries at run time (not their headers); systems with Steam installed usually have them. C++ and GCC's own libraries are built into the executable.

#### 64-bit build (experimental)

On x86-64 the game can also be built as a 64-bit program, which needs no 32-bit libraries, neither to build nor to play. You need CMake 3.24 or later, Ninja, GCC, `unzip` or `bsdtar` if your ROM is in a `.zip`, fontconfig and a TrueType font, and the ordinary (64-bit) development packages of the libraries SDL3 uses (X11 and Wayland, Mesa/GLVND, PulseAudio/PipeWire and ALSA, udev, D-Bus), which a desktop system usually has. On Arch Linux:

```bash
sudo pacman -S --needed cmake ninja gcc unzip fontconfig mesa libglvnd \
    libx11 libxext libxrandr libxcursor libxi libxfixes libxss libxtst \
    wayland libxkbcommon libdecor libpulse alsa-lib systemd dbus
```

Then run, from the project folder:

```bash
./build.sh 64
```

It writes `dist/DragonQuestIV` like the 32-bit build (it replaces a 32-bit one there), with the `linux64-game` CMake preset; intermediate files go to `build64`. Saves are the same in both builds.

The original game keeps addresses in 32-bit integers and in its files, so the 64-bit program keeps the game's memory below 2 GiB (`native/LowMemory.hpp`): at start it restarts itself once without address randomization, so that its heap begins right after the executable. It has been tested through the opening, towns, the world map, battles and the menus of chapter 1; the later chapters have not been played with it yet.

#### Automatic builds

GitHub builds the three versions (Linux 32-bit and 64-bit, Windows) without the game's data on every push and pull request: see the repository's Actions tab, where each run's builds can be downloaded for 14 days. The Linux builds are made on Ubuntu 22.04, so they start on systems with glibc 2.35 or later. Pushing a version tag (`git tag 1.9 && git push fork 1.9`) also packs `DragonQuestIV-linux32-1.9.zip`, `DragonQuestIV-linux64-1.9.zip` and `DragonQuestIV-win32-1.9.zip` into a draft release, to check and publish by hand (`.github/workflows/build.yml`).

#### Debug tools (for testing)

**Misc > Cheats** also holds debug tools, as more tabs after the cheats (L / R turns them). They are built in by default; `./build.sh nodebug` (or `./build.sh 64 nodebug`) leaves them out. Apart from what the cheats do, they hold:

- **Warp**: Zoom to any of the Zoom spell's 29 places; go to any of the game's 544 maps by its code (from the developers' list, by area), at its first entrance; walk through walls (towns, dungeons, world map); switch day and night (as the Day-Night spell); give or take the wagon, the ship and the balloon (they are moored where Zoom lands).
- **Battle**: fight 1 to 8 of any monster, on the ground of the place where you stand.
- **Party**: set a member's level (the level's stats and spells), teach a member every spell of their list, add or remove any character.
- **Story**: start a chapter from its beginning; view and flip any global, area or local story flag (this can break an adventure: use a copy of the save).
- **Tools**: an overlay with the map code, the chapter, the time of day, the position and the last message's number; play any music track or sound effect; fill the bestiary; start poker or a slot machine (from a town).

Once a debug tool has changed the game, no achievement is earned until the game is started again.

### Project contents

The source code's structure is described in [docs/architecture.md](docs/architecture.md).

| Folder / file | Contents |
| --- | --- |
| `game\` | the game's sources (decompilation) and the parts of the DS SDKs it uses |
| `native\` | the PC layer: window, OpenGL rendering, sound, files, settings |
| `addons\` | the PC version's additions: the party chat texts (English, French, German, Italian, Spanish) |
| `ROMandOBBfileHere\` | where you put your ROM and `.obb` (never added to git) |
| `assets\` | created by `build.ps1` or `build.sh`: the game's data extracted from your ROM and `.obb` (never added to git) |
| `icon\` | the icons: `DragonQuestIV-DS.ico` (the DS banner icon, built into the Windows executable) and `DragonQuestIV.png` (the README picture) |
| `docs\images\` | the pictures of this page |
| `cmake\` | placement of the game's modules (DS overlays); the 32-bit Linux toolchain (`linux-i686.cmake`) and the step that places the modules with GCC (`overlay-object.sh`) |
| `build.bat` | double-click to run `build.ps1` |
| `build.ps1` | builds the game and creates `dist\DragonQuestIV.exe` |
| `build.sh` | Linux: builds the game and creates `dist/DragonQuestIV` (`./build.sh 64`: the 64-bit build; `./build.sh nodebug`: without the debug tools) |
