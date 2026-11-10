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
#include "ov003/status/MonsterPartyWithDraw.hpp"
#include "ov016/casino/FightStadiumManager.hpp"
#include <chrono>
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
// The story battles' music for their monsters (the mod's table, from encounters 157-243).
constexpr unsigned short storyMusic[][2] = {
    {179, 31}, {171, 31}, {180, 11}, {192, 31}, {191, 31}, {190, 31}, {181, 11}, {194, 31}, {193, 31},
    {188, 32}, {143, 31}, {147, 31}, {146, 31}, {144, 31}, {174, 32}, {304, 32}, {300, 16}, {302, 16},
};
// The groups of the battle being started, by their place in it.
std::vector<DebugGroup> s_battleGroups;
void queueBattleMap()
{
    const auto& maps = debugBattleMaps();
    int map = debugState().battleMap;
    if (map == debugRandomMap && !maps.empty()) {
        map = static_cast<int>(std::chrono::steady_clock::now().time_since_epoch().count() % maps.size());
    }
    debugState().mapPending = map >= 0 && map < static_cast<int>(maps.size()) ? maps[map] : "";
}
status::BaseStatus& memberBase(int member)
{
    status::g_Party.setPlayerMode();
    return status::g_Party.getPlayerStatus(member)->haveStatusInfo_.haveStatus_.baseStatus_;
}
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
int debugMonsterStat(int monster, int stat)
{
    const auto& data = monsterDataTable.data[monster];
    switch (stat) {
    case debugStatLevel: return data.level;
    case debugStatHp: return data.HP;
    case debugStatMp: return data.MP;
    case debugStatAttack: return data.attack;
    case debugStatDefence: return data.defence;
    case debugStatAgility: return data.agility;
    case debugStatExp: return data.exp;
    case debugStatGold: return data.money;
    }
    return 0;
}
int debugStatLimit(int stat)
{
    // The sizes the monsters keep them in.
    return stat == debugStatLevel || stat == debugStatAgility ? 255 : stat == debugStatExp ? 9999999 : 65535;
}
void debugStartBattle()
{
    // The chosen groups, on this place's battle ground.
    auto& state = debugState();
    auto* encount = encount::Encount::getSingleton();
    g_Stage.updateBattleMap();
    encount->setChapter(status::g_Story.chapter_);
    encount->setPartyCount(status::g_Party.getCount());
    s_battleGroups.clear();
    // A form of a boss who changes form: his story battle, from that form (as the mod).
    // Psaro the Manslayer 174, 205-210 (his music changes at 210); the last boss 304-309 (at 306).
    const int boss = state.groups[0].monster;
    const int story = boss == 174 || (boss >= 205 && boss <= 210) ? 185 : boss >= 304 && boss <= 309 ? 241 : 0;
    if (story && state.groups[0].count > 0) {
        encount->setTileId(story); // the battle's scripts go by it
        encount->brew();
        encount->setMonsterIndex(0, boss);
        if (boss == 210 || boss >= 306) encount->encountParam_.sound_ = 0x21;
        s_battleGroups.push_back(state.groups[0]);
    } else {
        for (const auto& group : state.groups) {
            if (group.count > 0) s_battleGroups.push_back(group);
        }
        for (int i = 0; i < debugGroupCount; i++) {
            const bool used = i < static_cast<int>(s_battleGroups.size());
            encount->setMonsterIndex(i, used ? s_battleGroups[i].monster : 0);
            encount->setMonsterCount(i, used ? s_battleGroups[i].count : 0);
        }
        encount->setMonsterCountName();
        // An ordinary encounter's settings, with the music of a story battle's monster.
        encount->encountParam_.setup(0);
        for (const auto& group : s_battleGroups) {
            for (const auto& music : storyMusic) {
                if (music[0] == group.monster && !encount->encountParam_.sound_) encount->encountParam_.sound_ = music[1];
            }
        }
    }
    state.groupsPending = true;
    queueBattleMap();
    g_Global.startBattle();
}
int debugTile()
{
    return encount::Encount::getSingleton()->tileId_;
}
bool debugStartTileBattle(int tile)
{
    auto* encount = encount::Encount::getSingleton();
    if (tile < 0) tile = encount->tileId_;
    if (tile < 0) return false;
    encount->setTileId(tile);
    if (!encount->brew()) return false;
    queueBattleMap();
    g_Global.startBattle();
    return true;
}
const std::vector<std::string>& debugBattleMaps()
{
    static const std::vector<std::string> maps = [] {
        // "category,group,map" per line.
        std::vector<std::string> list;
        const auto bytes = readGameFile("data/btlmap/_list.txt");
        std::istringstream lines(std::string(bytes.begin(), bytes.end()));
        for (std::string line; std::getline(lines, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            const auto last = line.rfind(',');
            if (last != std::string::npos && last + 1 < line.size()) list.push_back(line.substr(last + 1));
        }
        return list;
    }();
    return maps;
}
std::string debugArenaMatchName(int match)
{
    const auto& card = FightStadiumManager::cardDataList_[match];
    std::string name;
    for (const int monster : {card.mobA, card.mobB, card.mobC, card.mobD}) {
        if (monster == -1) break;
        name += (name.empty() ? "" : ", ") + debugMonsterName(monster);
    }
    return name;
}
void debugApplyGroupStats()
{
    if (!debugState().groupsPending) return;
    debugState().groupsPending = false;
    const int count = g_monster.getCount();
    for (int i = 0; i < count; i++) {
        auto* monster = g_monster.getMonsterStatus(i);
        const int group = monster->characterGroup_;
        if (group < 0 || group >= static_cast<int>(s_battleGroups.size())) continue;
        const int* stats = s_battleGroups[group].stats;
        auto& have = monster->haveStatusInfo_.haveStatus_;
        auto& base = have.baseStatus_;
        if (stats[debugStatLevel] >= 0) have.level_ = stats[debugStatLevel];
        if (stats[debugStatHp] >= 0) base.hp_ = base.hpMax_ = stats[debugStatHp];
        if (stats[debugStatMp] >= 0) base.mp_ = base.mpMax_ = stats[debugStatMp];
        if (stats[debugStatAttack] >= 0) base.strength_ = stats[debugStatAttack];
        if (stats[debugStatDefence] >= 0) base.protection_ = stats[debugStatDefence];
        if (stats[debugStatAgility] >= 0) base.agility_ = stats[debugStatAgility];
        if (stats[debugStatExp] >= 0) have.exp_ = stats[debugStatExp];
        if (stats[debugStatGold] >= 0) have.gold_ = stats[debugStatGold];
    }
}
std::string debugTakeBattleMap()
{
    std::string map;
    map.swap(debugState().mapPending);
    return map;
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

int debugMemberStat(int member, int stat)
{
    const auto& base = memberBase(member);
    switch (stat) {
    case debugMemberHp: return base.hp_;
    case debugMemberHpMax: return base.hpMax_;
    case debugMemberMp: return base.mp_;
    case debugMemberMpMax: return base.mpMax_;
    case debugMemberStrength: return base.strength_;
    case debugMemberAgility: return base.agility_;
    case debugMemberResilience: return base.protection_;
    case debugMemberWisdom: return base.wisdom_;
    case debugMemberLuck: return base.luck_;
    }
    return 0;
}
void debugSetMemberStat(int member, int stat, int value)
{
    // The game's limits: 999 HP and MP, 255 for the others; HP and MP up to their maximum.
    auto& base = memberBase(member);
    const auto limit = [&](int high) { return static_cast<unsigned short>(value < 0 ? 0 : value > high ? high : value); };
    switch (stat) {
    case debugMemberHp: base.hp_ = limit(base.hpMax_); break;
    case debugMemberHpMax: base.hpMax_ = limit(999); if (!base.hpMax_) base.hpMax_ = 1; break;
    case debugMemberMp: base.mp_ = limit(base.mpMax_); break;
    case debugMemberMpMax: base.mpMax_ = limit(999); break;
    case debugMemberStrength: base.strength_ = limit(255); break;
    case debugMemberAgility: base.agility_ = static_cast<unsigned char>(limit(255)); break;
    case debugMemberResilience: base.protection_ = limit(255); break;
    case debugMemberWisdom: base.wisdom_ = static_cast<unsigned char>(limit(255)); break;
    case debugMemberLuck: base.luck_ = static_cast<unsigned char>(limit(255)); break;
    }
    if (base.hp_ > base.hpMax_) base.hp_ = base.hpMax_;
    if (base.mp_ > base.mpMax_) base.mp_ = base.mpMax_;
}
bool debugMemberDead(int member)
{
    return memberBase(member).hp_ == 0;
}
void debugToggleMemberDead(int member)
{
    auto& base = memberBase(member);
    base.hp_ = base.hp_ ? 0 : base.hpMax_;
    refreshParty();
}
bool debugMemberPoisoned(int member)
{
    status::g_Party.setPlayerMode();
    return status::g_Party.getPlayerStatus(member)->haveStatusInfo_.isPoison();
}
void debugToggleMemberPoison(int member)
{
    const bool poisoned = debugMemberPoisoned(member);
    auto& info = status::g_Party.getPlayerStatus(member)->haveStatusInfo_;
    if (poisoned) info.detoxPoison();
    else info.setPoison();
}
void debugHealMember(int member)
{
    auto& base = memberBase(member);
    status::g_Party.getPlayerStatus(member)->haveStatusInfo_.detoxPoison(); // memberBase chose the party's list
    base.hp_ = base.hpMax_;
    base.mp_ = base.mpMax_;
    refreshParty();
}
void debugMaxAllStats()
{
    for (int i = 1; i < 26; i++) {
        auto& have = originalPlayer_[i].haveStatusInfo_.haveStatus_;
        if (!have.playerIndex_) continue;
        auto& base = have.baseStatus_;
        base.strength_ = base.protection_ = 255;
        base.agility_ = base.wisdom_ = base.luck_ = 255;
        base.hp_ = base.hpMax_ = 999;
        base.mp_ = base.mpMax_ = 999;
    }
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

namespace {
void battleInfoLine(std::string& result, const std::string& name, status::HaveStatusInfo& info)
{
    char line[160];
    std::string shown = name;
    std::erase(shown, ';'); // the names' line break
    const auto& base = info.haveStatus_.baseStatus_;
    std::snprintf(line, sizeof(line), "%s  HP %d/%d  MP %d/%d  Atk %d  Def %d  Agi %d\n", shown.c_str(), base.hp_, base.hpMax_,
        base.mp_, base.mpMax_, info.getAttack(0), info.getDefence(0), info.getAgility(0));
    result += line;
}
}

std::string debugOverlayText()
{
    std::string result;
    if (debugState().battleInfo && g_GlobalDQ4.unkfunc_02058114(BATTLE_PART)) {
        // The fighters' stats: the party, then the monsters.
        const int members = status::g_Party.getCount();
        for (int i = 0; i < members && i < 4; i++) {
            auto& info = status::g_Party.getPlayerStatus(i)->haveStatusInfo_;
            battleInfoLine(result, text(0x50000000, info.haveStatus_.playerIndex_), info);
        }
        const int monsters = g_monster.getCount();
        for (int i = 0; i < monsters; i++) {
            auto& info = g_monster.getMonsterStatus(i)->haveStatusInfo_;
            battleInfoLine(result, debugMonsterName(info.haveStatus_.playerIndex_), info);
        }
    }
    if (!debugState().overlay) return result;
    char line[160];
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
