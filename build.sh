#!/usr/bin/env bash
# Builds the game on Linux and writes dist/DragonQuestIV, with every resource
# packed inside: the game's data (assets) and the PC version's additions (addons).
# When assets is incomplete, its data is extracted first from the files in
# ROMandOBBfileHere: the European DS ROM (.nds, or a .zip holding it) and the
# Android version's data file (.obb, optional: HD sound).
# Without a ROM, dist/DragonQuestIV holds only the PC version's additions and
# reads the game's data in place from the ROM and .obb that the player puts in
# dist/ROMandOBBfileHere (the layout of a release); build again with the ROM
# to pack it.
# The game keeps its save, settings and journal in dist/saves.
# "./build.sh 64" builds a 64-bit program instead (x86-64; experimental), which
# needs no 32-bit libraries. The debug tools (tabs of Misc > Cheats) are
# built in; "./build.sh nodebug" (or "./build.sh 64 nodebug") leaves them out.
set -euo pipefail
cd "$(dirname "$0")"

fail() { echo; echo "ERROR: $*" >&2; exit 1; }

command -v cmake >/dev/null || fail 'CMake was not found. Install CMake 3.24 or later (Arch: pacman -S cmake; Debian/Ubuntu: apt install cmake).'
command -v ninja >/dev/null || fail 'Ninja was not found (Arch: pacman -S ninja; Debian/Ubuntu: apt install ninja-build).'
wide= debug=ON
for argument in "$@"; do
    case $argument in
    64) wide=1 ;;
    nodebug) debug=OFF ;;
    *) fail "Unknown option: $argument (use 64 and/or nodebug)" ;;
    esac
done
if [ -n "$wide" ]; then
    preset=linux64-game build=build64
else
    preset=linux-game build=build32
    # The game is 32-bit: the compiler needs the 32-bit C and C++ libraries.
    echo 'int main(){}' | ${CXX:-c++} -m32 -x c++ - -o /dev/null 2>/dev/null ||
        fail 'The compiler cannot build 32-bit programs. Install the 32-bit libraries (Arch, with [multilib] enabled: pacman -S lib32-glibc lib32-gcc-libs; Debian/Ubuntu: apt install g++-multilib).'
fi

cmake --preset $preset -DDQIV_DEBUG_MENU=$debug || fail 'Configuration failed (see the messages above)'
cmake --build --preset $preset || fail 'Build failed (see the messages above)'
pack=$build/dqiv_pack
inbox=ROMandOBBfileHere

# The game's data, from the player's ROM.
if [ ! -f assets/data/SOUND/sound_data.sdat ]; then
    rom=$(find -L "$inbox" -maxdepth 1 -type f -iname '*.nds' | head -n 1)
    unzipped=
    if [ -z "$rom" ]; then
        zip=$(find -L "$inbox" -maxdepth 1 -type f -iname '*.zip' | head -n 1)
        if [ -n "$zip" ]; then
            unzipped=$build/rom
            rm -rf "$unzipped"; mkdir -p "$unzipped"
            if command -v unzip >/dev/null; then unzip -q "$zip" -d "$unzipped"
            elif command -v bsdtar >/dev/null; then bsdtar -xf "$zip" -C "$unzipped"
            else fail "Cannot open $(basename "$zip"): install unzip, or extract the .nds into $inbox"; fi
            rom=$(find "$unzipped" -type f -iname '*.nds' | head -n 1)
            [ -n "$rom" ] || fail "$(basename "$zip") contains no .nds ROM"
        fi
    fi
    if [ -n "$rom" ]; then
        echo "Extracting the game's data from $(basename "$rom")"
        "$pack" --rom "$rom" assets || fail 'ROM extraction failed'
        [ -z "$unzipped" ] || rm -rf "$unzipped"
    else
        echo "WARNING: No ROM in $inbox: the game is compiled without its data." >&2
    fi
fi

# HD sound, from the Android version's data file (optional).
if [ ! -f assets/data/SOUND/MOBILE/snd_vol.dat ]; then
    obb=$(find -L "$inbox" -maxdepth 1 -type f -iname '*.obb' | head -n 1)
    if [ -n "$obb" ]; then
        echo "Extracting the HD sound from $(basename "$obb")"
        "$pack" --obb "$obb" assets || fail 'HD sound extraction failed'
    else
        echo "WARNING: No .obb file in $inbox: the game will use the DS sound." >&2
    fi
fi

mkdir -p dist
if [ -f assets/data/SOUND/sound_data.sdat ]; then
    "$pack" $build/DragonQuestIV assets addons dist/DragonQuestIV || fail 'Packaging failed'
    echo 'Created dist/DragonQuestIV'
else
    "$pack" $build/DragonQuestIV addons dist/DragonQuestIV || fail 'Packaging failed'
    mkdir -p dist/$inbox
    cat > dist/$inbox/README.txt <<'TEXT'
Put your own copies of the game here before starting DragonQuestIV:

- Your European Dragon Quest IV ROM for Nintendo DS (game code YIVP): the
  .nds file (extracted from its .zip, if it came in one). Required.
- The data file of the Android version of Dragon Quest IV
  (main.xxxxx.com.square_enix.android_googleplay.dq4.obb). Optional: it gives
  the game the HD music and sound effects of the mobile version; without it,
  the game uses the DS sound.

The game reads them from here each time it starts; nothing is copied.
TEXT
    echo "Created dist/DragonQuestIV without the game's data: it reads the ROM and .obb put in dist/$inbox."
fi
