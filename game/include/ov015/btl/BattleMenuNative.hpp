#pragma once
#ifdef DQIV_NATIVE
#include "BattleWindows.hpp"
#include "main/menu/MenuBase.hpp"
#include "main/menu/CursorMoveGridLoop.hpp"

namespace status { struct HaveStatusInfo; }

// Classic battle menus (pc::gameSettings().battleMenus 1): each battle
// menu describes what it shows (pc::battleMenuState) instead of drawing the
// DS menu, and reads its input through battleMenuSelect.
bool classicBattleMenus();
// Overlay menus (battleMenus 2): the DS menus drawn on the port overlay (MenuLayers.hpp).
bool overlayMenus();
// A message value (as drawMenuText takes it) drawn by the game's 12-dot font.
// fontSize: the game's 12-dot font, or its 10-dot one.
pc::BattleText battleText(int value, int fontSize = 12);
// A number, as drawMenuNumber draws it.
pc::BattleText battleNumber(int number, int fontSize = 12);
// Starts this frame's description; endBattleMenu publishes it.
void beginBattleMenu(pc::BattleMenuKind kind, int columns);
void setBattleTitle(int value);
void setBattleFooter(int value, int number);   // a label and a number ("MP 12"); value 0: none
// right: a message value (rightKind 0), a number (1) or a count shown "×n" (2).
void addBattleEntry(int text, short index, bool enabled, int right = 0, int rightKind = 0);
// The last entry's command icon (command24, as drawBattleCommandIcon picks it).
void setBattleEntryIcon(int icon);
void setBattleEntryItemIcon(int item);        // the last entry's item icon (32x32, as the item lists)
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
// title: the list header's text (the command chosen).
void drawBattleEnemyTargets(const TOUCHRECT* rects, int count, int active, int title);
void drawBattlePartyTargets(int count, int active, int title);
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
// Classic field menus: the same state, placed from the top left (field), with the gold.
void beginFieldMenu(pc::BattleMenuKind kind, int columns);
void setFieldRootParent(int chosen);   // the root's commands above the list, the hand on chosen
void publishFieldParty(const status::HaveStatusInfo* active);  // the walking members' cards; active slides out (NULL: none)
void setBattleGold(int amount);
// A field list's end: the root's commands above it (the hand on rootCommand), the gold,
// endBattleMenu, the party (active slid out).
void endFieldMenu(int rootCommand, const status::HaveStatusInfo* active);
// The items of one page (6), their icons, "E" marks (owner NULL: none), pager and the
// chosen one's description.
void addBattleItems(const int* items, int count, int page, int active, status::HaveStatusInfo* owner);
// Field item lists: a member's (or the bag's) items; giver >= 0: a swap's target list with an empty slot.
void drawFieldItems(int chara, bool bag, int page, int active, int giver);
// The party, 4 a page, under an item's or a spell's name (user: the card slid out, may be NULL).
void drawBattlePartyTargetsPaged(int title, int rootCommand, const status::HaveStatusInfo* user, int page, int active);
// The towns of a Chimaera Wing or Zoom (drawRuraTownList), under the root command.
void drawFieldTowns(int rootCommand, const status::HaveStatusInfo* user, int page, const unsigned char* towns, int count, int active);
// Classic items window (the field Items flow, BattleItemsWindow): every Items screen draws
// all of it: beginItemsWindow, the panes out of focus (store*), then the focused one
// (beginFieldMenu .. setItemsDetail .. endItemsWindow; drawFieldItems,
// drawBattlePartyTargetsPaged and drawFieldTowns end it themselves while it is open).
void beginItemsWindow();
bool itemsWindowOpen();
void addItemsWhose(int cursor, bool bag = true);  // the members with their faces (and the bag), into the menu being built
void storeItemsWhose(int chosen, bool bag = true);  // the same, out of focus (chosen: its still cursor, -1 none)
// The window's header (the Items window's: the root's Items command).
void setItemsWindowTitle(int text);
// The detail pane without an item (Attributes): its header, and label / value rows
// (the value a number, "a/b", a play time or a text).
void setItemsKind(int text);
// The shop: the members (and the bag), each with what item means to them: its comparison
// (equipment) or how many they hold; out of focus. And the detail's count line, as text.
void storeShopWhose(int item);
void setItemsCountText(const char* utf8);
void addItemsRow(int label, int value);
void addItemsRowPair(int label, int value, int max);
void addItemsRowTime(int label, int frames);
void addItemsRowText(int label, int text);
// Equipment (TownMenu_OPERATION_EQUIP): a member's equipment items ids (6 a page, "E" on
// those worn), headed by title (0: none), then removeText's entry (0: none) at index
// count-on-page; into the menu being built, or out of focus.
void addEquipList(const unsigned char* ids, int count, int page, int active, const status::HaveStatusInfo* owner, int title, int removeText);
// A member's entry (name and face) at index, into the menu being built; and a pane of
// texts (title 0: none; numbered: 1, 2... before them) out of focus.
void addMemberEntry(const status::HaveStatusInfo* info, short index);
void storeItemsTexts(const int* texts, int count, int title, bool numbered = false);
// A record row (Battle Records): label, then at the right a number and its unit, a play
// time ("HHH:MM"), a text, or nothing (label 0: an empty row).
void addRecordNumber(int label, int value, int unit, short index);
void addRecordTime(int label, int frames, short index);
void addRecordText(int label, int text, short index);
void storeEquipList(const unsigned char* ids, int count, int page, const status::HaveStatusInfo* owner, int title);
void storeItemsList(int chara, bool bag, int page, int chosen);  // a member's or the bag's list, out of focus
// The detail of item: owner its holder (NULL: the bag; no comparison), bagIndex the bag's slot (-1: a member's).
void setItemsDetail(int item, status::HaveStatusInfo* owner, int bagIndex);
void endItemsWindow(pc::BattleItemsFocus focus, const status::HaveStatusInfo* slid);
// TownMenuPlayerControl's chosen member (whose: also the whose pane) and item, out of focus; its detail.
void storeItemsChosen(bool whose);
void setItemsChosenDetail(int item);
// The spells actions[0..num), their kind icons, the chosen one's description and "cost/casterMp".
void addBattleSpells(const int* actions, int num, int active, int casterMp);
// The battle message window (Classic): its lines as the game rendered them.
// True when published: the DS window is then not drawn.
struct MessageWindow;
bool publishBattleMessage(MessageWindow* window);
// The party cards' background shows while a member can still act this round:
// a turn starting marks its actor, the next command phase clears the marks.
namespace status { struct CharacterStatus; }
void markBattleActed(status::CharacterStatus* chara);
void clearBattleActed();
// A monster name plate (a framed MenuTextDisplay in battle, Classic): published
// for the port to draw. True when published: the DS sprites are then not drawn.
struct MenuTextDisplay;
bool publishBattlePlate(MenuTextDisplay* text);
// The launch menu (MaterielMenu_LOAD, Classic): its commands as a field list (texts:
// message values or text pointers), and its log list of save cards (entries pickable;
// title 0: none). fit: the save list, as tall as its cards beside the party's cards.
void publishLaunchCommands(int count, const int* texts, int active);
void publishLaunchList(int title, const std::vector<pc::BattleSaveCard>& cards, int entries, int active, int revision, bool fit = false);
// A save card's pieces: its date ("YYYY-MM-DD HH:MM", local; empty for 0), a play time
// ("HHH:MM" from the game's frames), a plain UTF-8 text, and a player's face.
pc::BattleText battleSaveDate(long long seconds);
// The save cards of both pages: (resumeSaves: the autosave, the quick save, then) logs 1-6,
// none pickable yet (entry -1); the session page is shown again after.
std::vector<pc::BattleSaveCard> readSaveCards(bool resumeSaves);
pc::BattleText battlePlayTime(int frames);
pc::BattleText battleString(const char* utf8);
const std::vector<std::uint8_t>& playerFace(int player);
// A town service's commands (church, shop, inn): a Classic field list of texts in
// columns, the hand on active, with the gold and the party's cards.
void publishFieldCommands(const int* texts, int count, int columns, int active);
// The church's member picker: the whole party in battle order, each with its HP (or its
// death or poison), the hand on active (whose card slides out), the gold and the cards.
void publishChurchMembers(int active);
// Clears the description at the start of each frame of the menu system.
void clearBattleMenu();
#endif
