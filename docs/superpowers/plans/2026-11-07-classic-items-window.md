# Classic Items Window Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** With Classic menus on, the field Items flow draws as one DQ11-style window: whose items (with faces), the list, the detail (big icon, description, count, equipment comparison), and a "Do What?" popup.

**Architecture:** `pc::BattleMenuState` gains `items` (a `BattleItemsWindow`): the panes not in focus, as stored copies, and the detail. The focused pane keeps using `entries`/`cursor`/`title`/`pageText`/`help`, so input (`battleMenuSelect`, the mouse, `moveBattleCursor`) is unchanged. `plan()` in `native/BattleWindows.cpp` lays out the big window when `items.shown`; `composeBattleMenu` draws it. The game side builds the stored panes with the existing builder and moves them aside (`stashItemsPane`).

**Tech Stack:** C++20, the port's native layer (`native/`), the decompiled game (`game/`), tests in `tests/battle/battle_windows_tests.cpp` (target `dqiv_battle_tests`), screen tests via `tests/screen/game.sh`.

**Spec:** `docs/superpowers/specs/2026-11-07-classic-items-window-design.md`

## Global Constraints

- Only with Classic menus (`battleMenus`=1 with the wide picture: `classicBattleMenus()` on the game side). Original and Overlay are unchanged; Magic and the other commands keep today's Classic panels.
- The look: Classic's (`fill`, `tan`, `cream`, `frame()`, the game's animated cursor); no parchment.
- Build: `distrobox enter vibecode -- sh -c 'cd /var/home/stabares/Descargas/dq4/DQIV-Recompiled-1.3 && cmake --build --preset linux64-game'`. Tests: the same with `--target dqiv_battle_tests && ./build64/dqiv_battle_tests` (the game preset does not rebuild the tests).
- Never edit `dist/saves/settings.ini`; screen tests set settings through `tests/screen/game.sh start 64 battleMenus=1 wide=1`. Only stop the test copy (`game.sh stop`), never `pkill`.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No push.
- Code style: the surrounding files' (dense one-line natives, the game's style in `game/`), comments only for the why.

## Review Focus

1. A member with no items, or an empty Bag: the list pane is blank with its header, the detail blank, no crash (unit test in Task 2: `itemsEmptyList`).
2. A party of 8 (wagon members) plus the Bag in the whose pane: the game does not page this list; 9 rows of 20 (180) fit the whose pane at 1080p (about 218 high), the tightest height (Task 1 test `itemsWhoseFitsNine`).
3. The popup's entries keep hit areas while the list and whose panes have none (Task 1 test `itemsHitAreasFocusOnly`).
4. A swap target with 6 items shows the "- - -" slot on page 2 in the list pane (existing `drawFieldItems` logic, kept; screen check in Task 4).
5. Leaving Items for the root: `items.shown` is cleared each frame (`MenuManager::execute`), so the commands and gold come back (Task 3 step).

---

### Task 1: The items window's state and layout

**Files:**
- Modify: `native/BattleWindows.hpp` (new types, `BattleMenuEntry::face`, `BattleMenuState::items`, `BattleMenuLayout::planHeight`)
- Modify: `native/BattleWindows.cpp` (`plan()` gains a height and the items window; `layoutBattleMenu`, `layoutBattleGold`, `classicFieldMapShown`)
- Test: `tests/battle/battle_windows_tests.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct BattleMenuEntry { ... std::vector<std::uint8_t> face; };  // 48x48 portrait indices (portraitPalette), may be empty
  struct BattleItemsPane {BattleText title,pageText;int pages=1;std::vector<BattleMenuEntry> entries;int cursor=-1;};
  enum class BattleItemsFocus {whose,list,popup};
  struct BattleItemsWindow {
      bool shown=false;
      BattleText title;                 // the root command's name ("Items")
      BattleItemsFocus focus=BattleItemsFocus::whose;
      BattleItemsPane whose,list;       // the stored panes; the focused one comes from the state's entries
      BattleText kind;                  // the detail's header (equipment kind, or the item's name)
      std::vector<std::uint32_t> icon;  // 32x32 ARGB, may be empty (no detail)
      BattleText count;                 // "×n" (the Bag), may be empty
      BattleText statLabel,statNow,statNew,statNote;  // the comparison: label + now ▶ new, or label + note (+ statNow)
  };
  BattleMenuState::items (BattleItemsWindow); BattleMenuLayout::planHeight (int).
  ```
  Layout facts later tasks rely on (DS units, `s`=`battleScale`, `W`=`windowWidth/s`, `H`=`windowHeight/s`):
  - `layout.windows[0]`: the big window `{margin, margin, W-2*margin-partyRoom-gap, H-2*margin}` (without a party: `W-2*margin`).
  - then the panes in order: whose, list, detail; then the popup when `focus==popup`.
  - `layout.entries`: one per `state.entries`, in the focused pane (the popup's when `focus==popup`).
  - Pane geometry: the title bar `headerHeight` high inside the window; panes below it, `pad` in, `gap` apart. whose width `2*pad+cursorWidth+max(label)+4+32` capped to 110; list width at least 130; detail at least 100; when `whose+list(130)+detail(100)+2*gap > inner`, the detail goes below the list (same x and width as the list), else beside it taking the rest. whose rows 20 high. The list pane is `headerHeight+2*pad+6*row` high.
  - Popup: right edge `W-margin`, top `margin`, as a field `list` window (title on its header).

- [ ] **Step 1: Write the failing tests**

```cpp
pc::BattleMenuState itemsState(pc::BattleItemsFocus focus,int whose=4) {
    pc::BattleMenuState state;state.kind=pc::BattleMenuKind::list;state.field=true;state.palette.fill(0xffffffff);
    state.items.shown=true;state.items.title=ink(30);state.items.focus=focus;
    for(int i=0;i<whose;++i)state.items.whose.entries.push_back(entry(40,short(i)));
    for(int i=0;i<6;++i)state.items.list.entries.push_back(entry(70,short(i)));
    state.items.list.title=ink(40);state.items.kind=ink(40);state.items.icon.assign(32*32,0xffffffff);
    for(int i=0;i<12;++i)state.help.push_back(ink(30,10));
    for(int i=0;i<4;++i)state.party.push_back({});
    const auto& from=focus==pc::BattleItemsFocus::whose?state.items.whose:state.items.list;
    if(focus!=pc::BattleItemsFocus::popup) {state.entries=from.entries;state.title=from.title;}
    else {for(int i=0;i<4;++i)state.entries.push_back(entry(50,short(i)));state.title=ink(60);}
    state.cursor=0;
    return state;
}
void itemsWindowFills() {
    for(auto [w,h]:{std::pair{640,480},{1280,720},{1920,1080}}) {
        const auto layout=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::list),w,h);
        const int s=pc::battleScale(h);
        if(layout.windows.size()<4) {check(false,"the window and its three panes");continue;}
        const auto& big=layout.windows[0];
        check(big.x==8*s&&big.y==8*s,"the items window 8 in at the top left");
        check(big.y+big.h==h-8*s,"down to 8 above the bottom");
        check(big.x+big.w<=pc::layoutBattleParty(itemsState(pc::BattleItemsFocus::list),w,h).picture.x,"left of the party cards");
        bool all=true;for(std::size_t i=1;i<4;++i)all=all&&layout.windows[i].x>=big.x&&layout.windows[i].y>=big.y
            &&layout.windows[i].x+layout.windows[i].w<=big.x+big.w&&layout.windows[i].y+layout.windows[i].h<=big.y+big.h;
        check(all,"the panes inside the window");
    }
}
void itemsDetailBeside() {
    const auto layout=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::list),1280,720);
    const auto& list=layout.windows[2];const auto& detail=layout.windows[3];
    check(detail.x>=list.x+list.w&&detail.y==list.y,"the detail beside the list where it fits");
}
void itemsDetailBelowWhenNarrow() {
    const auto layout=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::list),1920,1080);
    const auto& list=layout.windows[2];const auto& detail=layout.windows[3];
    check(detail.x==list.x&&detail.y>=list.y+list.h,"the detail below the list at 1080p");
}
void itemsPopupTopRight() {
    const auto layout=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::popup),1920,1080);
    if(layout.windows.size()<5) {check(false,"the popup");return;}
    const auto& popup=layout.windows[4];
    check(popup.x+popup.w==1920-8*4&&popup.y==8*4,"the popup at the screen's top right, 8 in");
}
void itemsHitAreasFocusOnly() {
    const auto popup=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::popup),1920,1080);
    check(popup.entries.size()==4&&popup.entries[0].x>=popup.windows[4].x,"hit areas: the popup's entries only");
    const auto whose=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::whose),1920,1080);
    check(whose.entries.size()==4&&whose.entries[0].x<whose.windows[2].x,"hit areas: the whose pane's");
}
void itemsWhoseFitsNine() {
    const auto layout=pc::layoutBattleMenu(itemsState(pc::BattleItemsFocus::whose,9),1920,1080);
    check(layout.entries.size()==9&&layout.entries[8].y+layout.entries[8].h<=layout.windows[1].y+layout.windows[1].h,"nine members and the Bag fit");
}
void itemsHideGoldAndMap() {
    auto state=itemsState(pc::BattleItemsFocus::list);state.gold=ink(30);state.goldUnit=ink(8);
    check(pc::layoutBattleGold(state,1920,1080).picture.w==0,"no gold with the items window");
    check(!pc::classicFieldMapShown(state),"no map with the items window");
}
```
Register all seven in `main()` after `originalCursor();`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: the test command (Global Constraints).
Expected: build error on `BattleItemsFocus` / `items` (the types do not exist).

- [ ] **Step 3: Add the types to `native/BattleWindows.hpp`** (Interfaces above), and `int planHeight=0;` beside `planWidth`.

- [ ] **Step 4: Run the tests** — they build and fail (`the window and its three panes` etc.).

- [ ] **Step 5: Implement the layout in `native/BattleWindows.cpp`**
  - `Plan plan(const BattleMenuState&,int width,int height=0)`; when `state.items.shown`, return `planItems(state,width,height)` (a new function in the anonymous namespace) with `p.picture={margin,0,W-2*margin,...}` covering the window and the popup, `p.entries` the focused pane's rows, and the pane boxes in `p.windows` in the order above. Record what compose needs in `Plan` (the pane boxes' indices are fixed: 0 window, 1 whose, 2 list, 3 detail, 4 popup).
  - `layoutBattleMenu`: pass `windowHeight/s`, set `layout.planHeight`; the items window is placed at `top=margin` like the field.
  - `layoutBattleGold`: picture.w 0 when `state.items.shown`. `classicFieldMapShown`: false when `state.items.shown`.

- [ ] **Step 6: Run the tests**
Expected: `battle windows: all passed 0`, no `FAIL:`.

- [ ] **Step 7: Commit**

```bash
git add native/BattleWindows.hpp native/BattleWindows.cpp tests/battle/battle_windows_tests.cpp
git commit -m "Classic items window: its state and layout

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

### Task 2: Drawing the items window

**Files:**
- Modify: `native/BattleWindows.cpp` (`composeBattleMenu`)
- Test: `tests/battle/battle_windows_tests.cpp`

**Interfaces:**
- Consumes: Task 1's types and plan (pane indices 0..4, `layout.planHeight`).
- Produces: nothing new; `composeBattleMenu` draws the items window.

What it draws:
- the window frame, its title bar (`items.title` centred, a `rule` under it); each pane `frame(…,false)` (no curls);
- whose: each row's label, and the face strip at the row's right: the portrait's 32×16 at x 8..40, y 14..30 (`portraitPalette`), its leftmost 8 columns faded (alpha ×(x+1)/9); the focused pane's cursor animated, else a still frame (frame 0) on `whose.cursor`;
- list: header (title, pager as today's list header), rows with "E" (`right`) drawn at the row's left before the icon, the 16-px icon, the label; cursor as above;
- detail: `items.kind` centred on top, the 32×32 icon unscaled in a framed 40×40 box centred, the description words wrapped below (`wrapBattleWords`, `helpRow`), then `count` right-aligned, then `statLabel` and `statNow ▶ statNew` (or `statNote` and `statNow`) under a `rule`; nothing when `icon` is empty;
- popup: today's list window look (title header, rows, animated cursor).

- [ ] **Step 1: Write the failing tests**

```cpp
void itemsDrawsFacesAndIcon() {
    auto state=itemsState(pc::BattleItemsFocus::list);
    for(auto& e:state.items.whose.entries)e.face.assign(48*48,1);
    state.portraitPalette.fill(0xff00ff00);state.items.icon.assign(32*32,0xff0000ffu);
    const auto layout=pc::layoutBattleMenu(state,1280,720);
    const auto pixels=pc::composeBattleMenu(state,layout,0);
    int green=0,blue=0;for(const auto p:pixels) {green+=p==0xff00ff00u;blue+=p==0xff0000ffu;}
    check(green>=4*24*16,"a face strip on each whose row");
    check(blue==32*32,"the big icon at full size");
}
void itemsStillCursorOnWhose() {
    auto state=itemsState(pc::BattleItemsFocus::list);state.items.whose.cursor=1;
    for(int f=0;f<5;++f)state.cursorFrames[std::size_t(f)].assign(16*16,0xff000000u|(0x10u*unsigned(f+1)));
    state.cursorFrame=3;
    const auto pixels=pc::composeBattleMenu(state,pc::layoutBattleMenu(state,1280,720),0);
    int current=0,still=0;for(const auto p:pixels) {current+=p==(0xff000000u|0x40u);still+=p==(0xff000000u|0x10u);}
    check(current==16*16&&still==16*16,"animated in the list, still on the chosen member");
}
void itemsEmptyList() {
    auto state=itemsState(pc::BattleItemsFocus::list);state.entries.clear();state.cursor=-1;state.help.clear();state.items.icon.clear();
    const auto layout=pc::layoutBattleMenu(state,1920,1080);
    check(layout.windows.size()>=4&&layout.entries.empty(),"an empty list keeps its pane");
    check(!pc::composeBattleMenu(state,layout,0).empty(),"and draws");
}
```
Register them after Task 1's.

- [ ] **Step 2: Run** — Expected: FAIL `a face strip on each whose row`, `the big icon at full size`, `animated in the list, still on the chosen member`.

- [ ] **Step 3: Implement** the drawing in `composeBattleMenu` (a branch on `state.items.shown` after the window frames, using `layout.planHeight` to re-plan). The 32×32 icon: copy pixels with alpha (`(c>>24)` non-zero), not `icon()` (which shrinks to 16).

- [ ] **Step 4: Run** — Expected: all pass, no `FAIL:`.

- [ ] **Step 5: Commit** `"Classic items window: drawing"` (same trailer).

### Task 3: The game side's items window helpers

**Files:**
- Modify: `game/include/ov015/btl/BattleMenuNative.hpp`, `game/src/main/menu/BattleMenuNative.cpp`
- Modify: `game/src/main/menu/MenuManager.cpp` (clear `items` each frame)

**Interfaces:**
- Consumes: Task 1's types; `getEquipCompareValues(status::HaveStatusInfo*,int item,int* param)` (declared in `ov016/UnkTownMenuDraw/TownMenuDraw.hpp`: returns 0 can't equip with `param[2]` its text, 2 equipped with `param[2]` text and `param[3]` value, 1 change with `param[2]` now and `param[4]` new); `menu::MenuDataCommon::getEquipKind(status::UseItem::getItemType(item))`, `getAbilityKind(type)` (`getEquipType(item)==4` → label `0xa000000b`, as `drawEquipCompareWindow`); `status::g_Party.haveItemSack_.getItemCount(index)`.
- Produces (in `BattleMenuNative.hpp`):
  ```cpp
  // Classic items window (the field Items flow): every Items screen draws all of it.
  void beginItemsWindow();                      // beginFieldMenu(list,1) and items.shown, title = the root's Items name
  void stashItemsPane(pc::BattleItemsPane& into);  // moves the entries, title, page and cursor built so far into a stored pane
  pc::BattleItemsPane& itemsWhosePane();        // s_building.items.whose
  pc::BattleItemsPane& itemsListPane();
  // The members (player order, each with its face) and the Bag, as the whose rows; chosen: the still cursor (-1 none).
  void addItemsWhose(int chosen);
  // The detail of item (owner: its holder, NULL for the Bag; bagIndex: the Bag's slot, -1 for a member).
  void setItemsDetail(int item, status::HaveStatusInfo* owner, int bagIndex);
  // Ends the frame: focus, the party (slid: its card out), endBattleMenu (no parent commands, no gold).
  void endItemsWindow(pc::BattleItemsFocus focus, const status::HaveStatusInfo* slid);
  ```
  and `drawFieldItems` lists one column (`beginFieldMenu(items, 1)`), keeping its signature.

- [ ] **Step 1: Extract `std::vector<std::uint8_t> portraitPixels(int icon)`** from `partyRow` (same cache) so whose rows can carry `face`; `partyRow` calls it.

- [ ] **Step 2: Implement the helpers** above. `addItemsWhose` adds entries with `addBattleEntry(playerIndex+0x50000000, i, true)` then the face, then the Bag (`0x5000005b`) when `g_Party.fukuro_`; the focused-pane use (whose in focus) calls it and does not stash. `setItemsDetail`: kind = `getEquipKind(type)` when `getItemType(item)<=4`, else `battleText(item+0x40000000)`; icon = the item's 32×32 (`menuIcon(0xf0000000,item,32)`); help = `setBattleHelp(getMenuMessage(item))`; count = "×n" for the Bag (the count form `addBattleEntry` rightKind 2 uses); the comparison only when `owner` and equipment. Each value goes into `s_keys` (the revision).

- [ ] **Step 3: Clear `items`** in `MenuManager::execute` with the other Classic fields (`pc::battleMenuState().items = pc::BattleItemsWindow();`).

- [ ] **Step 4: Build the game** (Global Constraints). Expected: builds, no errors.

- [ ] **Step 5: Commit** `"Classic items window: the game's helpers"` (same trailer).

### Task 4: The Items screens draw the window

**Files:**
- Modify: `game/src/ov016/TownMenu_ITEM/TownMenuItemSelectChara.cpp`, `game/src/ov016/TownMenuItemSelectItem/TownMenuItemSelectItem.cpp`, `game/src/ov016/TownMenu_ITEM/TownMenuItemSelectCommand.cpp`, `game/src/ov016/TownMenu_ITEM/TownMenuItemSelectTargetChara.cpp`, `game/src/ov016/TownMenu_ITEM/TownMenu_ITEM_USE.cpp`, `game/src/ov016/TownMenu_ITEM/TownMenuItemSelectTargetItem.cpp`, `game/src/ov016/TownMenu_ITEM/TownMenu_ITEM_MOVE.cpp`
- Modify: `native/GameWindow.cpp` (`drawBattleWindows`: the message after the menu while `items.shown`)

**Interfaces:**
- Consumes: Task 3's helpers; `TownMenuPlayerControl` (`activeChara_`, `activeFukuro_`, `activeItem_`, `activeItemPage_`, `targetChara_`, `targetFukuro_`).

Per screen (in each `menuDraw`'s Classic branch, replacing today's publish):

| Screen | Stored panes | Focus (built last, not stashed) | Detail | Slid |
|---|---|---|---|---|
| ItemSelectChara | list: preview of the member/Bag under the cursor, cursor -1 | whose (`addItemsWhose(-1)`, cursor `menuItem_.active_`) | none | the member under the cursor |
| ItemSelectItem | whose (chosen = activeChara or the Bag row) | list (`drawFieldItems` body, 1 column) | the item under the cursor | the owner |
| ItemSelectCommand | whose, list (cursor = activeItem_ on page activeItemPage_) | popup (today's entries, item name title) | the chosen item | the owner |
| ItemSelectTargetChara | list (as above) | whose (cursor = `menuItem_.active_`) | the chosen item | the giver |
| ITEM_USE | list (as above) | whose slot: today's targets (`drawBattlePartyTargetsPaged` body) | the chosen item | the user |
| ItemSelectTargetItem | whose (chosen = the target) | list (target's items, giver: today's call) | the item under the cursor | the giver |
| ITEM_MOVE | whose, list | list slot: the towns (`drawFieldTowns` body) | none | the user |

Pull today's bodies (`drawFieldItems`, `drawBattlePartyTargetsPaged`, `drawFieldTowns`) apart only as far as needed: they keep working for Magic (which still calls `drawBattlePartyTargetsPaged` / `drawFieldTowns` and must look as today). Rule: those functions take their end call (`endFieldMenu` vs. `endItemsWindow`) from whether `beginItemsWindow` was called this frame.

- [ ] **Step 1: The message after the window** in `GameWindow::drawBattleWindows`: when `state.items.shown`, draw the message overlay after the menu overlay (it shows over the window's bottom).

- [ ] **Step 2: Convert the seven screens** as the table says. Build. Expected: no errors.

- [ ] **Step 3: Run the unit tests** (all must pass) and pack: `build64/dqiv_pack build64/DragonQuestIV assets addons dist/DragonQuestIV64` (no DragonQuestIV running).

- [ ] **Step 4: Screen checks** (`battleMenus=1 wide=1`, the quick save, the scratchpad's `tofield.sh`, then `space` for Items): shots of whose (preview), Alena's list (detail: claw, "Equipped"), a medicinal herb (no comparison), an item Alena can't equip, the Bag (×n), the commands popup, Use a herb on Kiryl (message over the window), Transfer to a member with 6 items (swap with "- - -" on page 2), a Chimaera Wing's towns; Magic still in today's panels; `battleMenus=0` unchanged. Look at each shot.

- [ ] **Step 5: Commit** `"Classic items window: the Items screens"` (same trailer).
