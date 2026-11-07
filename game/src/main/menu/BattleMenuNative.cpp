// Classic battle and field menus: the menus' descriptions for the native windows
// (BattleWindows.hpp), their text drawn by the game's own font. In the main module:
// the field menus use it too, and the battle overlay's data is destroyed when it unloads.
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
#include "main/menu/MenuCommonDraw.hpp"
#include "main/menu/MenuIconData.hpp"
#include "main/menu/MessageWindow.hpp"
#include "main/menu/MenuDisplays.hpp"
#include "ov016/UnkTownMenuDraw/TownMenuDraw.hpp"
#include "ov016/TownMenu_ROOT/TownMenu_ROOT.hpp"
#include "ov016/TownMenuPlayerControl/TownMenuPlayerControl.hpp"
#include "ov016/status/PlayerItemInfo.hpp"
#include "ov016/status/FukuroItemInfo.hpp"
#include "main/menu/MenuDataCommon.hpp"
#include "main/menu/MenuTextDisplay.hpp"
#include "main/global/GlobalDQ4.hpp"
#include "main/global/GlobalGamePart.hpp"
#include "main/dss/UnkMemory.hpp"
#include "main/menu/MenuUpdateAssist.hpp"
#include "main/status/UseAction.hpp"
#include "main/status/UseItem.hpp"
#include "main/status/PartyStatus.hpp"
#include "main/status/CharacterStatus.hpp"
#include "main/status/PlayerStatus.hpp"
#include "ov003/status/MonsterPartyWithDraw.hpp"
#include "ov015/btl/BattleMenu.hpp"
#include "ov015/btl/BattleMenuPlayerControl.hpp"
#include "main/text/TextAPI.hpp"
#include "GameServices.hpp"
#include "Settings.hpp"
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {
pc::BattleMenuState s_building;
std::vector<int> s_keys, s_lastKeys;     // what the description shows, to tell when it changed
std::map<std::string, pc::BattleText> s_texts;
std::map<std::pair<int, int>, pc::BattleText> s_numbers;

// Draws UTF-8 text into a private 4-bit buffer (never the DS menu buffers).
pc::BattleText drawText(const char* text, int fontSize = 12)
{
    enum { TILES_X = 40, TILES_Y = 3 };
    static unsigned char tiles[TILES_X * TILES_Y * 0x20];
    BgCharBuffer buffer;
    initCharBuffer(&buffer, tiles, TILES_X, TILES_Y);
    MI_CpuFill(0, tiles, sizeof(tiles));
    const int size = g_BgText.size_;
    setBgTextFontSize(fontSize);
    drawBgText(&buffer, 0, 0, g_menuTextColors[0], text);
    pc::BattleText out;
    out.width = getBgTextWidth();
    if (out.width > TILES_X * 8) {
        out.width = TILES_X * 8;
    }
    out.height = getBgTextLineHeight(fontSize) + 2;
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
    return pc::classicBattleMenusOn(pc::gameSettings());
}

bool overlayMenus()
{
    return pc::gameSettings().battleMenus == 2;
}

pc::BattleText battleText(int value, int fontSize)
{
    // Cached by the text itself: a value can name something a save changes (the hero).
    char buf[0x400];
    buf[0] = 0;
    extractText(value, buf, sizeof(buf));
    const std::string key = std::string(fontSize == 10 ? "10:" : "12:") + buf;
    auto found = s_texts.find(key);
    if (found != s_texts.end()) {
        return found->second;
    }
    return s_texts[key] = drawText(buf, fontSize);
}

pc::BattleText battleNumber(int number, int fontSize)
{
    const std::pair<int, int> key(number, TextAPI::m_lang + (fontSize == 10 ? 100 : 0));
    auto found = s_numbers.find(key);
    if (found != s_numbers.end()) {
        return found->second;
    }
    char buf[0x40];
    char text[0x100];
    dss::sprintf_s(buf, sizeof(buf), "%d", number);
    translateText(g_fullWidthCharTable, g_asciiCharTable, text, sizeof(text), buf);
    return s_numbers[key] = drawText(text, fontSize);
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

namespace {
// A menu icon (side x side, 8x8 8-bit tiles) in the item icons' colours (MenuIconBg::loadPalettes), ARGB.
const std::vector<std::uint32_t>& menuIcon(int type, int index, int side)
{
    static std::map<std::pair<int, int>, std::vector<std::uint32_t> > s_icons;
    const std::pair<int, int> key(type, index);
    auto found = s_icons.find(key);
    if (found == s_icons.end()) {
        std::vector<std::uint32_t> pixels;
        void* packed = getMenuIconData(type, index);
        const unsigned short* colours = (const unsigned short*)getMenuIconData(0xf0000000, 9999);
        if (packed != NULL && colours != NULL && getUncompressedSize(packed) == (unsigned int)(side * side)) {
            const unsigned char* tiles = (const unsigned char*)uncompressAlloc(packed);
            const int across = side / 8;
            pixels.resize(side * side);
            for (int y = 0; y < side; y++) {
                for (int x = 0; x < side; x++) {
                    const unsigned char at = tiles[((y / 8) * across + x / 8) * 64 + (y % 8) * 8 + x % 8];
                    pixels[y * side + x] = at != 0 && at < 0xa0 ? rgb(colours[at]) : 0u;
                }
            }
            dssFree(&g_appHeap, (void*)tiles);
        }
        found = s_icons.insert(std::make_pair(key, pixels)).first;
    }
    return found->second;
}

void setEntryIcon(int type, int index, int side)
{
    if (s_building.entries.empty()) {
        return;
    }
    s_building.entries.back().icon = menuIcon(type, index, side);
    s_keys.push_back(type);
    s_keys.push_back(index);
}
}

pc::BattleText numbersText(const char* ascii, int fontSize);

void beginFieldMenu(pc::BattleMenuKind kind, int columns)
{
    beginBattleMenu(kind, columns);
    s_building.field = true;
    s_keys.push_back(-2);
}

void setFieldRootParent(int chosen)
{
    // The root's six commands, as drawRootMenuCommands lists them.
    for (int i = 0; i < 6; i++) {
        pc::BattleMenuEntry entry;
        entry.label = battleText(menu::MenuDataCommon::getRootMenuName(i));
        entry.icon = menuIcon(0xf4000000, menu::MenuDataCommon::getRootMenuKind(i), 24);
        entry.index = (short)i;
        s_building.parent.push_back(entry);
        s_keys.push_back(menu::MenuDataCommon::getRootMenuName(i));
    }
    s_building.parentCursor = chosen;
    s_keys.push_back(chosen);
}

void endFieldMenu(int rootCommand, const status::HaveStatusInfo* active)
{
    setFieldRootParent(rootCommand);
    setBattleGold(status::g_Party.gold_);
    endBattleMenu();
    publishFieldParty(active);
}

void setBattleGold(int amount)
{
    char buf[0x40];
    dss::sprintf_s(buf, sizeof(buf), "%d", amount);
    s_building.gold = numbersText(buf, 12);
    s_building.goldUnit = battleText(0xa000003c);
    s_keys.push_back(amount);
}

void setBattleEntryIcon(int icon)
{
    setEntryIcon(0xf4000000, icon, 24);
}

// A text of numbers ("1/3"), in the game's digits.
pc::BattleText numbersText(const char* ascii, int fontSize)
{
    const std::string key = std::string(fontSize == 10 ? "#10:" : "#12:") + ascii + char('0' + TextAPI::m_lang);
    auto found = s_texts.find(key);
    if (found != s_texts.end()) {
        return found->second;
    }
    char text[0x100];
    translateText(g_fullWidthCharTable, g_asciiCharTable, text, sizeof(text), ascii);
    return s_texts[key] = drawText(text, fontSize);
}

void setBattlePage(int page, int pages)
{
    s_building.page = page;
    s_building.pages = pages;
    if (pages > 1) {
        char buf[0x40];
        dss::sprintf_s(buf, sizeof(buf), "%d/%d", page + 1, pages);
        s_building.pageText = numbersText(buf, 12);
    }
    s_keys.push_back(page);
    s_keys.push_back(pages);
}

// The description window: the game's help message (as the DS shows under the list), in
// words the windows wrap to their width; the line breaks the DS needed are spaces again.
void setBattleHelp(int message)
{
    char buf[0x400];
    buf[0] = 0;
    extractText(message, buf, sizeof(buf));
    const bool spaced = std::strchr(buf, ' ') != NULL;
    std::string word;
    auto flush = [&]() {
        if (!word.empty()) {
            const std::string key = "10:" + word;
            auto found = s_texts.find(key);
            s_building.help.push_back(found != s_texts.end() ? found->second : (s_texts[key] = drawText(word.c_str(), 10)));
            word.clear();
        }
    };
    for (const char* c = buf; *c; c++) {
        if (*c == ' ' || (*c == '\n' && spaced)) {
            flush();
        } else if (*c == '\n') {
            flush();
            s_building.help.push_back(pc::BattleText());   // no spaces to wrap at (Japanese): keep the DS lines
        } else {
            word += *c;
        }
    }
    flush();
    s_keys.push_back(message);
}

// The cost line at the description's bottom: "MP" and "cost/the caster's MP".
void setBattleCost(int cost, int have)
{
    char buf[0x40];
    dss::sprintf_s(buf, sizeof(buf), "%d/%d", cost, have);
    s_building.costLabel = battleText(0xa0000002, 10);
    s_building.cost = numbersText(buf, 10);
    s_keys.push_back(cost);
    s_keys.push_back(have);
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
    // The party panel and the message window are published by others (any order of the menus).
    s_building.party.swap(state.party);
    s_building.portraitPalette = state.portraitPalette;
    s_building.message = state.message;
    state = s_building;
    state.revision = revision;
    s_lastKeys = s_keys;
}


void drawBattleEnemyTargets(const TOUCHRECT* rects, int count, int active, int title)
{
    beginBattleMenu(pc::BattleMenuKind::targetEnemy, 1);
    setBattleTitle(title);
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

void drawBattlePartyTargets(int count, int active, int title)
{
    beginBattleMenu(pc::BattleMenuKind::targetParty, 1);
    setBattleTitle(title);
    for (int i = 0; i < count; i++) {
        addBattleEntry(status::g_Party.getPlayerStatus(i)->haveStatusInfo_.haveStatus_.playerIndex_ + 0x50000000, (short)i, true);
    }
    setBattleCursor((short)active);
    endBattleMenu();
}

void battleListInput(menu::MenuItem& item)
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

namespace {
// The page of a 2×3 list (drawMagicMenu, drawItemList): its first entry and how many.
int pageEntries(int count, int page, int& top)
{
    const int pageMax = (count - 1) / 6;
    top = page * 6;
    return pageMax > page ? 6 : (count - 1) % 6 + 1;
}
}

// A spell's kind for its icon (DQ11's five), from what the game says it does.
pc::BattleSpellKind spellKind(int action)
{
    switch (status::UseAction::getDamageType(action)) {
    case status::UseAction::DamageTypeDamage:
    case status::UseAction::DamageTypeInstantDeath:
    case status::UseAction::DamageTypeMahosute:
    case status::UseAction::DamageTypeDragoram:
        return pc::BattleSpellKind::attack;
    case status::UseAction::DamageTypeRecovery:
    case status::UseAction::DamageTypeRebirth:
    case status::UseAction::DamageTypeAddMp:
    case status::UseAction::DamageTypeStatusClear:
        return pc::BattleSpellKind::healing;
    case status::UseAction::DamageTypeDefenceChange:
    case status::UseAction::DamageTypeAgilityChange:
        // Sap and Slow on the enemy, Kabuff and Acceleratle on the party.
        return status::UseAction::getUseType(action) == status::UseItem::Enemy ? pc::BattleSpellKind::debuff : pc::BattleSpellKind::buff;
    case status::UseAction::DamageTypeAstoron:
    case status::UseAction::DamageTypeBaikiruto:
    case status::UseAction::DamageTypeFubaha:
    case status::UseAction::DamageTypeMahokanta:
    case status::UseAction::DamageTypeDefence:
        return pc::BattleSpellKind::buff;
    case status::UseAction::DamageTypeSpazz:
    case status::UseAction::DamageTypeSleep:
    case status::UseAction::DamageTypeManusa:
    case status::UseAction::DamageTypeMahoton:
    case status::UseAction::DamageTypeConfusion:
    case status::UseAction::DamageTypePoison:
    case status::UseAction::DamageTypeSubMp:
    case status::UseAction::DamageTypeFizzleZone:
    case status::UseAction::DamageTypeTimeStop:
        return pc::BattleSpellKind::debuff;
    default:
        return pc::BattleSpellKind::utility;
    }
}

void drawBattleSpells(const int* actions, int count, int page, int active)
{
    // The chosen spell's description and MP cost in a window of their own.
    beginBattleMenu(pc::BattleMenuKind::spells, 2);
    setBattleTitle(0x80000005);
    int top;
    const int num = pageEntries(count, page, top);
    for (int i = 0; i < num; i++) {
        addBattleEntry(getActionNameMessage(actions[top + i]), (short)i, true);
        static std::vector<std::uint32_t> s_kinds[5];
        const int kind = (int)spellKind(actions[top + i]);
        if (s_kinds[kind].empty()) {
            s_kinds[kind] = pc::battleSpellIcon((pc::BattleSpellKind)kind);
        }
        s_building.entries.back().icon = s_kinds[kind];
        s_keys.push_back(kind);
    }
    setBattlePage(page, (count - 1) / 6 + 1);
    if (active >= 0 && active < num) {
        const int action = actions[top + active];
        setBattleHelp(status::UseAction::getMenuMessage(action));
        // The MP cost, unless it takes all the MP (0xff, 1000: drawMagicCommandHeader).
        const int mp = status::UseAction::getUseMp(action);
        if (mp > 0 && mp != 0xff && mp != 1000) {
            setBattleCost(mp, status::g_Party.getPlayerStatus(btl::BattleMenuPlayerControl::getSingleton()->activeChara_)->haveStatusInfo_.getMp());
        }
    }
    setBattleCursor((short)active);
    endBattleMenu();
}

void drawBattleItems(const int* items, int count, int page, int active)
{
    beginBattleMenu(pc::BattleMenuKind::items, 2);
    setBattleTitle(0x80000006);
    addBattleItems(items, count, page, active,
                   &status::g_Party.getPlayerStatus(btl::BattleMenuPlayerControl::getSingleton()->activeChara_)->haveStatusInfo_);
    endBattleMenu();
}

void addBattleItems(const int* items, int count, int page, int active, status::HaveStatusInfo* owner)
{
    int top;
    const int num = count > 0 ? pageEntries(count, page, top) : 0;
    for (int i = 0; i < num; i++) {
        // The equipped mark where drawBattleItemEntry puts it.
        addBattleEntry(items[top + i] + 0x40000000, (short)i, true, owner != NULL && owner->haveItem_.isEquipment(i + top) ? 0xa0000041 : 0);
        setEntryIcon(0xf0000000, items[top + i], 32);   // the item's icon, as drawBattleItemEntry
    }
    setBattlePage(page, count > 0 ? (count - 1) / 6 + 1 : 1);
    if (active >= 0 && active < num) {
        setBattleHelp(status::UseItem::getMenuMessage(items[top + active]));
    }
    setBattleCursor((short)active);
}

void drawBattleUseOrEquip(int item, int active)
{
    beginBattleMenu(pc::BattleMenuKind::items, 1);
    setBattleTitle(item + 0x40000000);
    addBattleEntry(0x80000069, 0, true);
    addBattleEntry(0x8000006b, 1, true);
    setBattleCursor((short)active);
    endBattleMenu();
}

namespace {
std::vector<const status::HaveStatusInfo*> s_acted;
}

void markBattleActed(status::CharacterStatus* chara)
{
    if (chara != NULL) {
        s_acted.push_back(&chara->haveStatusInfo_);
    }
}

void clearBattleActed()
{
    s_acted.clear();
}

namespace {
// The faces the DS status windows show (MenuFaceDisplay: 48x48, 8-bit, their own 160 colours).
void loadPortraitPalette(pc::BattleMenuState& state)
{
    const unsigned short* faceColours = (const unsigned short*)getMenuIconData(0xf5000000, 9999);
    for (int i = 0; i < 256; i++) {
        state.portraitPalette[i] = faceColours != NULL && i < 0xa0 ? rgb(faceColours[i]) : 0xff000000u;
    }
}

// A party card: label "Lv." (or the member's condition, level 0 then: none shown).
pc::BattlePartyRow partyRow(status::HaveStatusInfo* info, int hp, int mp, int colour, int label, bool showLevel)
{
    static std::map<int, std::vector<std::uint8_t> > s_portraits;
    pc::BattlePartyRow row;
    row.name = battleText(info->haveStatus_.playerIndex_ + 0x50000000);
    // The card's small text in the game's 10-dot font, as the DS status windows.
    row.label = battleText(label, 10);
    if (showLevel) {
        row.level = battleNumber(info->haveStatus_.isBattleNpc_ ? 0 : info->haveStatus_.level_, 10);
    }
    row.hpValue = hp;
    row.hpMax = info->getHpMax();
    row.mpValue = mp;
    row.mpMax = info->getMpMax();
    row.hp = battleNumber(row.hpValue, 10);
    row.mp = battleNumber(row.mpValue, 10);
    row.hpLabel = battleText(0xa0000001, 10);
    row.mpLabel = battleText(0xa0000002, 10);
    row.colour = colour;
    const int icon = info->haveStatus_.iconIndex_;
    auto found = s_portraits.find(icon);
    if (found == s_portraits.end()) {
        std::vector<std::uint8_t> pixels;
        const unsigned char* data = (const unsigned char*)getMenuIconData(0xf5000000, icon);
        if (data != NULL) {
            // 6 rows of 6 tiles of 8x8 (MenuFaceDisplay::loadIcon copies a row of 0x180 bytes at a time).
            pixels.resize(48 * 48);
            for (int y = 0; y < 48; y++) {
                for (int x = 0; x < 48; x++) {
                    pixels[y * 48 + x] = data[(y / 8) * 0x180 + (x / 8) * 64 + (y % 8) * 8 + x % 8];
                }
            }
            found = s_portraits.insert(std::make_pair(icon, pixels)).first;
        }
    }
    if (found != s_portraits.end()) {
        row.portrait = found->second;
    }
    return row;
}
}

void publishBattleParty()
{
    pc::BattleMenuState& state = pc::battleMenuState();
    state.party.clear();
    btl::BattleMenuPlayerControl* control = btl::BattleMenuPlayerControl::getSingleton();
    loadPortraitPalette(state);
    const int count = status::g_Party.getCarriageOutCount();
    for (int i = 0; i < count && i < 4; i++) {
        status::HaveStatusInfo* info = &status::g_Party.getPlayerStatus(i)->haveStatusInfo_;
        pc::BattlePartyRow row = partyRow(info, control->memberHP_[i], control->memberMP_[i], control->getHPColor(i),
                                          control->getCondition(i), !control->isConditionChange(i));
        row.active = control->activeChara_ == i;
        bool acted = false;
        for (size_t k = 0; k < s_acted.size(); k++) {
            acted = acted || s_acted[k] == info;
        }
        row.ready = !acted && info->isActionEnable();
        state.party.push_back(row);
    }
}

void publishFieldParty(const status::HaveStatusInfo* active)
{
    // The members walking (battle mode orders them), as drawPartyStatusWindows shows them.
    pc::BattleMenuState& state = pc::battleMenuState();
    state.party.clear();
    loadPortraitPalette(state);
    status::g_Party.setBattleMode();
    const int count = status::g_Party.getCarriageOutCount();
    for (int i = 0; i < count && i < 4; i++) {
        status::HaveStatusInfo* info = &status::g_Party.getPlayerStatus(i)->haveStatusInfo_;
        const int condition = getStatusCondition(info);
        const int label = condition == 2 ? menu::MenuDataCommon::convMessage(info, 0xa0000014)
                        : condition == 3 ? menu::MenuDataCommon::convMessage(info, 0xa0000018)
                                         : 0xa000003f;
        pc::BattlePartyRow row = partyRow(info, info->getHp(), info->getMp(), getHpTextColorIndex(info), label,
                                          condition != 2 && condition != 3);
        row.active = info == active;
        state.party.push_back(row);
    }
    status::g_Party.setPlayerMode();
}

namespace {
// A text display's rendered line (its 4-bit buffer, 256x16), up to what is revealed.
pc::BattleText lineText(MenuTextDisplay* line, int wholeWidth = 0)
{
    pc::BattleText text;
    int width = wholeWidth > 0 ? wholeWidth : line->visibleWidth_ < line->width_ ? line->visibleWidth_ : line->width_;
    if (width > 256) {
        width = 256;
    }
    if (width <= 0) {
        return text;
    }
    text.width = width;
    text.height = 16;
    text.ink.resize(width * 16);
    const unsigned char* tiles = (const unsigned char*)line->charData_;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < width; x++) {
            const unsigned char pair = tiles[((y >> 3) * 32 + (x >> 3)) * 0x20 + (y & 7) * 4 + (x & 7) / 2];
            text.ink[y * width + x] = (x & 1) ? pair >> 4 : pair & 15;
        }
    }
    return text;
}
}

bool publishBattleMessage(MessageWindow* window)
{
    // Battles, and the field menus while they show (they published this frame).
    if (!classicBattleMenus() || (!g_GlobalDQ4.unkfunc_02058114(BATTLE_PART) && !pc::battleMenuState().field)) {
        return false;
    }
    pc::BattleMessage& message = pc::battleMenuState().message;
    message = pc::BattleMessage();
    message.shown = true;
    message.waiting = window->isWaitTrigger() != 0;
    message.height = window->height_;
    message.shakeX = window->shakeX_;
    message.shakeY = window->shakeY_;
    for (int i = 0; i < window->lineCount_ && i < 4; i++) {
        MenuTextDisplay* line = window->lines_[i];
        if (line) {
            pc::BattleMessageLine out;
            out.text = lineText(line);
            out.fullWidth = line->width_ < 256 ? line->width_ : 256;
            out.y = window->config_->margin_ + i * window->lineHeight_ - window->scroll_;
            message.lines.push_back(out);
        }
    }
    if (window->namePlate_ && window->namePlate_->enable_) {
        message.name = lineText(window->namePlate_);
    }
    return true;
}

bool publishBattlePlate(MenuTextDisplay* text)
{
    if (!classicBattleMenus() || !text->frame_ || !g_GlobalDQ4.unkfunc_02058114(BATTLE_PART)) {
        return false;
    }
    // The message window's name plate is the message window's.
    if (g_MenuDisplays.message_.namePlate_ == text) {
        return false;
    }
    pc::BattlePlate plate;
    // A plate's text sits 4 pixels into its buffer, the whole frame wide (setFramedText).
    plate.text = lineText(text, text->frameWidth_);
    plate.width = text->frameWidth_;
    plate.x = text->x_;
    plate.y = text->y_ % 192;   // as MenuTextDisplay::draw places it on the bottom screen
    // The plates' colours (drawMonsterNamePlates): palette 15 bright, 14 dim;
    // letters 10/11 yellow (low health), 12/13 red (very low), odd ones dim.
    const int letters = text->letterPalette_;
    plate.tone = letters == 10 || letters == 11 ? 1 : letters == 12 || letters == 13 ? 2 : 0;
    plate.dim = letters >= 0 ? (letters & 1) != 0 : text->palette_ == 14;
    // The bright plate palette of the bottom screen's sprites (the count is in one of its colours).
    const PaletteBuffer* obj = getObjPaletteBuffer(1);
    for (int i = 0; i < 16; i++) {
        plate.palette[i] = rgb(obj->color_[15 * 16 + i]);
    }
    pc::battleMenuState().plates.push_back(plate);
    return true;
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
void drawFieldItems(int chara, bool bag, int page, int active, int giver)
{
    // A member's items (6 a page) or the bag's (FukuroItemInfo: also 6 a page), under its
    // name; for a swap (giver), the target's items and an empty slot to give into.
    status::g_Party.setPlayerMode();
    std::vector<int> items;
    status::HaveStatusInfo* owner = NULL;
    int title = 0x5000005b;
    if (bag) {
        const int count = status::FukuroItemInfo::getItemMaxCount();
        for (int k = 0; k < count; k++) {
            items.push_back(status::FukuroItemInfo::getItemId(k % 6, k / 6));
        }
    } else {
        owner = &status::g_Party.getPlayerStatus(chara)->haveStatusInfo_;
        const int count = status::PlayerItemInfo::getItemMaxCount(chara);
        for (int k = 0; k < count; k++) {
            items.push_back(status::PlayerItemInfo::getItemIndex(chara, k));
        }
        title = owner->haveStatus_.playerIndex_ + 0x50000000;
    }
    beginFieldMenu(pc::BattleMenuKind::items, 2);
    setBattleTitle(title);
    addBattleItems(items.empty() ? NULL : &items[0], (int)items.size(), page, active, owner);
    const int shown = (int)s_building.entries.size();
    if (giver >= 0 && !bag && shown < 6 && page * 6 + shown == (int)items.size()) {
        pc::BattleMenuEntry empty;
        empty.label = numbersText("- - -", 12);
        empty.index = (short)shown;
        s_building.entries.push_back(empty);
        if (items.empty()) {
            setBattlePage(0, 1);
        }
        setBattleCursor((short)active);
        s_keys.push_back(-3);
    }
    endFieldMenu(TownMenu_ROOT::ROOT_ITEM, giver >= 0 ? &status::g_Party.getPlayerStatus(giver)->haveStatusInfo_ : owner);
}

void drawBattlePartyTargetsPaged(int title, int page, int active)
{
    // The party in battle order, 4 a page (drawPartyHpMpList), under the item or spell.
    const status::HaveStatusInfo* user = NULL;
    TownMenuPlayerControl* control = TownMenuPlayerControl::getSingleton();
    status::g_Party.setPlayerMode();
    if (!control->activeFukuro_) {
        user = &status::g_Party.getPlayerStatus(control->activeChara_)->haveStatusInfo_;
    }
    status::g_Party.setBattleMode();
    const int count = status::g_Party.getCount();
    beginFieldMenu(pc::BattleMenuKind::targetParty, 1);
    setBattleTitle(title);
    for (int i = page * 4; i < count && i < page * 4 + 4; i++) {
        addBattleEntry(status::g_Party.getPlayerStatus(i)->haveStatusInfo_.haveStatus_.playerIndex_ + 0x50000000, (short)(i - page * 4), true);
    }
    if (count > 4) {
        setBattlePage(page, (count - 1) / 4 + 1);
    }
    setBattleCursor((short)active);
    endFieldMenu((title & 0xf0000000) == 0x40000000 ? TownMenu_ROOT::ROOT_ITEM : TownMenu_ROOT::ROOT_MAGIC, user);
}

void drawFieldTowns(int rootCommand, int chara, int page, const unsigned char* towns, int count, int active)
{
    // drawRuraTownList's towns, 8 a page in two columns.
    beginFieldMenu(pc::BattleMenuKind::list, 2);
    for (int i = page * 8; i < count && i < page * 8 + 8; i++) {
        addBattleEntry(getRuraTownName(towns[i]), (short)(i - page * 8), true);
    }
    if (count > 8) {
        setBattlePage(page, (count - 1) / 8 + 1);
    }
    setBattleCursor((short)active);
    status::g_Party.setPlayerMode();
    endFieldMenu(rootCommand, chara >= 0 ? &status::g_Party.getPlayerStatus(chara)->haveStatusInfo_ : NULL);
}
#endif

