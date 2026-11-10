# Classic Launch Menu Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** With Classic menus, the launch menu's commands in a Classic panel and its adventure log list as one list of eight dated save cards.

**Architecture:** `pc::SavePages` stamps each bank's save time into the files' unused tail. The game side (`MaterielMenu_LOAD.cpp`) reads both pages into cards, publishes them through a new `pc::BattleLaunchMenu` part of `pc::BattleMenuState`, and maps a picked card back to a page and a slot. `native/BattleWindows` lays out and draws the list; `GameWindow` draws it like the other Classic menus.

**Tech Stack:** C++20, CMake presets in the `vibecode` distrobox, the project's own assert-style test executables.

**Spec:** `docs/superpowers/specs/2026-11-10-classic-launch-menu-design.md`

## Global Constraints

- Shows only when `classicBattleMenus()` is true (`battleMenus` = Classic with the wide picture); otherwise today's drawing, pages of three and L/R stay.
- Card order: Autosave, Quick save, Logs 1–6.
- Date text: `YYYY-MM-DD HH:MM`, local time; a zero stamp shows no date.
- Stamps: Unix seconds, 64-bit little-endian, at `0xFF00 + 8*k` of the page's own file, k = bank (0–2 logs, 3 the resume save); banks are `0x3C00` bytes from `0x400`.
- Continue: all eight cards pickable. Create and Delete: the six logs only; the resume cards drawn dimmed, not pickable.
- Messages, yes/no prompts, name entry and the gender pick stay as they are.
- Build: `distrobox enter vibecode -- sh -c 'cd /var/home/stabares/Descargas/dq4/DQIV-Recompiled-1.3 && cmake --build --preset linux64-game'`.
- Never edit `dist/saves/settings.ini`; screen tests set settings through `tests/screen/game.sh start 64 battleMenus=1 wide=1`. Never `pkill` the game; stop only the test copy (`game.sh stop`).
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No push.

## Review Focus

- Session page after reading both pages: loading log 5 then saving in the game must write log 5, not log 2 (Task 4 screen check; Task 3's `readAllCards` must restore `showPage(sessionPage())`).
- Names from the catalogue are overwritten by the next page's read: cards must hold rendered `BattleText`, never the catalogue's `char*` (Task 3).
- A delete or create changes a page: the cards must be re-read before the list shows again (Task 3, Task 4 screen check).
- 640×480 cannot hold eight cards: the cursor's card must stay inside the window while scrolling (Task 2 test `launchScrollsToCursor`).
- A write spanning a bank edge (bank 2 into bank 3, as `splitsAcrossBank3` does) must stamp both banks (Task 1 test `stampsBothBanksOfASplitWrite`).

---

### Task 1: Save dates

**Files:**
- Modify: `native/SavePages.hpp`, `native/SavePages.cpp`
- Test: `tests/save/save_pages_tests.cpp` (add `#include <ctime>`)

**Interfaces:**
- Produces: `std::int64_t pc::SavePages::saveTime(int page,int bank) const` — the stamp (0: none) of bank 0–3 in page 0–1's file; `pc::SavePages::write` stamps as below. Bank 3's stamp lives in the resume slot's file (quick: page 0's, autosave: page 1's), matching where bank 3 is written.

- [ ] **Step 1: Write the failing tests** (add to `main()`)

```cpp
void stampsLogBank() {
    const auto d=freshDirectory("stamp");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.showPage(1);
    const std::array<std::uint8_t,4> data{1,2,3,4};
    const auto before=std::int64_t(std::time(nullptr));
    pages.write(0x400+0x3c00*2+16,data);           // bank 2, page 2
    const auto t=pages.saveTime(1,2);
    check(t>=before&&t<=std::int64_t(std::time(nullptr)),"a log write stamps its bank with now");
    check(pages.saveTime(1,0)==0&&pages.saveTime(0,2)==0,"other banks and the other page keep no stamp");
    const auto bytes=fileBytes(d/"two.sav");
    std::int64_t onDisk=0;for(int i=7;i>=0;--i)onDisk=onDisk<<8|bytes[0xff00+16+i];
    check(onDisk==t,"the stamp is little-endian at 0xFF00 + 8*bank");
}
void stampsResumeInItsSlotsFile() {
    const auto d=freshDirectory("stampresume");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    pages.useResumeSlot(pc::ResumeSlot::autosave);
    const std::array<std::uint8_t,4> data{1,2,3,4};
    pages.write(0xb800,data);
    check(pages.saveTime(1,3)!=0&&pages.saveTime(0,3)==0,"bank 3 stamps the autosave's file");
}
void headerWriteStampsNothing() {
    const auto d=freshDirectory("stampheader");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    const std::array<std::uint8_t,4> data{1,2,3,4};
    pages.write(0x200,data);
    bool none=true;for(int b=0;b<4;++b)none=none&&pages.saveTime(0,b)==0;
    check(none,"a header write stamps no bank");
}
void stampsBothBanksOfASplitWrite() {
    const auto d=freshDirectory("stampsplit");
    pc::SavePages pages(d/"one.sav",d/"two.sav");
    std::vector<std::uint8_t> data(32,7);
    pages.write(0xb800-16,data);
    check(pages.saveTime(0,2)!=0&&pages.saveTime(0,3)!=0,"a write across bank 2 and 3 stamps both");
}
```

- [ ] **Step 2: Run them to see them fail**

Run: `distrobox enter vibecode -- sh -c 'cd /var/home/stabares/Descargas/dq4/DQIV-Recompiled-1.3 && cmake --build --preset linux64-game --target dqiv_save_tests && ./build64/dqiv_save_tests'`
Expected: compile error, `saveTime` is not a member of `pc::SavePages`.

- [ ] **Step 3: Implement** `saveTime` and the stamping in `SavePages::write`: for each part already split by the existing loop, every bank k (0–3) whose range `[0x400+k*0x3c00, 0x400+(k+1)*0x3c00)` the part overlaps gets `time(nullptr)` written through the same store that part went to (`pageStore(shown_)` for 0–2, `resumeStore(slot_)` for 3). Constants beside `resumeStart` in `SavePages.cpp`: `stampOffset=0xff00`, `bankStart=0x400`, `bankSize=0x3c00`.

- [ ] **Step 4: Run them to see them pass**

Same command. Expected: the suite's pass line, no `FAIL:` lines.

- [ ] **Step 5: Commit**

```bash
git add native/SavePages.hpp native/SavePages.cpp tests/save/save_pages_tests.cpp
git commit -m "Classic launch menu: save dates

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: The launch list's layout and drawing

**Files:**
- Modify: `native/BattleWindows.hpp`, `native/BattleWindows.cpp`, `game/src/main/menu/MenuManager.cpp` (clear `launch` each frame)
- Test: `tests/battle/battle_windows_tests.cpp`

**Interfaces:**
- Produces, in `BattleWindows.hpp`:

```cpp
// The launch menu (MaterielMenu_LOAD, Classic). shown: the commands (a field list) or the
// log list; the map and gold stay off. cards non-empty: the log list in one window, the
// pickable cards being the state's entries (card.entry their index).
struct BattleSaveCard {
    BattleText label,name,chapter,place,time,date;  // label: the number ("3") or "Quick save"; empty texts not drawn
    std::vector<std::uint8_t> face;                 // 48x48 portraitPalette indices, may be empty
    int entry=-1;                                   // its index in state.entries, -1 not pickable (drawn dimmed)
};
struct BattleLaunchMenu {bool shown=false;std::vector<BattleSaveCard> cards;};
```

  `BattleMenuState::launch` (cleared each frame in `MenuManager::execute` like `items`).
- Layout rules (DS units): the window 8 in from every side of the plan; header row 20 high (`state.title`); cards 30 high (2 rows of 12, padding 3), 2 apart; face 24×24 at the card's left (the 48×48 portrait halved); `windows[0]` the window, then one per visible card in order; `entries` one per state entry, an off-screen card's rect `{0,0,0,0}`. Visible cards: as many as fit; first visible = `clamp(cursorCard - visible/2, 0, n - visible)` where `cursorCard` is the card whose `entry == state.cursor`. ▲ at the window's top right when cards are hidden above, ▼ at its bottom right when hidden below. An empty plan when not even one card fits.
- `classicFieldMapShown` and `layoutBattleGold` are off while `launch.shown`.

- [ ] **Step 1: Write the failing tests** (helper `launchState(int cards,int pickable)`: kind `list`, `field=true`, `launch.shown=true`, `title=ink(80)`, cards with `label=ink(10)`, `name=ink(40)`, `time=ink(30)`, `date=ink(60)`, the last `pickable` cards entries 0.., `cursor=0`)

```cpp
void launchFitsEightAt720() {
    const auto layout=pc::layoutBattleMenu(launchState(8,8),1280,720);
    const int s=pc::battleScale(720);
    check(layout.windows.size()==9,"the window and eight cards at 720p");
    if(layout.windows.empty())return;
    check(layout.windows[0].x==8*s&&layout.windows[0].y==8*s,"8 in at the top left");
    check(layout.windows[0].x+layout.windows[0].w==1280-8*s,"8 in from the right");
    check(layout.entries.size()==8,"one hit area per pickable card");
}
void launchScrollsToCursor() {
    auto state=launchState(8,8);state.cursor=7;
    const auto layout=pc::layoutBattleMenu(state,640,480);
    check(layout.windows.size()<9&&layout.windows.size()>1,"fewer cards shown at 480p");
    if(layout.entries.size()<8)return;
    const auto& last=layout.entries[7];
    check(last.w>0&&inside(last,640,480),"the cursor's card in view");
    check(layout.entries[0].w==0,"the first card scrolled out");
}
void launchDimmedNotPickable() {
    const auto layout=pc::layoutBattleMenu(launchState(8,6),1280,720);
    check(layout.windows.size()==9,"the resume cards still drawn");
    check(layout.entries.size()==6,"hit areas only for the logs");
}
void launchWindowsInside() {
    for(auto [w,h]:{std::pair{640,480},{1280,720},{1920,1080}}) {
        const auto layout=pc::layoutBattleMenu(launchState(8,8),w,h);
        bool all=!layout.windows.empty();for(auto& r:layout.windows)all=all&&inside(r,w,h);
        check(all,"the launch windows inside the screen");
    }
}
void launchHidesMapAndGold() {
    auto state=launchState(0,0);state.gold=ink(20);
    check(!pc::classicFieldMapShown(state),"no map behind the launch menu");
    check(pc::layoutBattleGold(state,1280,720).picture.w==0,"no gold panel");
}
void launchDrawsCards() {
    auto state=launchState(8,8);state.launch.cards[2].face.assign(48*48,5);state.portraitPalette[5]=0xff123456;
    const auto layout=pc::layoutBattleMenu(state,1280,720);
    const auto pixels=pc::composeBattleMenu(state,layout,0);
    bool face=false;for(auto c:pixels)face=face||c==0xff123456;
    check(face,"a card draws its face");
}
```

- [ ] **Step 2: Run them to see them fail**

Run: `distrobox enter vibecode -- sh -c 'cd /var/home/stabares/Descargas/dq4/DQIV-Recompiled-1.3 && cmake --build --preset linux64-game --target dqiv_battle_tests && ./build64/dqiv_battle_tests'`
Expected: compile error, no member `launch` in `pc::BattleMenuState`.

- [ ] **Step 3: Implement** the structs; `planLaunch(state,width,height)` dispatched from `plan()` when `state.launch.shown && !state.launch.cards.empty()` (before the items check); its drawing in `composeBattleMenu` next to `composeItems`: header text, card frames (the chosen card's frame lighter, as the items panes mark the active pane), face, label and name and time on row 1, chapter, place and date (right-aligned) on row 2, dimmed cards with the palette's dim as disabled entries use, the animated cursor at the chosen card's left, ▲/▼. The commands (`launch.shown`, no cards) go through today's field list unchanged.

- [ ] **Step 4: Run them to see them pass**

Same command. Expected: `battle windows: all passed 0`.

- [ ] **Step 5: Commit**

```bash
git add native/BattleWindows.hpp native/BattleWindows.cpp tests/battle/battle_windows_tests.cpp game/src/main/menu/MenuManager.cpp
git commit -m "Classic launch menu: the list's layout and drawing

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: The game's cards and the commands panel

**Files:**
- Modify: `game/src/ov016/MaterielMenu_LOAD/MaterielMenu_LOAD.cpp`, `game/include/ov016/MaterielMenu_LOAD/MaterielMenu_LOAD.hpp`
- Modify: `game/src/main/menu/BattleMenuNative.cpp`, `game/include/ov015/btl/BattleMenuNative.hpp`
- Modify: `game/src/ov016/MenuTemplate/MenuTemplate_materiel.cpp` (`MATERIEL_DIARY_PAGE`'s array to 9: up to 8 rows)

**Interfaces:**
- Consumes: Task 1 `saveTime(page,bank)`; Task 2 `BattleSaveCard`, `BattleLaunchMenu`.
- Produces (in `BattleMenuNative`):
  - `pc::BattleText battleSaveDate(std::int64_t seconds)` — `YYYY-MM-DD HH:MM` local (`localtime`), empty for 0.
  - `const std::vector<std::uint8_t>& chapterLeadFace(int chapter)` — `portraitPixels` of the chapter's lead: chapter 1 Ragnar, 2 Alena, 3 Torneko, 4 Meena, 0 and 5+ the hero; the lead's `CharInitData` row looked up by its name message (`index + 0x50000000`) once, then its `.icon`. Empty when the data is not loaded.
  - `void publishLaunchCommands(int count, const int* texts, int active)` — the root entries as a field list (`beginFieldMenu(list,1)`, `launch.shown=true`, `endBattleMenu`), no party, no gold.
  - `void publishLaunchList(int title, const std::vector<pc::BattleSaveCard>& cards, int entries, int active)`.
- In `MaterielMenu_LOAD.cpp` (anonymous namespace): `std::vector<pc::BattleSaveCard> s_cards; void readAllCards(int menuMode);` — for page 0 then 1: `showPage(p)`, `getCatalogView()` into 3 log cards (name via `battleText((int)view.name_)` rendered now; chapter `0xa0000323+chapter`, chapter ≥ 5 also the level as the slot draws it; place `town` or `0x80000032` when -1; time `HHH:MM` as `drawJournalSlotNumbered`; empty: label + `0xa0000321`, no face; `savetype_` 4: no chapter/place), and its resume save through `useResumeSlot` + `getResumeView` (labels `pc::autosaveLabel`/`pc::quickSaveLabel`); dates from `saveTime(p, k)`; then `useResumeSlot(quick)`, `showPage(sessionPage())`, and `updateActiveDiary()` so `diary_` is the session page's again. Order in `s_cards`: autosave, quick save, logs 1–6. `entry`: Continue all 0–7; Create/Delete the logs 0–5, resume cards -1.

- [ ] **Step 1: Build check that the gate is off by default** — with `battleMenus=0` the launch menu draws as today. Run the build; expected: builds.
- [ ] **Step 2: Implement** the helpers and, under `classicBattleMenus()`, in `menuDraw`: `LOAD_MODESELECT` calls `publishLaunchCommands` instead of the DS drawing (texts: today's root messages plus the settings, achievements and quit buffers); `LOAD_DATASELECT` calls `publishLaunchList(rootMessage[menuMode_], s_cards, menuMode_==0?8:6, dataItem_.active_)`. `readAllCards` runs on entering `LOAD_DATASELECT` (from `LOAD_DATACHECK`) and after a create or delete returns to it.
- [ ] **Step 3: Build** with the command in Global Constraints. Expected: no errors or new warnings.
- [ ] **Step 4: Commit**

```bash
git add game/src/ov016/MaterielMenu_LOAD game/include/ov016/MaterielMenu_LOAD game/src/main/menu/BattleMenuNative.cpp game/include/ov015/btl/BattleMenuNative.hpp game/src/ov016/MenuTemplate/MenuTemplate_materiel.cpp
git commit -m "Classic launch menu: the cards and the commands panel

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Picking a card, and the screen checks

**Files:**
- Modify: `game/src/ov016/MaterielMenu_LOAD/MaterielMenu_LOAD.cpp`
- Modify: `native/GameWindow.cpp` only if the list does not draw through the existing `layoutBattleMenu` path

**Interfaces:**
- Consumes: Task 3 `s_cards`, `readAllCards`.

- [ ] **Step 1: Implement**, under `classicBattleMenus()`:
  - `menuExecute`: `MATERIEL_DIARY_PAGE(&dataItem_, menuMode_==0?8:6)`.
  - `rootUpdate` and `dataSelectUpdate` read input through `battleMenuSelect`-style `battleListInput` as the Items screens do; no L/R.
  - `dataSelectUpdate` on a pick of row r: Continue rows 0/1 → `useResumeSlot(autosave/quick)` then `LOAD_RESUME`, which loads that slot instead of `resumeSlotShown()`; a log row (Continue r−2, Create/Delete r) = log n (0–5) → `showPage(n/3)`, `updateActiveDiary()`, `activeDiaryNo_ = n%3`, then today's branch for `menuMode_` on that slot (so `setCatalogMacro` and the yes/no texts name the right log).
  - `case 5` (to last) and `case 6` (to first) use the 8 or 6 rows.
  - The cursor on opening: the card of `getCatalogRecent()` on the session page, or the first used card.
  - Back from a cancelled yes/no or a failed load: `showPage(sessionPage())` before the list shows again.
- [ ] **Step 2: Build.** Expected: no errors.
- [ ] **Step 3: Screen checks** (`tests/screen/game.sh start 64 battleMenus=1 wide=1`, 1280x720; screenshots in the scratchpad), each with its expected result:
  - the commands in a Classic panel at the top left, no map, no gold;
  - Continue: Autosave, Quick save, Logs 1–6 with faces, chapter, place, time; dates on saves made after this change, none on older ones;
  - load log 5, walk, save at a church into log 5, quit, relaunch: log 5 has the new time and date, log 2 unchanged;
  - resume the autosave: the game resumes where the autosave was made;
  - Create: resume cards dimmed, the cursor never lands on them; create into an empty log 4, then the list shows it;
  - Delete a log on page 1: gone from the list on return;
  - 640×480 window: the list scrolls with the cursor;
  - `battleMenus=0`: today's menu, pages of three, L/R work.
- [ ] **Step 4: Run both unit suites.** `dqiv_save_tests` and `dqiv_battle_tests`; expected: both pass lines.
- [ ] **Step 5: Commit**

```bash
git add game/src/ov016/MaterielMenu_LOAD native/GameWindow.cpp
git commit -m "Classic launch menu: picking a card

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```
