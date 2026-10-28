#include "DebugTools.hpp"
#ifdef DQIV_DEBUG_MENU
#include "GameServices.hpp"
#include "main/Commands/CommonCommand.hpp"
#include "main/cmn/ExtraMapLink.hpp"
#include "main/cmn/NonBattleActionManager.hpp"
#include "main/encount/Encount.hpp"
#include "main/global/Global.hpp"
#include "main/global/GlobalDQ4.hpp"
#include "main/global/GlobalGamePart.hpp"
#include "main/menu/MenuDataCommon.hpp"
#include "main/param/Param.hpp"
#include "main/sound/SoundManager.hpp"
#include "main/status/BattleResult.hpp"
#include "main/status/GameFlag.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/PlayerStatus.hpp"
#include "main/status/StageStatus.hpp"
#include "main/status/StoryStatus.hpp"
#include "main/text/TextAPI.hpp"
#include "ov000/Commands/TownCommand.hpp"
#include "ov000/town/TownPlayerManager.hpp"
#include "ov001/fld/FieldPlayerManager.hpp"
#include <cstdio>
#include <sstream>

namespace pc {
namespace {
std::string text(int type, int value)
{
    char buffer[128] = "";
    TextAPI::extractText(buffer, sizeof(buffer), type, value);
    buffer[sizeof(buffer) - 1] = 0;
    return buffer;
}
bool inTown() { return g_GlobalDQ4.unkfunc_02058114(TOWN_PART) != 0; }
bool inField() { return g_GlobalDQ4.unkfunc_02058114(FIELD_PART) != 0; }
void refreshParty()
{
    if (inTown()) TownPlayerManager::getSingleton()->resetParty();
    else if (inField()) FieldPlayerManager::getSingleton()->resetParty();
}
status::GameFlag& flagSet(int set)
{
    return set == debugAreaFlags ? g_AreaFlag : set == debugLocalFlags ? g_LocalFlag : g_GlobalFlag;
}
// The flags the game rebuilds the ship and the balloon from on each map.
constexpr int shipFlag = 0x13d, balloonFlag = 0x172;
}

DebugState& debugState()
{
    static DebugState state;
    return state;
}

std::string debugLocationName(int location)
{
    // The names the Zoom spell lists (for the current chapter).
    return text(0xc0000000, menu::MenuDataCommon::getRuraName(location) & 0xfffffff);
}
void debugWarpToLocation(int location)
{
    // As the Zoom spell: the town or the field takes the party there.
    g_Stage.setRuraFlag(1);
    g_Stage.setRuraTownID(location);
}

const std::vector<DebugMap>& debugMaps()
{
    static const std::vector<DebugMap> maps = [] {
        std::vector<DebugMap> list;
        const auto bytes = readGameFile("data/map/_list.txt");
        std::istringstream lines(std::string(bytes.begin(), bytes.end()));
        // "category,area,map" per line.
        for (std::string line; std::getline(lines, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            const auto first = line.find(','), last = line.rfind(',');
            if (first != std::string::npos && last > first && last + 1 < line.size())
                list.push_back({line.substr(first + 1, last - first - 1), line.substr(last + 1)});
        }
        return list;
    }();
    return maps;
}
void debugEnterMap(const std::string& map)
{
    // The map's first entrance, as a town exit leading there.
    cmn::g_extraMapLink.setExtraExitTown(map.c_str(), 0);
}

void debugSwitchDayNight()
{
    // As the Day-Night spell, once the menus are closed.
    g_Global.setRanarutaFlag(true);
    cmn::NonBattleActionManager::getSingleton()->setAction(cmn::ACTION_RANARUTA);
}

bool debugHasVehicle(int vehicle)
{
    switch (vehicle) {
    case debugWagon: return status::g_Party.basha_ != 0;
    case debugShip: return g_AreaFlag.check(shipFlag);
    case debugBalloon: return g_AreaFlag.check(balloonFlag);
    }
    return false;
}
void debugToggleVehicle(int vehicle)
{
    const bool on = !debugHasVehicle(vehicle);
    switch (vehicle) {
    case debugWagon:
        status::g_Party.basha_ = on;
        break;
    case debugShip:
        if (on) g_AreaFlag.set(shipFlag); else g_AreaFlag.remove(shipFlag);
        status::g_Party.ship_ = on;
        break;
    case debugBalloon:
        if (on) g_AreaFlag.set(balloonFlag); else g_AreaFlag.remove(balloonFlag);
        status::g_Party.balloon_ = on;
        break;
    }
}

int debugLastMonster()
{
    return static_cast<int>(sizeof(monsterDataTable.data) / sizeof(monsterDataTable.data[0])) - 1;
}
std::string debugMonsterName(int monster)
{
    return text(0x60000000, monster);
}
void debugStartBattle(int monster, int count)
{
    // One group of the chosen monster, on this place's battle ground.
    auto* encount = encount::Encount::getSingleton();
    g_Stage.updateBattleMap();
    encount->setChapter(status::g_Story.chapter_);
    encount->setPartyCount(status::g_Party.getCount());
    for (int i = 0; i < 4; i++) {
        encount->setMonsterIndex(i, i == 0 ? monster : 0);
        encount->setMonsterCount(i, i == 0 ? count : 0);
    }
    encount->setMonsterCountName();
    g_Global.startBattle();
}

int debugPartySize()
{
    status::g_Party.setPlayerMode();
    return status::g_Party.getCount();
}
std::string debugMemberName(int member)
{
    status::g_Party.setPlayerMode();
    return text(0x50000000, status::g_Party.getPlayerStatus(member)->haveStatusInfo_.haveStatus_.playerIndex_);
}
int debugLevel(int member)
{
    status::g_Party.setPlayerMode();
    return status::g_Party.getPlayerStatus(member)->haveStatusInfo_.haveStatus_.level_;
}
void debugSetLevel(int member, int level)
{
    // The developers' own level setter: the level's stats and spells.
    status::g_Party.setPlayerMode();
    status::g_Party.getPlayerStatus(member)->levelup(level);
}
void debugLearnAllSpells(int member)
{
    status::g_Party.setPlayerMode();
    auto& info = status::g_Party.getPlayerStatus(member)->haveStatusInfo_;
    for (int level = 1; level <= 99; level++) {
        info.haveAction_.levelup(info.index_, level, 255, true);
    }
    info.haveAction_.forceLearn();
}

const std::vector<int>& debugCharacters()
{
    // The hero (0) is always there.
    static std::vector<int> list;
    list.clear();
    for (int i = 1; i < 26; i++) {
        if (status::g_Party.isPlayer(i) || status::g_Party.isBattleNpc(i)) list.push_back(i);
    }
    return list;
}
std::string debugCharacterName(int character)
{
    return text(0x50000000, character);
}
bool debugInParty(int character)
{
    return (originalPlayerFlag_[character].flag_ & (0x100 | 0x1000)) != 0;
}
void debugToggleMember(int character)
{
    if (debugInParty(character)) status::g_Party.del(character);
    else status::g_Party.add(character);
    refreshParty();
}

void debugJumpToChapter(int chapter)
{
    // The game's own chapter start: its map, church, party and settings.
    int parameter = chapter;
    cmd_set_chapter(&parameter);
    cmn::g_extraMapLink.setExtraExitTown(g_Global.getMapName(), 0);
}
bool debugFlag(int set, int flag)
{
    return flagSet(set).check(flag);
}
void debugFlipFlag(int set, int flag)
{
    if (debugFlag(set, flag)) flagSet(set).remove(flag);
    else flagSet(set).set(flag);
}

void debugPlayMusic(int track)
{
    SoundManager::play(track, 0xf);
}
const std::vector<int>& debugSoundEffects()
{
    // The sound effects of the developers' list (data/debug/soundlist.dat).
    static const std::vector<int> sounds = [] {
        std::vector<int> list;
        const auto bytes = readGameFile("data/debug/soundlist.dat");
        std::istringstream lines(std::string(bytes.begin(), bytes.end()));
        for (std::string line; std::getline(lines, line);) {
            int id = 0;
            if (std::sscanf(line.c_str(), "SE,%d", &id) == 1) list.push_back(id);
        }
        return list;
    }();
    return sounds;
}
void debugPlaySound(int sound)
{
    SoundManager::playSe(sound, 0);
}
void debugCompleteBestiary()
{
    // Every monster of the book seen and defeated once.
    for (unsigned i = 0; i < 210; i++) {
        status::g_BattleResult.setEncount(i, 1);
        if (status::g_BattleResult.getMonsterCount(i) == 0) status::g_BattleResult.setMonsterCount(i, 1);
    }
}
bool debugStartMinigame(int game)
{
    if (!inTown()) return false;
    // As the casino's people: poker, or the 1, 10 and 100 coin slots.
    int parameters[2] = {game == debugPoker ? 0 : 1, game == debugPoker ? 0 : game - debugSlots1};
    cmd_mini_game(parameters);
    return true;
}

std::string debugOverlayText()
{
    if (!debugState().overlay) return {};
    char line[160];
    std::string result;
    const char* part = inTown() ? "town" : inField() ? "field" : g_GlobalDQ4.unkfunc_02058114(BATTLE_PART) ? "battle" : "other";
    std::snprintf(line, sizeof(line), "%s %s  chapter %d  time %d\n", part, g_Global.getMapName(), status::g_Story.chapter_,
        static_cast<int>(g_Stage.getTimeZone()));
    result += line;
    if (inTown() || inField()) {
        const auto position = inTown() ? TownPlayerManager::getSingleton()->getPosition()
                                       : FieldPlayerManager::getSingleton()->getPosition();
        std::snprintf(line, sizeof(line), "x %.2f  y %.2f  z %.2f\n", position.vx.value / 4096.0, position.vy.value / 4096.0,
            position.vz.value / 4096.0);
        result += line;
    }
    if (debugState().lastMessage >= 0) {
        std::snprintf(line, sizeof(line), "message %d\n", debugState().lastMessage);
        result += line;
    }
    return result;
}
}
#else
namespace pc {
std::string debugOverlayText() { return {}; }
}
#endif
