#pragma once
#include <string>
#include <vector>

// The debug menu's tools (Misc > Debug), in builds made with DQIV_DEBUG_MENU
// (./build.sh debug). Nothing here overlaps the cheats (Cheats.hpp).
namespace pc {
// Select Battle (the Battle page): up to four groups of monsters, each with
// stats that replace the monster's own (-1 keeps its own).
inline constexpr int debugGroupCount = 4;
enum DebugStat {debugStatLevel, debugStatHp, debugStatMp, debugStatAttack, debugStatDefence, debugStatAgility,
    debugStatExp, debugStatGold, debugStatCount};
struct DebugGroup {
    int monster = 0, count = 0;
    int stats[debugStatCount] = {-1, -1, -1, -1, -1, -1, -1, -1};
};
// Battle backgrounds: -1 the place's own, -2 a random one, else a line of debugBattleMaps().
inline constexpr int debugOwnMap = -1, debugRandomMap = -2;
struct DebugState {
    bool walkThroughWalls = false;
    bool overlay = false;
    int lastMessage = -1; // the last message shown, for the overlay
    // Once a debug tool changed the game, no achievement is earned until the
    // game restarts (a filled bestiary or a level would unlock some).
    bool used = false;
    DebugGroup groups[debugGroupCount] = {{1, 1}};
    int battleMap = debugOwnMap;
    int arenaMatch = -1; // the Monster Arena's next match, -1 a random one
    bool battleInfo = false; // the fighters' stats over the battle
    // Set when a debug battle starts, taken when it loads.
    bool groupsPending = false;
    std::string mapPending;
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

// A battle against the chosen groups, on the chosen background.
int debugLastMonster();
std::string debugMonsterName(int monster);
// Ids with no real monster (placeholders, blank entries), which the picker skips (as the mod's).
constexpr bool debugBlankMonster(int monster)
{
    return monster == 189 || (monster >= 195 && monster <= 204) || (monster >= 247 && monster <= 299)
        || (monster >= 310 && monster <= 314);
}
static_assert(debugBlankMonster(189) && debugBlankMonster(250) && !debugBlankMonster(174) && !debugBlankMonster(309));
int debugMonsterStat(int monster, int stat); // its own
int debugStatLimit(int stat);
void debugStartBattle();
// The battle's own encounter of a map tile (-1 the party's), false if it has none.
int debugTile();
bool debugStartTileBattle(int tile);
const std::vector<std::string>& debugBattleMaps();
inline constexpr int debugArenaMatchCount = 46;
std::string debugArenaMatchName(int match);
// For the battle: the groups' stats once its monsters are set up, the
// background when it loads its scenery ("" the place's own).
void debugApplyGroupStats();
std::string debugTakeBattleMap();
// Left / right held: steps of 1, then 10, 100 and 1000 (as the mod's Select Battle).
constexpr int debugHeldStep(int repeats)
{
    return repeats < 10 ? 1 : repeats < 25 ? 10 : repeats < 45 ? 100 : 1000;
}
static_assert(debugHeldStep(0) == 1 && debugHeldStep(9) == 1 && debugHeldStep(10) == 10 && debugHeldStep(25) == 100
    && debugHeldStep(45) == 1000);

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
// A member's stats, with the game's limits.
enum DebugMemberStat {debugMemberHp, debugMemberHpMax, debugMemberMp, debugMemberMpMax, debugMemberStrength,
    debugMemberAgility, debugMemberResilience, debugMemberWisdom, debugMemberLuck, debugMemberStatCount};
int debugMemberStat(int member, int stat);
void debugSetMemberStat(int member, int stat, int value);
bool debugMemberDead(int member);
void debugToggleMemberDead(int member);
bool debugMemberPoisoned(int member);
void debugToggleMemberPoison(int member);
void debugHealMember(int member);
// Every character's stats at the game's limits.
void debugMaxAllStats();

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
