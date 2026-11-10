#ifdef DQIV_DEBUG_MENU
#include "ov016/MenuDebug/DebugPanel.hpp"
#include "ov016/MenuTemplate/MenuTemplate_materiel.hpp"
#include "ov016/UnkMaterielMenuDraw/MaterielMenuDraw.hpp"
#include "main/menu/MenuPartsDraw.hpp"
#include "main/status/StageStatus.hpp"
#include "main/dss/Pad.hpp"
#include "DebugTools.hpp"
#include <nitro/pad.h>
#include <stdio.h>
#include <string.h>

namespace {
// Texts in the game's font: the menu API takes pointers to strings in the
// program's memory. The debug menu is in English.
char s_label[16][48];
char s_value[16][48];
MenuPartsEntry s_left[] = {
    { 0x0d, 0x08, (short)0xf000, 0, 0, 0, 0xd0, 0xe },
    { 0xff, 0x00, 0, 0, 0, 0, 0, 0 },
};
MenuPartsEntry s_right[] = {
    { 0x0d, 0x0a, (short)0xf000, 0, 0, 0, 0xd0, 0xe },
    { 0xff, 0x00, 0, 0, 0, 0, 0, 0 },
};
void text(MenuPartsEntry* parts, char* line, int x, int y)
{
    int param = (int)line;
    parts[0].x_ = x;
    parts[0].y_ = y;
    drawMenuParts(parts, &param, 0, 0, 1);
}
void copy(char* destination, int size, const char* source)
{
    strncpy(destination, source, size - 1);
    destination[size - 1] = 0;
}
// Steps through [first, last], wrapping around.
int wrap(int value, int step, int first, int last)
{
    value += step;
    if (value < first) return last;
    if (value > last) return first;
    return value;
}
int clamp(int value, int first, int last)
{
    return value < first ? first : value > last ? last : value;
}
const char* yesNo(bool value) { return value ? "Yes" : "No"; }
const char* const s_pages[DebugPanel::PAGE_COUNT] = { "Warp", "Battle", "Party", "Story", "Tools" };
const char* const s_flagSets[pc::debugFlagSetCount] = { "Global", "Area", "Local" };
const char* const s_minigames[pc::debugMinigameCount] = { "Poker", "Slots (1)", "Slots (10)", "Slots (100)" };
const char* const s_times[] = { "-", "Morning", "Day", "Evening", "Night" };
const char* const s_stats[pc::debugStatCount] = { "Level", "HP", "MP", "Attack", "Defence", "Agility", "EXP", "Gold" };
const char* const s_memberStats[pc::debugMemberStatCount] = {
    "HP", "Max HP", "MP", "Max MP", "Strength", "Agility", "Resilience", "Wisdom", "Luck",
};
}

void DebugPanel::open()
{
    page_ = PAGE_WARP;
    doneRow_ = -1;
    closeMenus_ = false;
    member_ = 0;
    level_ = pc::debugLevel(0);
    buildRows();
}

void DebugPanel::showPage(int page, const char* const* tabs, int tabCount)
{
    page_ = page;
    tabs_ = tabs;
    tabCount_ = tabCount;
    doneRow_ = -1;
    buildRows();
}

const char* DebugPanel::pageTitle(int page)
{
    return s_pages[page];
}

void DebugPanel::openPage(int page, int row, menu::MenuItem& item)
{
    page_ = page;
    doneRow_ = -1;
    buildRows();
    item.active_ = 0;
    for (int i = 0; i < rowCount_; i++) {
        if (rows_[i] == row) item.active_ = i;
    }
}

void DebugPanel::back(menu::MenuItem& item)
{
    if (page_ == PAGE_GROUP) {
        openPage(PAGE_BATTLE, ROW_GROUP + group_, item);
    } else {
        openPage(PAGE_PARTY, ROW_EDIT_MEMBER, item);
    }
}

void DebugPanel::buildRows()
{
    rowCount_ = 0;
    switch (page_) {
    case PAGE_WARP:
        rows_[rowCount_++] = ROW_LOCATION;
        rows_[rowCount_++] = ROW_MAP_AREA;
        rows_[rowCount_++] = ROW_MAP;
        rows_[rowCount_++] = ROW_WALLS;
        rows_[rowCount_++] = ROW_DAY_NIGHT;
        rows_[rowCount_++] = ROW_WAGON;
        rows_[rowCount_++] = ROW_SHIP;
        rows_[rowCount_++] = ROW_BALLOON;
        break;
    case PAGE_BATTLE:
        // Select Battle, as the 1.8.1 mod's: four groups, A on one opens its stats.
        for (int row = ROW_GROUP; row <= ROW_GROUP_LAST; row++) rows_[rowCount_++] = row;
        rows_[rowCount_++] = ROW_MONSTER_COUNT;
        rows_[rowCount_++] = ROW_BACKGROUND;
        rows_[rowCount_++] = ROW_ARENA;
        rows_[rowCount_++] = ROW_BATTLE_INFO;
        rows_[rowCount_++] = ROW_TILE;
        rows_[rowCount_++] = ROW_RESET_BATTLE;
        rows_[rowCount_++] = ROW_FIGHT;
        break;
    case PAGE_GROUP:
        rows_[rowCount_++] = ROW_GROUP_HEAD;
        for (int row = ROW_STAT; row <= ROW_STAT_LAST; row++) rows_[rowCount_++] = row;
        rows_[rowCount_++] = ROW_RESET_STATS;
        break;
    case PAGE_PARTY:
        rows_[rowCount_++] = ROW_MEMBER;
        rows_[rowCount_++] = ROW_LEVEL;
        rows_[rowCount_++] = ROW_SPELLS;
        rows_[rowCount_++] = ROW_EDIT_MEMBER;
        rows_[rowCount_++] = ROW_MAX_STATS;
        rows_[rowCount_++] = ROW_CHARACTER;
        rows_[rowCount_++] = ROW_JOINED;
        break;
    case PAGE_MEMBER:
        rows_[rowCount_++] = ROW_MEMBER;
        for (int row = ROW_MEMBER_STAT; row <= ROW_MEMBER_STAT_LAST; row++) rows_[rowCount_++] = row;
        rows_[rowCount_++] = ROW_CONDITION;
        rows_[rowCount_++] = ROW_POISON;
        rows_[rowCount_++] = ROW_HEAL;
        break;
    case PAGE_STORY:
        rows_[rowCount_++] = ROW_CHAPTER;
        rows_[rowCount_++] = ROW_FLAG_SET;
        rows_[rowCount_++] = ROW_FLAG;
        rows_[rowCount_++] = ROW_FLAG_BLOCK;
        rows_[rowCount_++] = ROW_FLAG_VALUE;
        break;
    case PAGE_TOOLS:
        rows_[rowCount_++] = ROW_OVERLAY;
        rows_[rowCount_++] = ROW_MUSIC;
        rows_[rowCount_++] = ROW_SOUND;
        rows_[rowCount_++] = ROW_BESTIARY;
        rows_[rowCount_++] = ROW_MINIGAME;
        break;
    }
    rows_[rowCount_++] = ROW_BACK;
}

int DebugPanel::step() const
{
    return rowCount_ <= 8 ? 16 : rowCount_ <= 12 ? 11 : 10;
}

void DebugPanel::execute(menu::MenuItem& item)
{
    MenuTemplate_materiel::MATERIEL_SETTINGS(&item, rowCount_, step());
}

void DebugPanel::change(int row, int step)
{
    doneRow_ = -1;
    if (row == ROW_WALLS || row == ROW_WAGON || row == ROW_SHIP || row == ROW_BALLOON || row == ROW_CONDITION
        || row == ROW_POISON || (row >= ROW_MEMBER_STAT && row <= ROW_MEMBER_STAT_LAST)) {
        pc::debugState().used = true; // no achievements now (DebugTools.hpp)
    }
    auto& state = pc::debugState();
    pc::DebugGroup& group = state.groups[group_];
    // Numbers move faster while left / right is held; the lists, by 10 at most.
    const int fast = step * pc::debugHeldStep(held_);
    const int listStep = step * (pc::debugHeldStep(held_) < 10 ? pc::debugHeldStep(held_) : 10);
    if (row >= ROW_GROUP && row <= ROW_GROUP_LAST) {
        // How many is then this group's: the cursor passes the others on its way there.
        group_ = row - ROW_GROUP;
        pc::DebugGroup& chosen = state.groups[group_];
        chosen.monster = wrap(chosen.monster + listStep - step, step, 1, pc::debugLastMonster());
        while (pc::debugBlankMonster(chosen.monster)) chosen.monster = wrap(chosen.monster, step, 1, pc::debugLastMonster());
        if (chosen.count == 0) chosen.count = 1; // choosing a monster makes the group
        for (int& stat : chosen.stats) stat = -1;
        return;
    }
    if (row >= ROW_STAT && row <= ROW_STAT_LAST) {
        const int stat = row - ROW_STAT;
        const int value = group.stats[stat] >= 0 ? group.stats[stat] : pc::debugMonsterStat(group.monster, stat);
        group.stats[stat] = clamp(value + fast, stat == pc::debugStatLevel || stat == pc::debugStatHp ? 1 : 0, pc::debugStatLimit(stat));
        return;
    }
    if (row >= ROW_MEMBER_STAT && row <= ROW_MEMBER_STAT_LAST) {
        const int stat = row - ROW_MEMBER_STAT;
        pc::debugSetMemberStat(member_, stat, pc::debugMemberStat(member_, stat) + fast);
        return;
    }
    const auto& maps = pc::debugMaps();
    switch (row) {
    case ROW_LOCATION: location_ = wrap(location_, step, 0, pc::debugLocationCount - 1); break;
    case ROW_MAP: map_ = wrap(map_, step, 0, (int)maps.size() - 1); break;
    case ROW_MAP_AREA: {
        // The first map of the next (or previous) area.
        if (maps.empty()) break;
        const std::string area = maps[map_].area;
        int at = map_;
        do {
            at = wrap(at, step, 0, (int)maps.size() - 1);
        } while (at != map_ && maps[at].area == area);
        while (at > 0 && maps[at - 1].area == maps[at].area) at--;
        map_ = at;
        break;
    }
    case ROW_WALLS: pc::debugState().walkThroughWalls = !pc::debugState().walkThroughWalls; break;
    case ROW_WAGON: pc::debugToggleVehicle(pc::debugWagon); break;
    case ROW_SHIP: pc::debugToggleVehicle(pc::debugShip); break;
    case ROW_BALLOON: pc::debugToggleVehicle(pc::debugBalloon); break;
    case ROW_MONSTER_COUNT:
        // The first group is always there.
        group.count = clamp(group.count + step, group_ == 0 ? 1 : 0, 8);
        if (group.count && !group.monster) group.monster = 1;
        break;
    case ROW_GROUP_HEAD: group_ = wrap(group_, step, 0, pc::debugGroupCount - 1); break;
    case ROW_BACKGROUND:
        state.battleMap = wrap(state.battleMap, step, pc::debugRandomMap, (int)pc::debugBattleMaps().size() - 1);
        break;
    case ROW_ARENA: state.arenaMatch = wrap(state.arenaMatch, step, -1, pc::debugArenaMatchCount - 1); break;
    case ROW_BATTLE_INFO: state.battleInfo = !state.battleInfo; break;
    case ROW_TILE: tile_ = wrap(tile_ + listStep - step, step, -1, 255); break;
    case ROW_CONDITION: pc::debugToggleMemberDead(member_); break;
    case ROW_POISON: pc::debugToggleMemberPoison(member_); break;
    case ROW_MEMBER:
        member_ = wrap(member_, step, 0, pc::debugPartySize() - 1);
        level_ = pc::debugLevel(member_);
        break;
    case ROW_LEVEL: level_ = clamp(level_ + step, 1, 99); break;
    case ROW_CHARACTER: {
        const int count = (int)pc::debugCharacters().size();
        if (count) character_ = wrap(character_, step, 0, count - 1);
        break;
    }
    case ROW_CHAPTER: chapter_ = wrap(chapter_, step, 1, 5); break;
    case ROW_FLAG_SET: flagSet_ = wrap(flagSet_, step, 0, pc::debugFlagSetCount - 1); break;
    case ROW_FLAG: flag_ = wrap(flag_, step, 0, pc::debugFlagCount - 1); break;
    case ROW_FLAG_BLOCK: flag_ = (flag_ + step * 32 + pc::debugFlagCount) % pc::debugFlagCount; break;
    case ROW_OVERLAY: pc::debugState().overlay = !pc::debugState().overlay; break;
    case ROW_MUSIC: music_ = wrap(music_, step, 1, pc::debugMusicCount); break;
    case ROW_SOUND: {
        const int count = (int)pc::debugSoundEffects().size();
        if (count) sound_ = wrap(sound_, step, 0, count - 1);
        break;
    }
    case ROW_MINIGAME: minigame_ = wrap(minigame_, step, 0, pc::debugMinigameCount - 1); break;
    }
}

void DebugPanel::run(int row)
{
    done_ = "Done";
    if (row != ROW_MUSIC && row != ROW_SOUND && row != ROW_OVERLAY && row != ROW_BATTLE_INFO) {
        pc::debugState().used = true; // no achievements now (DebugTools.hpp)
    }
    switch (row) {
    case ROW_LOCATION:
        pc::debugWarpToLocation(location_);
        closeMenus_ = true;
        return;
    case ROW_MAP:
    case ROW_MAP_AREA:
        if (!pc::debugMaps().empty()) {
            pc::debugEnterMap(pc::debugMaps()[map_].name);
            closeMenus_ = true;
        }
        return;
    case ROW_DAY_NIGHT:
        pc::debugSwitchDayNight();
        closeMenus_ = true;
        return;
    case ROW_FIGHT:
        pc::debugStartBattle();
        closeMenus_ = true;
        return;
    case ROW_TILE:
        if (pc::debugStartTileBattle(tile_)) {
            closeMenus_ = true;
            return;
        }
        done_ = "No encounters";
        break;
    case ROW_RESET_BATTLE: {
        auto& state = pc::debugState();
        for (auto& group : state.groups) group = {};
        state.groups[0] = {1, 1};
        state.battleMap = pc::debugOwnMap;
        state.arenaMatch = -1;
        group_ = 0;
        break;
    }
    case ROW_RESET_STATS:
        for (int& stat : pc::debugState().groups[group_].stats) stat = -1;
        break;
    case ROW_BACKGROUND:
        return; // left / right choose it
    case ROW_ARENA:
        pc::debugState().arenaMatch = -1;
        break;
    case ROW_MAX_STATS:
        pc::debugMaxAllStats();
        break;
    case ROW_HEAL:
        pc::debugHealMember(member_);
        break;
    case ROW_LEVEL:
        pc::debugSetLevel(member_, level_);
        break;
    case ROW_SPELLS:
        pc::debugLearnAllSpells(member_);
        break;
    case ROW_JOINED:
        if (!pc::debugCharacters().empty()) {
            pc::debugToggleMember(pc::debugCharacters()[character_]);
            member_ = 0;
            level_ = pc::debugLevel(0);
        }
        break;
    case ROW_CHAPTER:
        pc::debugJumpToChapter(chapter_);
        closeMenus_ = true;
        return;
    case ROW_FLAG_VALUE:
        pc::debugFlipFlag(flagSet_, flag_);
        break;
    case ROW_MUSIC:
        pc::debugPlayMusic(music_);
        break;
    case ROW_SOUND:
        if (!pc::debugSoundEffects().empty()) pc::debugPlaySound(pc::debugSoundEffects()[sound_]);
        break;
    case ROW_BESTIARY:
        pc::debugCompleteBestiary();
        break;
    case ROW_MINIGAME:
        if (pc::debugStartMinigame(minigame_)) {
            closeMenus_ = true;
            return;
        }
        done_ = "In a town";
        break;
    default:
        // A switch: A changes it too.
        change(row, 1);
        return;
    }
    doneRow_ = row;
}

bool DebugPanel::update(menu::MenuItem& item, int& redraw)
{
    const int before = item.active_;
    item.execInput();
    if (item.active_ != before) {
        redraw = 1;
        doneRow_ = -1;
    }
    // Held left / right step through the long lists (maps, monsters, flags).
    int step = 0;
    if (dss::g_Pad.repeat() & PAD_KEY_LEFT) {
        step = -1;
    }
    if (dss::g_Pad.repeat() & PAD_KEY_RIGHT) {
        step = 1;
    }
    if (dss::g_Pad.edge() & (PAD_KEY_LEFT | PAD_KEY_RIGHT)) {
        held_ = 0;
    } else if (step != 0) {
        held_++;
    }
    bool leave = false;
    const int row = rows_[item.active_];
    switch (item.result_) {
    case 2:
        item.result_ = 0;
        item.lastresult_ = 0;
        if (row == ROW_BACK || row == ROW_GROUP_HEAD) {
            if (page_ >= PAGE_COUNT) back(item);
            else leave = true;
        } else if (row >= ROW_GROUP && row <= ROW_GROUP_LAST) {
            group_ = row - ROW_GROUP;
            openPage(PAGE_GROUP, ROW_GROUP_HEAD, item);
        } else if (row == ROW_EDIT_MEMBER) {
            openPage(PAGE_MEMBER, ROW_MEMBER, item);
        } else {
            run(row);
            leave = closeMenus_;
        }
        redraw = 1;
        break;
    case 3:
        item.result_ = 0;
        item.lastresult_ = 0;
        if (page_ >= PAGE_COUNT) {
            back(item);
            redraw = 1;
        } else {
            leave = true;
        }
        break;
    case 5:
        item.active_ = rowCount_ - 1;
        break;
    case 6:
        item.active_ = 0;
        break;
    }
    if (step != 0 && row != ROW_BACK) {
        change(row, step);
        redraw = 1;
    }
    return leave;
}

void DebugPanel::draw(menu::MenuItem& item, int x, int w, void (*frame)(int, int, int, int, int))
{
    // The cheats window's tabs: the cheats' pages, then these pages.
    const int tab = page_ == PAGE_GROUP ? PAGE_BATTLE : page_ == PAGE_MEMBER ? PAGE_PARTY : page_;
    unkfunc_native_tabs(tabs_, tabCount_, tabCount_ - PAGE_COUNT + tab, x, w, frame);
    const auto& state = pc::debugState();
    const auto& battleMaps = pc::debugBattleMaps();
    frame(x, 0x18, w, 0xc0 - 0x18, -1);
    const auto& maps = pc::debugMaps();
    const auto& characters = pc::debugCharacters();
    for (int i = 0; i < rowCount_; i++) {
        const int row = rows_[i];
        const int y = 32 + i * step();
        const char* label = "";
        char value[48] = "";
        bool action = false; // A runs it: its value is a choice, not a switch
        switch (row) {
        case ROW_LOCATION:
            label = "Zoom to";
            copy(value, sizeof(value), pc::debugLocationName(location_).c_str());
            action = true;
            break;
        case ROW_MAP_AREA:
            label = "Map area";
            copy(value, sizeof(value), maps.empty() ? "-" : maps[map_].area.c_str());
            break;
        case ROW_MAP:
            label = "Go to map";
            snprintf(value, sizeof(value), "%s (%d/%d)", maps.empty() ? "-" : maps[map_].name.c_str(), map_ + 1, (int)maps.size());
            action = true;
            break;
        case ROW_WALLS: label = "Walk through walls"; copy(value, sizeof(value), yesNo(pc::debugState().walkThroughWalls)); break;
        case ROW_DAY_NIGHT: {
            label = "Day / night";
            const int time = (int)g_Stage.getTimeZone();
            copy(value, sizeof(value), s_times[time >= 0 && time <= 4 ? time : 0]);
            action = true;
            break;
        }
        case ROW_WAGON: label = "Wagon"; copy(value, sizeof(value), yesNo(pc::debugHasVehicle(pc::debugWagon))); break;
        case ROW_SHIP: label = "Ship"; copy(value, sizeof(value), yesNo(pc::debugHasVehicle(pc::debugShip))); break;
        case ROW_BALLOON: label = "Balloon"; copy(value, sizeof(value), yesNo(pc::debugHasVehicle(pc::debugBalloon))); break;
        case ROW_GROUP_HEAD:
        case ROW_GROUP:
        case ROW_GROUP + 1:
        case ROW_GROUP + 2:
        case ROW_GROUP_LAST: {
            static char groupLabel[16];
            const int index = row == ROW_GROUP_HEAD ? group_ : row - ROW_GROUP;
            const pc::DebugGroup& group = state.groups[index];
            snprintf(groupLabel, sizeof(groupLabel), "Group %d", index + 1);
            label = groupLabel;
            if (group.count > 0) {
                snprintf(value, sizeof(value), "%d %s x%d", group.monster, pc::debugMonsterName(group.monster).c_str(), group.count);
            } else {
                copy(value, sizeof(value), "Nothing");
            }
            break;
        }
        case ROW_MONSTER_COUNT:
            label = "How many";
            snprintf(value, sizeof(value), "%d (group %d)", state.groups[group_].count, group_ + 1);
            break;
        case ROW_BACKGROUND:
            label = "Background";
            copy(value, sizeof(value), state.battleMap == pc::debugRandomMap ? "Random"
                : state.battleMap == pc::debugOwnMap ? "The place's own" : battleMaps[state.battleMap].c_str());
            break;
        case ROW_ARENA:
            label = "Arena match";
            if (state.arenaMatch < 0) {
                copy(value, sizeof(value), "Random");
            } else {
                snprintf(value, sizeof(value), "%d: %s", state.arenaMatch + 1, pc::debugArenaMatchName(state.arenaMatch).c_str());
            }
            break;
        case ROW_BATTLE_INFO: label = "Battle info"; copy(value, sizeof(value), yesNo(state.battleInfo)); break;
        case ROW_TILE:
            label = "Tile battle";
            if (tile_ >= 0) {
                snprintf(value, sizeof(value), "Tile %d", tile_);
            } else if (pc::debugTile() >= 0) {
                snprintf(value, sizeof(value), "Here (tile %d)", pc::debugTile());
            } else {
                copy(value, sizeof(value), "Here (none)");
            }
            action = true;
            break;
        case ROW_RESET_BATTLE: label = "Reset all"; action = true; break;
        case ROW_RESET_STATS: label = "Reset stats"; action = true; break;
        case ROW_EDIT_MEMBER: label = "Edit member"; action = true; break;
        case ROW_MAX_STATS: label = "Max out all stats"; action = true; break;
        case ROW_CONDITION: label = "Condition"; copy(value, sizeof(value), pc::debugMemberDead(member_) ? "Dead" : "Alive"); break;
        case ROW_POISON: label = "Poison"; copy(value, sizeof(value), pc::debugMemberPoisoned(member_) ? "On" : "Off"); break;
        case ROW_HEAL: label = "Fully heal"; action = true; break;
        case ROW_FIGHT: label = "Start battle"; action = true; break;
        case ROW_MEMBER: label = "Member"; copy(value, sizeof(value), pc::debugMemberName(member_).c_str()); break;
        case ROW_LEVEL:
            label = "Set level";
            snprintf(value, sizeof(value), "%d (now %d)", level_, pc::debugLevel(member_));
            action = true;
            break;
        case ROW_SPELLS: label = "Learn all spells"; action = true; break;
        case ROW_CHARACTER:
            label = "Character";
            copy(value, sizeof(value), characters.empty() ? "-" : pc::debugCharacterName(characters[character_]).c_str());
            break;
        case ROW_JOINED:
            label = "In the party";
            copy(value, sizeof(value), characters.empty() ? "-" : yesNo(pc::debugInParty(characters[character_])));
            action = true;
            break;
        case ROW_CHAPTER: label = "Start chapter"; snprintf(value, sizeof(value), "%d", chapter_); action = true; break;
        case ROW_FLAG_SET: label = "Flags"; copy(value, sizeof(value), s_flagSets[flagSet_]); break;
        case ROW_FLAG: label = "Flag"; snprintf(value, sizeof(value), "%d (0x%03x)", flag_, flag_); break;
        case ROW_FLAG_BLOCK: label = "Flag (by 32)"; snprintf(value, sizeof(value), "%d", flag_ / 32); break;
        case ROW_FLAG_VALUE: label = "Flag value"; copy(value, sizeof(value), pc::debugFlag(flagSet_, flag_) ? "On" : "Off"); action = true; break;
        case ROW_OVERLAY: label = "Position overlay"; copy(value, sizeof(value), yesNo(pc::debugState().overlay)); break;
        case ROW_MUSIC: label = "Play music"; snprintf(value, sizeof(value), "%d", music_); action = true; break;
        case ROW_SOUND:
            label = "Play sound";
            snprintf(value, sizeof(value), "%d", pc::debugSoundEffects().empty() ? 0 : pc::debugSoundEffects()[sound_]);
            action = true;
            break;
        case ROW_BESTIARY: label = "Complete bestiary"; action = true; break;
        case ROW_MINIGAME: label = "Start minigame"; copy(value, sizeof(value), s_minigames[minigame_]); action = true; break;
        case ROW_BACK: label = "Back"; break;
        }
        if (row >= ROW_STAT && row <= ROW_STAT_LAST) {
            // The group's own value, or the one that replaces it (*).
            const int stat = row - ROW_STAT;
            const pc::DebugGroup& group = state.groups[group_];
            label = s_stats[stat];
            if (group.stats[stat] >= 0) {
                snprintf(value, sizeof(value), "%d *", group.stats[stat]);
            } else {
                snprintf(value, sizeof(value), "%d", pc::debugMonsterStat(group.monster ? group.monster : 1, stat));
            }
        }
        if (row >= ROW_MEMBER_STAT && row <= ROW_MEMBER_STAT_LAST) {
            label = s_memberStats[row - ROW_MEMBER_STAT];
            snprintf(value, sizeof(value), "%d", pc::debugMemberStat(member_, row - ROW_MEMBER_STAT));
        }
        if (row == doneRow_) {
            copy(value, sizeof(value), done_);
        } else if (action && !value[0] && i == item.active_) {
            copy(value, sizeof(value), "A");
        }
        copy(s_label[i], sizeof(s_label[i]), label);
        text(s_left, s_label[i], 0x18, y);
        if (value[0]) {
            const bool chosen = i == item.active_ && row != doneRow_;
            snprintf(s_value[i], sizeof(s_value[i]), chosen ? "< %s >" : "%s", value);
            text(s_right, s_value[i], 0x18, y);
        }
    }
    item.drawActive();
}
#endif
