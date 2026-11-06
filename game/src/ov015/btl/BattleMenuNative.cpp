// Classic battle menus: the battle menus' descriptions for the native windows
// (BattleWindows.hpp), their text drawn by the game's own font.
#ifdef DQIV_NATIVE
#include "ov015/btl/BattleMenuNative.hpp"
#include "main/dss/BgBuffer.hpp"
#include "main/dss/BgText.hpp"
#include "main/dss/Display.hpp"
#include "main/dss/Pad.hpp"
#include "nitro/pad.h"
#include "main/dss/DssUtils.hpp"
#include "main/dss/TextConvert.hpp"
#include "main/dss/VramRequest.hpp"
#include "main/menu/MenuUpdateAssist.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/PlayerStatus.hpp"
#include "ov003/status/MonsterPartyWithDraw.hpp"
#include "ov015/btl/BattleMenu.hpp"
#include "ov015/btl/BattleMenuPlayerControl.hpp"
#include "main/text/TextAPI.hpp"
#include "GameServices.hpp"
#include "Settings.hpp"
#include <map>
#include <utility>
#include <vector>

namespace {
pc::BattleMenuState s_building;
std::vector<int> s_keys, s_lastKeys;     // what the description shows, to tell when it changed
std::map<std::pair<int, int>, pc::BattleText> s_texts;
std::map<std::pair<int, int>, pc::BattleText> s_numbers;

// Draws UTF-8 text into a private 4-bit buffer (never the DS menu buffers).
pc::BattleText drawText(const char* text)
{
    enum { TILES_X = 40, TILES_Y = 3 };
    static unsigned char tiles[TILES_X * TILES_Y * 0x20];
    BgCharBuffer buffer;
    initCharBuffer(&buffer, tiles, TILES_X, TILES_Y);
    MI_CpuFill(0, tiles, sizeof(tiles));
    const int size = g_BgText.size_;
    setBgTextFontSize(12);
    drawBgText(&buffer, 0, 0, g_menuTextColors[0], text);
    pc::BattleText out;
    out.width = getBgTextWidth();
    if (out.width > TILES_X * 8) {
        out.width = TILES_X * 8;
    }
    out.height = getBgTextLineHeight(12) + 2;
    if (size == 10 || size == 12) {
        setBgTextFontSize(size);
    }
    // The ink, without the blank columns some names carry (the DS left room for an icon).
    auto ink = [&](int x, int y) {
        const unsigned char pair = tiles[((y >> 3) * TILES_X + (x >> 3)) * 0x20 + (y & 7) * 4 + (x & 7) / 2];
        return (unsigned char)((x & 1) ? pair >> 4 : pair & 15);
    };
    int left = out.width, right = 0;
    for (int y = 0; y < out.height; y++) {
        for (int x = 0; x < out.width; x++) {
            if (ink(x, y)) {
                left = x < left ? x : left;
                right = x + 1 > right ? x + 1 : right;
            }
        }
    }
    if (left >= right) {
        left = right = 0;
    }
    out.width = right - left;
    out.ink.resize(out.width * out.height);
    for (int y = 0; y < out.height; y++) {
        for (int x = 0; x < out.width; x++) {
            out.ink[y * out.width + x] = ink(left + x, y);
        }
    }
    return out;
}
// The text drawMenuText would draw for a value (MenuBgDraw.cpp).
void extractText(int value, char* buf, int size)
{
    char buf2[0x200];
    const char* text = (const char*)value;
    if (!pc::isGameTextPointer(static_cast<unsigned int>(value))) {
        unsigned int type = value & 0xf0000000;
        const unsigned int id = value & 0x0fffffff;
        if (type == 0) {
            type = 0x10000000;
        }
        const int ex = type == 0x60000000 && TextAPI::isGermanMonsterException(id) ? 2 : 0;
        g_text_extractor.extract_text(buf, size, type, id, ex, 0);
        if (type == 0x40000000 || type == 0x60000000) {
            translateTextPrefix(g_upperCaseCharTable, g_lowerCaseCharTable, buf2, sizeof(buf2), buf, 1);
            dss::strcpy_s(buf, size, buf2);
        }
        return;
    }
    if (text[0] == '@') {
        convertSjisToUtf8(buf, size, text + 1);
    } else if ((unsigned char)text[0] == 0xff && (unsigned char)text[1] == 0xfe) {
        convertUtf16ToUtf8(buf, size, text);
    } else {
        dss::strcpy_s(buf, size, text);
    }
}
unsigned int rgb(unsigned short c)
{
    const unsigned int r = (c & 31) * 255 / 31, g = ((c >> 5) & 31) * 255 / 31, b = ((c >> 10) & 31) * 255 / 31;
    return 0xff000000u | (r << 16) | (g << 8) | b;
}
}

bool classicBattleMenus()
{
    return pc::gameSettings().classicBattleMenus;
}

pc::BattleText battleText(int value)
{
    const std::pair<int, int> key(value, TextAPI::m_lang);
    auto found = s_texts.find(key);
    if (found != s_texts.end()) {
        return found->second;
    }
    char buf[0x400];
    buf[0] = 0;
    extractText(value, buf, sizeof(buf));
    return s_texts[key] = drawText(buf);
}

pc::BattleText battleNumber(int number)
{
    const std::pair<int, int> key(number, TextAPI::m_lang);
    auto found = s_numbers.find(key);
    if (found != s_numbers.end()) {
        return found->second;
    }
    char buf[0x40];
    char text[0x100];
    dss::sprintf_s(buf, sizeof(buf), "%d", number);
    translateText(g_fullWidthCharTable, g_asciiCharTable, text, sizeof(text), buf);
    return s_numbers[key] = drawText(text);
}

// "×n", as the monster name plates count a group.
pc::BattleText battleCount(int number)
{
    const std::pair<int, int> key(-1 - number, TextAPI::m_lang);
    auto found = s_numbers.find(key);
    if (found != s_numbers.end()) {
        return found->second;
    }
    char text[0x40];
    dss::sprintf_s(text, sizeof(text), "\xc3\x97%d", number);
    return s_numbers[key] = drawText(text);
}

void beginBattleMenu(pc::BattleMenuKind kind, int columns)
{
    s_building = pc::BattleMenuState();
    s_building.kind = kind;
    s_building.columns = columns;
    s_keys.clear();
    s_keys.push_back((int)kind);
    // The menu text colours (BG palette 15, as drawMenuText uses).
    const PaletteBuffer* palette = getBgPaletteBuffer(0);
    for (int i = 0; i < 16; i++) {
        s_building.palette[i] = rgb(palette->color_[0xf0 + i]);
        s_keys.push_back(palette->color_[0xf0 + i]);
    }
}

void setBattleTitle(int value)
{
    s_building.title = battleText(value);
    s_keys.push_back(value);
}

void setBattleFooter(int value, int number)
{
    if (value == 0) {
        return;
    }
    const pc::BattleText label = battleText(value), digits = battleNumber(number);
    // The label, a space, the number: one text.
    pc::BattleText footer;
    footer.width = label.width + 6 + digits.width;
    footer.height = label.height > digits.height ? label.height : digits.height;
    footer.ink.assign(footer.width * footer.height, 0);
    for (int y = 0; y < label.height; y++) {
        for (int x = 0; x < label.width; x++) {
            footer.ink[y * footer.width + x] = label.ink[y * label.width + x];
        }
    }
    for (int y = 0; y < digits.height; y++) {
        for (int x = 0; x < digits.width; x++) {
            footer.ink[y * footer.width + label.width + 6 + x] = digits.ink[y * digits.width + x];
        }
    }
    s_building.footer = footer;
    s_keys.push_back(value);
    s_keys.push_back(number);
}

void addBattleEntry(int text, short index, bool enabled, int right, int rightKind)
{
    pc::BattleMenuEntry entry;
    entry.label = battleText(text);
    if (rightKind == 1) {
        entry.right = battleNumber(right);
    } else if (rightKind == 2) {
        entry.right = battleCount(right);
    } else if (right != 0) {
        entry.right = battleText(right);
    }
    entry.index = index;
    entry.enabled = enabled;
    s_building.entries.push_back(entry);
    s_keys.push_back(text);
    s_keys.push_back(index);
    s_keys.push_back(enabled);
    s_keys.push_back(right);
    s_keys.push_back(rightKind);
}

void setBattlePage(int page, int pages)
{
    s_building.page = page;
    s_building.pages = pages;
    s_keys.push_back(page);
    s_keys.push_back(pages);
}

void setBattleCursor(short active)
{
    s_building.cursor = -1;
    for (int i = 0; i < (int)s_building.entries.size(); i++) {
        if (s_building.entries[i].index == active) {
            s_building.cursor = i;
        }
    }
    s_keys.push_back(s_building.cursor);
}

void endBattleMenu()
{
    pc::BattleMenuState& state = pc::battleMenuState();
    const unsigned int revision = state.revision + (s_keys != s_lastKeys || state.kind == pc::BattleMenuKind::none ? 1 : 0);
    state = s_building;
    state.revision = revision;
    s_lastKeys = s_keys;
}

namespace {
void activeMemberTitle()
{
    const int chara = btl::BattleMenuPlayerControl::getSingleton()->activeChara_;
    setBattleTitle(status::g_Party.getPlayerStatus(chara)->haveStatusInfo_.haveStatus_.playerIndex_ + 0x50000000);
}
}

void drawBattleEnemyTargets(const TOUCHRECT* rects, int count, int active)
{
    beginBattleMenu(pc::BattleMenuKind::targetEnemy, 1);
    activeMemberTitle();
    const int group = rects[active].group;
    int cursor = active;
    for (int i = 0; i < count; i++) {
        bool seen = false;
        for (int j = 0; j < i; j++) {
            seen = seen || rects[j].group == rects[i].group;
        }
        if (seen) {
            continue;
        }
        if (rects[i].group == group) {
            cursor = i;
        }
        const int left = g_monster.getMonsterCountInGroupExist(rects[i].group);
        addBattleEntry(0x60000000 + g_monster.getMonsterIndexInGroup(rects[i].group), (short)i, true, left > 1 ? left : 0, left > 1 ? 2 : 0);
    }
    setBattleCursor((short)cursor);
    endBattleMenu();
    drawMonsterNamePlates(group);
}

void drawBattlePartyTargets(int count, int active)
{
    beginBattleMenu(pc::BattleMenuKind::targetParty, 1);
    activeMemberTitle();
    for (int i = 0; i < count; i++) {
        addBattleEntry(status::g_Party.getPlayerStatus(i)->haveStatusInfo_.haveStatus_.playerIndex_ + 0x50000000, (short)i, true);
    }
    setBattleCursor((short)active);
    endBattleMenu();
}

void battleTargetInput(menu::MenuItem& item)
{
    if (!classicBattleMenus()) {
        return;
    }
    const pc::BattleMenuState& state = pc::battleMenuState();
    const int at = state.cursor;
    int to = -1;
    const pc::BattlePointer pointer = pc::takeBattlePointer();
    if (pointer.entry >= 0 && pointer.entry < (int)state.entries.size()) {
        to = pointer.entry;
    }
    const int keys = dss::g_Pad.repeat();
    const int direction = keys & PAD_KEY_UP ? 0 : keys & PAD_KEY_DOWN ? 1 : keys & PAD_KEY_LEFT ? 2 : keys & PAD_KEY_RIGHT ? 3 : -1;
    if (direction >= 0 && at >= 0) {
        to = pc::moveBattleCursor(state, at, direction);
        if (item.result_ != menu::MenuItem::MENUITEM_RESULT_OK && item.result_ != menu::MenuItem::MENUITEM_RESULT_CANCEL) {
            item.result_ = menu::MenuItem::MENUITEM_RESULT_NONE;   // the item's own move is not the list's
        }
    }
    if (to >= 0 && to != at && item.result_ != menu::MenuItem::MENUITEM_RESULT_OK) {
        // A rectangle of another group: the menu's "changed" code keeps it.
        item.active_ = state.entries[to].index;
        item.result_ = menu::MenuItem::MENUITEM_RESULT_CHANGE;
        item.reason_ = menu::MenuItem::MENUITEM_REASON_PAD;
    }
}

void clearBattleMenu()
{
    pc::battleMenuState().kind = pc::BattleMenuKind::none;
}

int battleMenuSelect(menu::MenuItem& item, CursorMoveBase& cursor)
{
    const pc::BattleMenuState& state = pc::battleMenuState();
    // The mouse on an entry (moved there or clicking it): the cursor follows.
    const pc::BattlePointer pointer = pc::takeBattlePointer();
    if (pointer.entry >= 0 && pointer.entry < (int)state.entries.size()) {
        item.active_ = state.entries[pointer.entry].index;
    }
    const int from = item.active_;
    // The item moves within its own grid and reports a direction only at its
    // edges (MenuItem::check60_PAD_DOWN...): the direction keys themselves say
    // where the windows' cursor goes.
    const int keys = dss::g_Pad.repeat();
    const int result = MenuUpdate_Assist::menuSelect(item, cursor);
    const int direction = keys & PAD_KEY_UP ? 0 : keys & PAD_KEY_DOWN ? 1 : keys & PAD_KEY_LEFT ? 2 : keys & PAD_KEY_RIGHT ? 3 : -1;
    if (direction < 0 || (result != 1 && (result < 4 || result > 7))) {
        return result;
    }
    int at = -1;
    for (int i = 0; i < (int)state.entries.size(); i++) {
        if (state.entries[i].index == from) {
            at = i;
        }
    }
    if (at < 0) {
        return result;
    }
    const int to = pc::moveBattleCursor(state, at, direction);
    if (to >= 0) {
        item.active_ = state.entries[to].index;
    }
    return result;
}
#endif
