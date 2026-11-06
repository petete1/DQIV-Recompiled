#pragma once
#ifdef DQIV_NATIVE
#include "BattleWindows.hpp"
#include "main/menu/MenuBase.hpp"
#include "main/menu/CursorMoveGridLoop.hpp"

// Classic battle menus (pc::gameSettings().battleMenus 1): each battle
// menu describes what it shows (pc::battleMenuState) instead of drawing the
// DS menu, and reads its input through battleMenuSelect.
bool classicBattleMenus();
// Overlay menus (battleMenus 2): the DS menus drawn on the port overlay (MenuLayers.hpp).
bool overlayMenus();
// A message value (as drawMenuText takes it) drawn by the game's 12-dot font.
pc::BattleText battleText(int value);
// A number, as drawMenuNumber draws it.
pc::BattleText battleNumber(int number);
// Starts this frame's description; endBattleMenu publishes it.
void beginBattleMenu(pc::BattleMenuKind kind, int columns);
void setBattleTitle(int value);
void setBattleFooter(int value, int number);   // a label and a number ("MP 12"); value 0: none
// right: a message value (rightKind 0), a number (1) or a count shown "×n" (2).
void addBattleEntry(int text, short index, bool enabled, int right = 0, int rightKind = 0);
void setBattlePage(int page, int pages);
void setBattleCursor(short active);           // the entry with that game index
void endBattleMenu();
// MenuUpdate_Assist::menuSelect with the classic layout's cursor: directions
// move in the windows (or pass to the game to turn a page), the mouse points.
int battleMenuSelect(menu::MenuItem& item, CursorMoveBase& cursor);
struct TOUCHRECT;
// Target menus: one entry per monster group (the first of its touch
// rectangles, as the game's own group-to-group moves land), or per member;
// the windows show the active member's name beside them.
void drawBattleEnemyTargets(const TOUCHRECT* rects, int count, int active);
void drawBattlePartyTargets(int count, int active);
// After MenuItem::execInput in a menu that reads it directly (targets, use or
// equip): the pointer and the direction keys choose an entry of the list,
// reported as the item's own "changed" result (a target menu then switches
// the target as its touches and edges do).
void battleListInput(menu::MenuItem& item);
// Spells and items: the current page, two columns, as drawMagicMenu and
// drawItemList show it; and the use-or-equip choice of an item.
void drawBattleSpells(const int* actions, int count, int page, int active);
void drawBattleItems(const int* items, int count, int page, int active);
void drawBattleUseOrEquip(int item, int active);
// The party panel (Classic): the members' portraits, names, HP, MP and level,
// as the DS status windows show them (BattleMenuSub_HISTORY draws this instead).
void publishBattleParty();
// Clears the description at the start of each frame of the menu system.
void clearBattleMenu();
#endif
