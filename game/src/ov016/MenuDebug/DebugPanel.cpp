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
        rows_[rowCount_++] = ROW_MONSTER;
        rows_[rowCount_++] = ROW_MONSTER_COUNT;
        rows_[rowCount_++] = ROW_FIGHT;
        break;
    case PAGE_PARTY:
        rows_[rowCount_++] = ROW_MEMBER;
        rows_[rowCount_++] = ROW_LEVEL;
        rows_[rowCount_++] = ROW_SPELLS;
        rows_[rowCount_++] = ROW_CHARACTER;
        rows_[rowCount_++] = ROW_JOINED;
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
    if (row == ROW_WALLS || row == ROW_WAGON || row == ROW_SHIP || row == ROW_BALLOON) {
        pc::debugState().used = true; // no achievements now (DebugTools.hpp)
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
    case ROW_MONSTER: monster_ = wrap(monster_, step, 1, pc::debugLastMonster()); break;
    case ROW_MONSTER_COUNT: monsterCount_ = clamp(monsterCount_ + step, 1, 8); break;
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
    if (row != ROW_MUSIC && row != ROW_SOUND && row != ROW_OVERLAY) {
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
        pc::debugStartBattle(monster_, monsterCount_);
        closeMenus_ = true;
        return;
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
    // L and R turn the pages.
    int page = page_;
    if (dss::g_Pad.edge() & PAD_BUTTON_R) {
        page = (page + 1) % PAGE_COUNT;
    }
    if (dss::g_Pad.edge() & PAD_BUTTON_L) {
        page = (page + PAGE_COUNT - 1) % PAGE_COUNT;
    }
    if (page != page_) {
        page_ = page;
        doneRow_ = -1;
        buildRows();
        item.active_ = 0;
        item.result_ = 0;
        item.lastresult_ = 0;
        redraw = 1;
        return false;
    }
    // Held left / right step through the long lists (maps, monsters, flags).
    int step = 0;
    if (dss::g_Pad.repeat() & PAD_KEY_LEFT) {
        step = -1;
    }
    if (dss::g_Pad.repeat() & PAD_KEY_RIGHT) {
        step = 1;
    }
    bool leave = false;
    const int row = rows_[item.active_];
    switch (item.result_) {
    case 2:
        item.result_ = 0;
        item.lastresult_ = 0;
        if (row == ROW_BACK) {
            leave = true;
        } else {
            run(row);
            leave = closeMenus_;
        }
        redraw = 1;
        break;
    case 3:
        item.result_ = 0;
        item.lastresult_ = 0;
        leave = true;
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
    const char* pages[PAGE_COUNT];
    for (int page = 0; page < PAGE_COUNT; page++) {
        pages[page] = s_pages[page];
    }
    unkfunc_native_tabs(pages, PAGE_COUNT, page_, x, w, frame);
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
        case ROW_MONSTER:
            label = "Monster";
            snprintf(value, sizeof(value), "%d %s", monster_, pc::debugMonsterName(monster_).c_str());
            break;
        case ROW_MONSTER_COUNT: label = "How many"; snprintf(value, sizeof(value), "%d", monsterCount_); break;
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
