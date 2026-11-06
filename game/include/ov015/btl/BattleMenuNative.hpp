#pragma once
#ifdef DQIV_NATIVE
#include "BattleWindows.hpp"
#include "main/menu/MenuBase.hpp"
#include "main/menu/CursorMoveGridLoop.hpp"

// Classic battle menus (pc::gameSettings().classicBattleMenus): each battle
// menu describes what it shows (pc::battleMenuState) instead of drawing the
// DS menu, and reads its input through battleMenuSelect.
bool classicBattleMenus();
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
// After a target menu's MenuItem::execInput: the pointer and the direction
// keys choose a group in the list; the menu's own "changed" code then
// switches the target as its touches and edges do.
void battleTargetInput(menu::MenuItem& item);
// Clears the description at the start of each frame of the menu system.
void clearBattleMenu();
#endif
