#!/usr/bin/env bash
# Builds the game on Linux and writes dist/DragonQuestIV, with every resource
# packed inside: the game's data (assets) and the PC version's additions (addons).
# When assets is incomplete, its data is extracted first from the files in
# ROMandOBBfileHere: the European DS ROM (.nds, or a .zip holding it) and the
# Android version's data file (.obb, optional: HD sound).
# Without a ROM, the game is only compiled: dist/DragonQuestIV is written
# without the game's data, and reads it from the assets and addons folders
# beside it or in a folder above it; build again with the ROM to pack it.
# The game keeps its save, settings and journal in dist/saves.
# "./build.sh 64" builds a 64-bit program instead (x86-64; experimental), which
# needs no 32-bit libraries.
set -euo pipefail
cd "$(dirname "$0")"

fail() { echo; echo "ERROR: $*" >&2; exit 1; }

command -v cmake >/dev/null || fail 'CMake was not found. Install CMake 3.24 or later (Arch: pacman -S cmake; Debian/Ubuntu: apt install cmake).'
command -v ninja >/dev/null || fail 'Ninja was not found (Arch: pacman -S ninja; Debian/Ubuntu: apt install ninja-build).'
if [ "${1:-}" = 64 ]; then
    preset=linux64-game build=build64
else
    preset=linux-game build=build32
    # The game is 32-bit: the compiler needs the 32-bit C and C++ libraries.
    echo 'int main(){}' | ${CXX:-c++} -m32 -x c++ - -o /dev/null 2>/dev/null ||
        fail 'The compiler cannot build 32-bit programs. Install the 32-bit libraries (Arch, with [multilib] enabled: pacman -S lib32-glibc lib32-gcc-libs; Debian/Ubuntu: apt install g++-multilib).'
fi

cmake --preset $preset || fail 'Configuration failed (see the messages above)'
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
    cp $build/DragonQuestIV dist/DragonQuestIV || fail 'Copy failed'
    echo "Created dist/DragonQuestIV without the game's data: put your European Dragon Quest IV ROM (.nds, or a .zip holding it) in the $inbox folder and build again to add it."
fi
