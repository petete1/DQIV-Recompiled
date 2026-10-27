#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

namespace pc {
// The game's data comes from the player's own copies of the game: the
// European Nintendo DS ROM (required) and the Android version's data file
// (.obb, optional HD sound), extracted into the assets folder when the game
// is built (build.ps1, dqiv_pack), or read in place by the game.

// The ROM's four-letter game code ("YIVP" for the European Dragon Quest IV),
// empty when the file is not a Nintendo DS ROM.
std::string romGameCode(const std::filesystem::path& rom);
inline constexpr const char* europeanGameCode="YIVP";
// Where each file lies in the ROM or the data file: name -> {offset, size}.
struct FileRange {std::uint64_t offset=0,size=0;};
using FileRanges=std::map<std::string,FileRange>;
// The ROM's file system (data/...); throws on another ROM.
FileRanges romFileRanges(const std::filesystem::path& rom);
// The Android version's sounds, named data/SOUND/MOBILE/...; throws when the
// file has none.
FileRanges mobileSoundRanges(const std::filesystem::path& obb);
// Copies the ROM's file system (data/...) into a folder; throws on another ROM.
void extractRom(const std::filesystem::path& rom,const std::filesystem::path& folder);
// Copies the Android version's sounds (assets/sound in the .obb) into
// folder/data/SOUND/MOBILE; throws when the file has none.
void extractMobileSounds(const std::filesystem::path& obb,const std::filesystem::path& folder);
}
