#pragma once
#include <string>
#include <vector>

// The debug menu's tools (Misc > Debug), in builds made with DQIV_DEBUG_MENU
// (./build.sh debug). Nothing here overlaps the cheats (Cheats.hpp).
namespace pc {
struct DebugState {
    bool walkThroughWalls = false;
    bool overlay = false;
    int lastMessage = -1; // the last message shown, for the overlay
    // Once a debug tool changed the game, no achievement is earned until the
    // game restarts (a filled bestiary or a level would unlock some).
    bool used = false;
};
DebugState& debugState();

// Warps. The actions that change the scene act once the menus are closed.
inline constexpr int debugLocationCount = 29; // the Zoom spell's destinations
std::string debugLocationName(int location);
void debugWarpToLocation(int location);
// Every map of the game, from the developers' list (data/map/_list.txt),
// with its area (the first letters of the name).
struct DebugMap {
    std::string area, name;
};
const std::vector<DebugMap>& debugMaps();
void debugEnterMap(const std::string& map);
void debugSwitchDayNight();
enum DebugVehicle {debugWagon, debugShip, debugBalloon, debugVehicleCount};
bool debugHasVehicle(int vehicle);
void debugToggleVehicle(int vehicle);

// A battle against a chosen monster.
int debugLastMonster();
std::string debugMonsterName(int monster);
void debugStartBattle(int monster, int count);

// The party.
int debugPartySize();
std::string debugMemberName(int member);
int debugLevel(int member);
void debugSetLevel(int member, int level);
void debugLearnAllSpells(int member);
// Characters who can join, and whether they are in the party.
const std::vector<int>& debugCharacters();
std::string debugCharacterName(int character);
bool debugInParty(int character);
void debugToggleMember(int character);

// The story.
void debugJumpToChapter(int chapter);
enum DebugFlagSet {debugGlobalFlags, debugAreaFlags, debugLocalFlags, debugFlagSetCount};
inline constexpr int debugFlagCount = 1024;
bool debugFlag(int set, int flag);
void debugFlipFlag(int set, int flag);

// Tools.
inline constexpr int debugMusicCount = 48;
void debugPlayMusic(int track);
const std::vector<int>& debugSoundEffects();
void debugPlaySound(int sound);
void debugCompleteBestiary();
enum DebugMinigame {debugPoker, debugSlots1, debugSlots10, debugSlots100, debugMinigameCount};
// False when the game is not in a town (minigames start from a town).
bool debugStartMinigame(int game);
// The position overlay's text (empty when it is off).
std::string debugOverlayText();
}
