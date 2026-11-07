# Classic field menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The menu outside battle (root, Items, Magic) takes Classic's DQ11 look, reusing the Classic battle windows, with a gold panel, the party cards and the kept world map.

**Architecture:** The field menus publish what they show into the same `pc::BattleMenuState` the battle menus fill (via the helpers in `BattleMenuNative.cpp`), with a flag that places the group at the top left and an optional frozen parent group (the root commands) above the lists. `native/BattleWindows` lays out and composes it with the battle code, plus a gold panel; `GameWindow::drawBattleWindows` draws it; `FramePresenter` shows the last field map inset at the bottom left while a Classic field menu is open.

**Tech Stack:** C++20, CMake preset `linux64-game` (distrobox `vibecode`), the game's text engine.

**Spec:** `docs/superpowers/specs/2026-11-07-classic-field-menu-design.md`

## Global Constraints

- Classic only (`battleMenus==1` with the wide picture, `classicBattleMenusOn`); Original and Overlay run today's code in the field.
- The ini key stays `battleMenus`; the label becomes "Menus" in every language.
- Look exactly as Classic battle (`BattleWindows.cpp` constants); only the gold panel is new.
- Layout in DS pixels at `battleScale`: commands top left 8 in; lists 4 below the commands, left aligned; gold top right 8 in; party cards bottom right as in battle; map bottom left.
- Build: `distrobox enter vibecode -- sh -c 'cd <repo> && cmake --build --preset linux64-game'`; tests: `--target dqiv_battle_tests && ./build64/dqiv_battle_tests` (the game preset does not rebuild the tests).
- Screen checks: `tests/screen/game.sh start 64 battleMenus=1 wide=1 …` (settings as start arguments, never the menus or `dist/saves/settings.ini`); the field menu opens with `s` after loading the quick save (as `tobattle.sh` does).

## Review Focus

1. A party of 1 or of 8 (carriage): the cards show the carriage-out members only (≤4), the "whose items" list shows everyone plus the bag — Task 5 screen check with the test save; `publishFieldParty` caps at 4 rows.
2. The bag empty or with more than 6 items: no entries / a pager — Task 5 test `fieldListPaged` (native) and screen check.
3. A message (item used) while the map shows: the map hides while the message shows — Task 4 test `mapHiddenWithMessage`.
4. The menu opened in a town or right after arriving on the overworld (map not faded in): no map, no stale map from an earlier field — Task 4 (`seen3d_` reset) screen check.
5. 640×480 window: commands, list, description, gold and cards stay inside — Task 2 test `fieldInsideSmallWindow`.

---

### Task 1: The setting becomes "Menus"

**Files:** `native/Settings.cpp` (`settingLabel`, help text of `settingBattleMenus`), `tests/battle/battle_windows_tests.cpp`

- [ ] Failing test `menusLabel`: `std::string(pc::settingLabel(pc::settingBattleMenus,0))=="Menus"`.
- [ ] Run tests: FAIL on "Menus".
- [ ] Labels: "Menus", "Menus", "Menüs", "Menu", "Menús". Help (English): "Classic, console-style windows in battle and in the field menu (wide picture only); Overlay, the DS battle menus anywhere on the screen; Original, the DS touch menus." — the other four languages translated the same way.
- [ ] Tests pass; commit "Settings: Battle menus becomes Menus".

### Task 2: Field placement, parent commands and the gold panel (native)

**Files:** `native/BattleWindows.hpp/.cpp`, `tests/battle/battle_windows_tests.cpp`

**Interfaces (Produces):**
```cpp
// BattleMenuKind gains: list   (a one-column list: whose items, item commands, towns, casters)
// BattleMenuState gains:
bool field=false;                         // a field menu: placed from the top left
std::vector<BattleMenuEntry> parent;      // the root commands shown above a list (2 columns, field only)
int parentCursor=-1;                      // the chosen command (a still hand, no blink)
BattleText gold,goldUnit;                 // "1259" and "G"; empty gold: no panel
BattleGroupLayout layoutBattleGold(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleGold(const BattleMenuState&,const BattleGroupLayout&);
// The map inset at the bottom left, canvas units (canvas 384 high): {x,y,w,h}.
std::array<float,4> fieldMapInset(float canvasHeight,float insetWidth,float insetHeight);
```
- [ ] Failing tests:
  - `fieldAtTopLeft`: `field=true`, kind `commands`, 6 entries, 2 columns at 1920×1080 → `windows[0].x==8*4 && windows[0].y==8*4`.
  - `listBelowParent`: `field=true`, kind `items`, 4 entries, `parent` 6 entries → 2+ windows; the list window `y >= parent window y+h+4*4`, same `x`; hit areas only for the list (`entries.size()==4`).
  - `goldTopRight`: `gold=ink(30)`, `goldUnit=ink(8)` at 1920×1080 → `picture.x+picture.w==1920-8*4 && picture.y==8*4`; at 640×480 inside the window; empty `gold` → `picture.w==0`.
  - `fieldInsideSmallWindow`: the `listBelowParent` state plus 4 `help` words, 4 party rows, at 640×480 → every window and the gold picture inside 640×480.
  - `fieldListPaged`: kind `list`, `pages=3`, `pageText=ink(20)` → one window, entries below the header (as `listHeader`).
  - `fieldDescriptionBelow`: the `listBelowParent` state with labels 100 wide, 4 `help` words and 4 party rows at 1920×1080 → the description window `y >= list y+h+4*4` and same `x` (field: below the list when there is no room at its right; battle keeps above).
  - `mapBottomLeft`: `fieldMapInset(384,153.6f,115.2f)` → `{6,384-6-115.2f,153.6f,115.2f}`.
- [ ] Run tests: compile FAIL (missing names).
- [ ] Implement in `plan()`: when `field`, the vertical placement in `layoutBattleMenu` starts at `margin` from the top instead of the bottom; when `parent` is non-empty, a first window for it (2 columns, icons, no header, `cursorWidth` column with a still hand at `parentCursor`), and the list window (and its description) 4 below it, x 0; `layout.entries` only for `state.entries`. The gold panel: one window, `pad` around the texts, height `row+2*pad`, amount then 3 pixels then the unit, right edge `windowWidth-margin*s`, top `margin*s`.
- [ ] Tests pass (whole suite); commit "Classic field menu: top-left placement, parent commands, gold panel".

### Task 3: The root screen (game side)

**Files:** `game/include/ov015/btl/BattleMenuNative.hpp`, `game/src/ov015/btl/BattleMenuNative.cpp`, `game/src/ov016/TownMenu_ROOT/TownMenu_ROOT.cpp`, `native/GameWindow.cpp`

**Interfaces:**
- Consumes: Task 2's state fields and `layoutBattleGold`/`composeBattleGold`.
- Produces:
```cpp
void beginFieldMenu(pc::BattleMenuKind kind, int columns);   // beginBattleMenu + field=true
void setFieldRootParent(int chosen);   // the 6 root commands (getRootMenuName/Kind, icons) as parent, cursor on chosen
void publishFieldParty(int active);    // cards from the carriage-out members (≤4), own HP/MP; active slides out (-1 none)
void setBattleGold(int amount);        // gold = numbersText(amount), goldUnit = battleText(0xa000003c)
```
- [ ] `publishFieldParty`: shares the row building of `publishBattleParty` (extract `fillPartyRow(pc::BattlePartyRow&, status::HaveStatusInfo*, int hp, int mp, int colour, int conditionText)`); HP/MP `info->getHp()/getMp()`, colour `getHpTextColorIndex(info)`, label: `getStatusCondition(info)` 2 → `convMessage(info,0xa0000014)`, 3 → `convMessage(info,0xa0000018)`, else "Lv." (`0xa000003f`) with the level; `ready=false`; `active` = index == active.
- [ ] `TownMenu_ROOT::menuDraw`: when `classicBattleMenus()`: `beginFieldMenu(commands,2)`, the 6 commands in DS order with `setBattleEntryIcon(getRootMenuKind(i))`, `setBattleCursor(menuItem_.active_)`, `setBattleGold(status::g_Party.gold_)`, `endBattleMenu()`, `publishFieldParty(-1)`; no DS drawing. `menuUpdate`: `battleMenuSelect` in place of `MenuUpdate_Assist::menuSelect` when classic.
- [ ] `publishBattleMessage`: also when `pc::battleMenuState().field` (the field menu published this frame) — not only in `BATTLE_PART`.
- [ ] `GameWindow::drawBattleWindows`: draws the gold panel (its own overlay id after `overlayBattlePlate0+3`) when `gold` is non-empty.
- [ ] Build; screen check: overworld, `s`: the commands top left with icons, gold top right, cards bottom right, no DS windows; arrows move the hand; Magic with nobody able → the message in the Classic window; Talk/Attributes/Misc open their DS screens and come back. Commit "Classic field menu: the root screen".

### Task 4: The map stand-in

**Files:** `native/FramePresenter.cpp/.hpp`, `native/BattleWindows.hpp/.cpp`, `tests/battle/battle_windows_tests.cpp`

**Interfaces:**
- Consumes: `fieldMapInset` (Task 2), `battleMenuState()`.
- Produces: `bool classicFieldMapShown(const BattleMenuState&)` — `field && kind!=none && !message.shown`.
- [ ] Failing test `mapHiddenWithMessage`: a field state → true; with `message.shown=true` → false; a battle state (`field=false`) → false.
- [ ] Implement; in `FramePresenter::frame`, `Mode::menu` with Classic on, `seen3d_` and `classicFieldMapShown(battleMenuState())`: `layout_.inset=fieldMapInset(384,256*0.6f,192*0.6f)` (as array → `GpuLayout::Rect`), `insetAlpha` as last set. The inset's 3D target (slot 1) and its 2D rows (recipes 384..575) are not rewritten in `Mode::menu`; verify on screen, and if the compose pass needs the map rows marked, keep the last field frame's rows there.
- [ ] Tests pass; build; screen check: overworld menu shows the map bottom left; a town shows none; leaving the menu the map is back top right; a message hides it. Commit "Classic field menu: the world map at the bottom left".

### Task 5: Items

**Files:** `game/src/ov015/btl/BattleMenuNative.cpp/.hpp`, `game/src/ov016/TownMenu_ITEM/TownMenuItemSelectChara.cpp`, `game/src/ov016/TownMenuItemSelectItem/TownMenuItemSelectItem.cpp`, `game/src/ov016/TownMenu_ITEM/TownMenuItemSelectCommand.cpp`, `TownMenuItemSelectTargetChara.cpp`, `TownMenuItemSelectTargetItem.cpp`, `TownMenu_ITEM_USE.cpp`, `TownMenu_ITEM_MOVE.cpp`

**Interfaces:**
- Consumes: Task 3's helpers.
- Produces: `drawBattleItems(const int* items, int count, int page, int active, status::HaveStatusInfo* owner)` — `owner` for the "E" marks (NULL for the bag); battle passes the active member's.
- [ ] Every screen, when classic: `beginFieldMenu(kind, columns)`, `setFieldRootParent(ROOT_ITEM)`, its entries, `setBattleGold`, `endBattleMenu()`, `publishFieldParty(active member or -1)`; no DS drawing; `battleMenuSelect`/`battleListInput` for the cursor.
  - whose items: kind `list`, members (`0x50000000+playerIndex_`) and, when `g_Party.fukuro_`, the bag (the text `drawPartyIconRow` shows for it), cursor on the member; that member slides out.
  - the list: `drawBattleItems` with the member's items (`PlayerItemInfo::getItemIndex`, 6 per page) or the bag's (`FukuroItemInfo::getItemId(i,page)`, `getPageMax()` pages); title the member's name or the bag's.
  - the item's commands: kind `list`, the texts `drawItemUseCommands(flag)` draws for the flag, title the item's name.
  - targets (`SelectTargetChara`, `ITEM_USE`): kind `targetParty` (`drawBattlePartyTargets` with title the item's name).
  - swap (`SelectTargetItem`): `drawBattleItems` with the target's items, title the target's name.
  - Chimaera Wing (`ITEM_MOVE`): kind `list`, the towns `drawRuraTownList` draws, paged as the game pages them.
- [ ] Build; tests pass; screen check: a member's list with icons and description, the bag and its pages, use a medicinal herb on a member (target list, the message, HP on the card goes up), equip/transfer commands listed, Esc back through every screen. Commit "Classic field menu: Items".

### Task 6: Magic

**Files:** `game/src/ov015/btl/BattleMenuNative.cpp/.hpp`, `game/src/ov016/TownMenu_MAGIC/TownMenu_MAGIC_ROOT.cpp`, `TownMenu_MAGIC_CHARSELECT.cpp`, `TownMenu_MAGIC_MOVE.cpp`

**Interfaces:**
- Produces: `drawBattleSpells(const int* actions, int count, int page, int active, int caster)` — `caster` for "cost/MP" (party index); battle passes `activeChara_`.
- [ ] `MAGIC_ROOT` mode 0: kind `list`, the members `drawCharaSelectWindow(0,0)` shows (those with spells), cursor on `activeChara_`, that member slides out; mode 1: `drawBattleSpells` with the member's field spells (the actions `drawFieldSpellList` lists, its paging), title the member's name. `CHARSELECT`: `drawBattlePartyTargets` titled with the spell's name. `MAGIC_MOVE`: kind `list`, Zoom's towns as in Task 5. All with `setFieldRootParent(ROOT_MAGIC)`, gold, party, no DS drawing.
- [ ] Build; tests pass; screen check: choose a caster, spells with kind icons and "cost/MP", cast Heal on a member (MP on the card goes down, the message), Zoom's town list. Commit "Classic field menu: Magic".

### Task 7: Original and Overlay untouched

- [ ] Screen shots of the field menu root, Items list and Magic list with `battleMenus=0` and `=2`, and with `battleMenus=1 wide=0`: the DS screens as before this branch (compare with shots from the commit before Task 1). Ledger the result.
