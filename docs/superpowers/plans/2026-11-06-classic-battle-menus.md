# Classic battle menus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The DS bottom-screen battle menus redrawn as classic DQ windows along the bottom of the game window, at full frame rate, behind a setting that keeps the original menus untouched.

**Architecture:** Each `BattleMenu_*::menuDraw` (game side) fills a `pc::BattleMenuState` instead of drawing; the native `BattleWindows` module lays it out, composes one picture and draws it on the interface layer every frame; a native input step after `MenuItem::execInput()` turns directions and mouse clicks into the game's own `active_` / `result_`.

**Tech Stack:** C++20, CMake presets (`linux64-game`), SDL3, OpenGL (`GpuRenderer::drawOverlay`), the decompiled game's text engine (`drawBgText`).

**Spec:** `docs/superpowers/specs/2026-11-06-classic-battle-menus-design.md`

## Global Constraints

- Every game-side change is under `#ifdef DQIV_NATIVE` and only acts when `pc::gameSettings().classicBattleMenus` is true; with it false, the code that runs is today's.
- The setting: `GameSettings::classicBattleMenus`, key `classicBattleMenus`, Game page, label "Battle menus", values "Classic" / "Original", through `pick(language,en,fr,de,it,es)` (Settings.cpp:178; Japanese shows the English text, as every other setting does). Default `false` until Task 7, then `true`.
- Scale: `max(1, windowHeight / 270)`; layout in DS pixels; margin 8 DS pixels from the bottom and left of the game window; 4 DS pixels between windows.
- Window look: fill `0xff101830` (navy), border white `0xffffffff`, 2 pixels, corners cut by 2 pixels; disabled text drawn at 50% (`0xff808080` multiply); cursor ▶ on 20 frames, off 10 while waiting.
- Text: the game's 12-dot font through `drawBgText` into a private `BgCharBuffer`, never `g_menuWindowChars`; the font size is restored after.
- Match the surrounding code style: `native/` uses compact one-line bodies and `pc::` names; game files use the decompilation's style with `// ` comments for the native why.
- Build/test: `distrobox enter vibecode -- sh -c 'cd <repo> && cmake --build --preset linux64-game'`.

## Notes on the spec

Two refinements found while planning; the behaviour is as specified:

- `dqiv_core` (where `native/` lives) cannot call game code, so entry text is rendered on the game side (Task 3, `BattleMenuNative.cpp`) and handed over as small palette-index bitmaps with the 16 colours of BG palette 15; the native side only composes pictures.
- The DS touch rectangles stay set up; instead `GameWindow` stops sending touches to the DS while classic battle windows are shown (Task 2), which keeps the game's menu code unchanged.

## Review Focus

1. A menu closes in the same frame another opens (Commands → Spells): the windows must show only the new one, never both or a stale one — Task 3 Step 2 (cleared each frame), Task 3 Step 6 screen check and Task 1 test `noneHidesPicture`.
2. A spell list's last page with an odd number of spells: Down from the last left entry must not land on an empty slot — Task 1 test `cursorSkipsEmptySlot`.
3. A very narrow game window (e.g. 640×480 with a long German spell name): windows must stay inside the window, entries cut with "…" — Task 1 test `windowsStayInside`.
4. Switching the setting to Original in the middle of a battle menu: the DS menu must come back the next frame with its cursor where the classic one was — Task 3 Step 6 screen check.
5. Mouse moving over the battle scene with classic windows shown: no DS touch may reach the game (no accidental cancel through `BATTLE_RECT_ENEMY`) — Task 2 test `pointerOutsideWindows`, plus Task 3 Step 6.

---

### Task 1: `BattleWindows` core (state, layout, cursor, picture) and its tests

**Files:**
- Create: `native/BattleWindows.hpp`, `native/BattleWindows.cpp`
- Create: `tests/battle/battle_windows_tests.cpp`
- Modify: `CMakeLists.txt` (add `native/BattleWindows.cpp` to `dqiv_core`; add `dqiv_battle_tests` like `dqiv_save_tests` at line 158)

**Interfaces:**
- Produces (all in `namespace pc`, `native/BattleWindows.hpp`):

```cpp
enum class BattleMenuKind {none,root,commands,targetEnemy,targetParty,spells,items,
    tacticsMember,tacticsSelect,formation,message,stadiumAbort};
// A text drawn by the game's font: palette indices (0 transparent), one byte per pixel.
struct BattleText {std::vector<std::uint8_t> ink;int width=0,height=0;};
struct BattleMenuEntry {
    BattleText label,right;  // right: MP cost, count or mark (may be empty)
    short index=0;           // the game's MenuItem::active_ for this entry
    bool enabled=true;
};
struct BattleMenuState {
    BattleMenuKind kind=BattleMenuKind::none;
    BattleText title,footer;          // member name; "MP left" line (may be empty)
    std::vector<BattleMenuEntry> entries;  // the current page, row-major
    int columns=1;                    // 1 list, 2 two columns
    int cursor=-1;                    // entries index of the game's active_
    int page=0,pages=1;
    bool waiting=true;
    std::array<std::uint32_t,16> palette{};  // ARGB of BG palette 15
    std::uint32_t revision=0;         // changes whenever anything above changes (Task 3 bumps it)
};
BattleMenuState& battleMenuState();       // filled by the game each frame
struct BattleRect {int x=0,y=0,w=0,h=0;}; // game window pixels
struct BattleLayout {
    int scale=1;
    std::vector<BattleRect> windows;      // frames, in drawing order
    std::vector<BattleRect> entries;      // one per state entry (hit areas)
    BattleRect picture;                   // where the composed picture goes
};
int battleScale(int windowHeight);                       // max(1, h / 270)
BattleLayout layoutBattleMenu(const BattleMenuState&,int windowWidth,int windowHeight);
// The entry index after a direction (0 up, 1 down, 2 left, 3 right), or -1 to pass it to the game (page turn).
int moveBattleCursor(const BattleMenuState&,int cursor,int direction);
// Composed at DS scale (picture.w / scale × picture.h / scale), premultiplied ARGB; empty when kind is none.
std::vector<std::uint32_t> composeBattleMenu(const BattleMenuState&,const BattleLayout&,int blinkFrame);
// The entry under a window-pixel point, or -1.
int battleEntryAt(const BattleLayout&,int x,int y);
```

- [ ] **Step 1: Write the failing tests** in `tests/battle/battle_windows_tests.cpp` (same `check()` style as `tests/save/save_pages_tests.cpp`), with a helper `entry(width)` making a `BattleMenuEntry` whose label is `width`×12 ink:
  - `scaleSteps`: `battleScale(720)==2`, `battleScale(1080)==4`, `battleScale(2160)==8`, `battleScale(200)==1`.
  - `rootAtBottomLeft`: root with 2 entries, 1920×1080 → `windows[0].x==8*4`, `windows[0].y+windows[0].h==1080-8*4`.
  - `listBesideCommands`: spells state (columns 2, 8 entries) → its window's `x` ≥ commands window right + 4×scale (the commands window is laid out for `spells`/`items`/targets too, from the title).
  - `windowsStayInside`: 640×480, an entry 400 DS pixels wide → every window inside `[0,640)×[0,480)`.
  - `cursorSkipsEmptySlot`: columns 2, 5 entries, cursor 3 (row 1, right), down → 3 stays (no entry 5); cursor 2, down → 4.
  - `listWraps`: columns 1, 4 entries: up from 0 → 3, down from 3 → 0.
  - `pageTurnPasses`: columns 2, 6 entries, pages 3: left from cursor 0 → -1; right from cursor 1 → -1; left from 1 → 0; right from 0 → 1. With pages 1: left from 0 → 0, right from 1 → 1 (nothing to pass).
  - `noneHidesPicture`: kind none → `composeBattleMenu(...)` empty and `layout.windows` empty.
  - `hitTest`: `battleEntryAt(layout, centre of entries[2])==2`, a point outside every window → -1.
- [ ] **Step 2: Run them to see them fail** — `distrobox enter vibecode -- sh -c 'cd <repo> && cmake --build --preset linux64-game --target dqiv_battle_tests'` → fails to compile (no `BattleWindows.hpp`).
- [ ] **Step 3: Implement the module.** Window sizes in DS pixels: padding 8 around text, entry row height 16, ▶ column 12, a right column as wide as the widest `right` + 8; two-column lists put columns side by side with 8 between. Commands window title drawn on the top border (DQ style). `composeBattleMenu` maps ink index `i` through `palette[i]`, multiplies disabled entries by `0xff808080`, draws the ▶ (a 7×7 triangle in palette colour 1) when `waiting` and `blinkFrame%30<20`, "◀ n/m ▶" as small triangles plus the footer when `pages>1`.
- [ ] **Step 4: Run the tests** — same command plus `&& ./build64/dqiv_battle_tests` → no `FAIL:` lines, exit 0.
- [ ] **Step 5: Commit** — `git add native/BattleWindows.* tests/battle CMakeLists.txt && git commit -m "Battle windows: layout, cursor and picture"`.

### Task 2: The setting, the overlay and the mouse

**Files:**
- Modify: `native/Settings.hpp` (field `bool classicBattleMenus=false;` in `GameSettings`; `settingBattleMenus` in `SettingItem` after `settingBattleZoom`)
- Modify: `native/Settings.cpp` (load line ~48, save line ~71, label ~283, value ~322, help ~356, change ~395; and the Game page's item list)
- Modify: `native/GpuRenderer.hpp/.cpp:471` (`drawOverlay` gets `int drawWidth=-1,int drawHeight=-1`; -1 means the picture's own size)
- Modify: `native/GameWindow.hpp/.cpp` (`drawInterface` draws the battle picture; the touch code at line 256)
- Modify: `native/BattleWindows.hpp/.cpp`, `tests/battle/battle_windows_tests.cpp`

**Interfaces:**
- Consumes: Task 1 `battleMenuState`, `layoutBattleMenu`, `composeBattleMenu`, `battleEntryAt`.
- Produces:

```cpp
// native/BattleWindows.hpp
struct BattlePointer {int entry=-1;bool confirm=false,cancel=false;};
// GameWindow: the mouse this frame (window pixels, buttons pressed this frame).
void setBattlePointer(int x,int y,bool leftPressed,bool rightPressed);
// The game: the pointer against the last drawn layout; clears the presses.
BattlePointer takeBattlePointer();
// True while classic battle windows are on screen (GameWindow sends no DS touch then).
bool battleWindowsShown();
// The layout the pointer is resolved against (drawBattleWindows sets it each frame).
void rememberBattleLayout(const BattleLayout&);
// native/GameWindow.cpp (needs the GpuRenderer): lays out, composes when changed, draws.
void GameWindow::drawBattleWindows(int windowWidth,int windowHeight);
```

- [ ] **Step 1: Failing test** `pointerOutsideWindows`: `rememberBattleLayout(layoutBattleMenu(root state, 1920, 1080))`, then `setBattlePointer(5,5,true,false)` → `takeBattlePointer().entry==-1`, `confirm==false` (a click outside a window is no confirm); a click on entry 1 → `entry==1`, `confirm==true`; a second `takeBattlePointer()` → `confirm==false`.
- [ ] **Step 2: Run** — Task 1 Step 4 command → `FAIL: pointerOutsideWindows` (or no symbol).
- [ ] **Step 3: Implement.** `GameWindow::drawBattleWindows` (called from `drawInterface`) lays out, calls `rememberBattleLayout`, composes when the state, window size or blink phase changed, and calls `drawOverlay(overlayBattle, pixels, picture.w/scale, picture.h/scale, changed, picture.x, picture.y, windowWidth, windowHeight, picture.w, picture.h)` (a new overlay id beside `overlayFps`). The state needs a cheap change check: a `std::uint32_t revision` in `BattleMenuState`, bumped by the game side whenever an entry, the cursor or the page differs from the last frame. In `GameWindow::pumpEvents` (line 256) pass `down&&!battleWindowsShown()` to `setTouch` and feed `setBattlePointer` with the raw mouse and the press edges. Settings texts, each through `pick(language,en,fr,de,it,es)`:
  - label: "Battle menus", "Menus de combat", "Kampfmenüs", "Menu di battaglia", "Menús de combate";
  - Classic: "Classic", "Classiques", "Klassisch", "Classici", "Clásicos"; Original: "Original", "Originaux", "Original", "Originali", "Originales";
  - help: "Battles: menus as windows along the bottom, like the console games (Original: the DS touch menus).", "Combats : menus en fenêtres en bas de l'écran, comme sur console (Originaux : les menus tactiles de la DS).", "Kämpfe: Menüs als Fenster am unteren Rand wie auf Konsolen (Original: die Touch-Menüs der DS).", "Battaglie: menu a finestre in basso, come su console (Originali: i menu touch del DS).", "Combates: menús en ventanas abajo, como en consola (Originales: los menús táctiles de la DS).".
- [ ] **Step 4: Run tests and build the game** — Task 1 Step 4 command, then the full build → both succeed.
- [ ] **Step 5: Screen check** — `tests/screen/game.sh start 64`, open Settings from the title, shot: "Battle menus: Original" on the Game page; toggle it, quit, start again: it stays Classic (`settings.ini` has `classicBattleMenus=1`).
- [ ] **Step 6: Commit** — "Battle menus setting; battle windows on the interface layer".

### Task 3: Game-side helpers, then Root and Commands

**Files:**
- Create: `game/src/ov015/btl/BattleMenuNative.cpp`, `game/include/ov015/btl/BattleMenuNative.hpp`
- Modify: `game/src/main/menu/MenuManager.cpp:110` (clear the state before `s_currentMenu->draw`)
- Modify: `game/src/ov015/btl/BattleMenu_ROOT.cpp`, `game/src/ov015/btl/BattleMenu_ACTIONMENU.cpp`

**Interfaces:**
- Consumes: Task 1 types; Task 2 `takeBattlePointer`, `gameSettings().classicBattleMenus`.
- Produces (game side, `#ifdef DQIV_NATIVE`):

```cpp
bool classicBattleMenus();                          // the setting
pc::BattleText battleText(int value);               // a message value (as drawMenuText takes it), cached by value and language
pc::BattleText battleNumber(int number);            // digits, for MP costs and counts
void beginBattleMenu(pc::BattleMenuKind kind,int columns); // resets battleMenuState(), fills palette from getBgPaletteBuffer(0) colours 0xf0..0xff
void addBattleEntry(int text,short index,bool enabled,int right=0,bool rightIsNumber=false);
void setBattleCursor(short active);                 // finds the entry with that index
// After execInput()/menuSelect: directions and the pointer → active_/result_. Returns menuSelect's code (0 none, 2 confirm, 3 cancel, 4-7 moved).
int battleMenuSelect(menu::MenuItem& item,CursorMoveBase& cursor);
```

- [ ] **Step 1: Implement the helpers.** `battleText`: extract like `drawMenuText` (MenuBgDraw.cpp:114–150, including the German exception and the prefix case table), `setBgTextFontSize(12)`, `drawBgText` into a private 4bpp `BgCharBuffer` of 32×2 tiles (`initCharBuffer`), width from `getBgTextWidth()`, unpack to one byte per pixel; restore the previous size from `g_BgText.size_`. `beginBattleMenu`…`setBattleCursor` build into a scratch state; the Step 2 clear and the end of `MenuManager`'s draw compare it with the published one and bump `revision` when kind, entry texts/indexes/enabled, cursor or page differ. `battleMenuSelect`: call `MenuUpdate_Assist::menuSelect`; on 4–7 undo its move (restore the saved `active_`) and apply `pc::moveBattleCursor` (pass through when it returns -1); a pointer entry sets `active_` to that entry's index; pointer confirm/cancel return 2/3 the way `menuSelect` reports them.
- [ ] **Step 2: Clear each frame** — in `MenuManager.cpp` before `s_currentMenu->draw(0, 0xc0)`: `pc::battleMenuState().kind = pc::BattleMenuKind::none;` (under `DQIV_NATIVE`).
- [ ] **Step 3: Root hook** — `BattleMenu_ROOT::menuDraw`: when `classicBattleMenus()`, `beginBattleMenu(root,1)`, entries in the order of `drawBattleRootMenu` (command bits 1, 2, 4, 8 → their names `0x80000000…0x80000003` as `drawBattleCommandIcon` maps them; simple menu: Fight, Flee with game index 0 and 1), `setBattleCursor(menuItem_.active_)`, return without DS drawing. `menuUpdate`: `MenuUpdate_Assist::menuSelect` → `battleMenuSelect` when classic.
- [ ] **Step 4: Commands hook** — `BattleMenu_ACTIONMENU`: title `battleText(chara + 0x50000000)` (the PARTY name), entries from bits 0x10/0x20/0x40/0x80 of its flag (names `0x80000004…0x80000007`), `enabled` from the same flag the DS greys with.
- [ ] **Step 5: Build** — full build succeeds.
- [ ] **Step 6: Screen checks** (a battle save in `~/.cache/dq4-screen`; make one with the debug encounter if none): Classic on → shots of Root and Commands; arrows move the ▶, Enter chooses Attack, Escape goes back; switch to Original in the settings mid-menu → the DS menu returns next frame with its cursor on the same command; mouse over the monsters and click → nothing happens (Review Focus 5).
- [ ] **Step 7: Commit** — "Classic battle menus: root and commands".

### Task 4: Target menus

**Files:**
- Modify: `game/src/ov015/btl/BattleMenu_ATTACK.cpp`, `BattleMenu_MAGIC2ENEMY.cpp`, `BattleMenu_MAGIC2PARTY.cpp`, `BattleMenu_ITEMUSE2ENEMY.cpp`, `BattleMenu_ITEMUSE2PARTY.cpp`

**Interfaces:**
- Consumes: Task 3 helpers.

- [ ] **Step 1: Enemy targets** — kind `targetEnemy`, one entry per living group in game order: label the group name the plates use (`drawMonsterNamePlates`, BattleMenuDrawCommon.cpp:302), right `battleNumber(count)` when more than one; index = the value the menu's `menuItem_.active_` takes for that group.
- [ ] **Step 2: Party targets** — kind `targetParty`, member names (`index + 0x50000000`) in party order.
- [ ] **Step 3: Input** — each menu's `menuSelect` call goes through `battleMenuSelect` when classic.
- [ ] **Step 4: Build and screen check** — Attack with 2+ groups: list "Slime ×3"; Heal on a member; cancel returns to Commands.
- [ ] **Step 5: Commit** — "Classic battle menus: targets".

### Task 5: Spells and items

**Files:**
- Modify: `game/src/ov015/btl/BattleMenu_MAGIC.cpp`, `BattleMenu_ITEM.cpp`, `BattleMenu_ITEMUSEEQUIP.cpp`

- [ ] **Step 1: Spells** — kind `spells`, columns 2, entries = the current page of `haveAction_` (`navigator_.getPageNo()`, the page size the navigator uses), label `getActionNameMessage(action)`, right `battleNumber(mp cost)` as `drawBattleMpCost` gets it, `enabled` = enough MP (as the DS greys); footer "MP" + caster's MP; `page`/`pages` from the navigator.
- [ ] **Step 2: Items** — kind `items`, columns 2, label `item + 0x40000000`, right the equipped mark (`0xa0000041`) where `drawBattleItemEntry` puts it; `ITEMUSEEQUIP` likewise.
- [ ] **Step 3: Input** through `battleMenuSelect`; Left/Right at the column edges pass to the navigator (page turn).
- [ ] **Step 4: Build and screen check** — a caster with several pages: page with an odd count (Review Focus 2), page turn both ways, a spell greyed with too little MP.
- [ ] **Step 5: Commit** — "Classic battle menus: spells and items".

### Task 6: Tactics, formation and the rest

**Files:**
- Modify: `game/src/ov015/btl/BattleMenu_TACTICSMENU.cpp`, `BattleMenu_ARRAYMENU.cpp`, `BattleMenu_ARRAY_CHANGE.cpp`, `BattleMenu_ARRAY_ALL.cpp`, `BattleMenu_NGMESSAGE.cpp`, `BattleMenu_StadiumAbort.cpp`

- [ ] **Step 1: Tactics** — `tacticsMember` (member names) then `tacticsSelect` (the tactic names `drawTacticsSelectMenu` draws, current one as `right`).
- [ ] **Step 2: Formation** — `formation` lists from `drawFormationMenu`, `drawFormationChangeMemberMenu`, `drawFormationChangeTargetMenu`, `drawFormationAllMenu` (order shown as `right` numbers).
- [ ] **Step 3: Message and stadium** — `message`: one entry-less window with `title = battleText(messageID_)`; `stadiumAbort`: its entries.
- [ ] **Step 4: Build and screen check** — late-game save with Formation/Tactics (or the debug flag `0x132`): each menu shown, choices still work.
- [ ] **Step 5: Commit** — "Classic battle menus: tactics, formation, messages".

### Task 7: Original untouched, then Classic by default

**Files:**
- Modify: `native/Settings.hpp` (`classicBattleMenus=true`)
- Modify: `README.md` (one line under the settings list, if it lists settings)

- [ ] **Step 1: Original regression** — build the commit before Task 1 and the current one; with Original, take the same Root, Commands, Spells and target shots from the same save and input script in both; `cmp` the PNGs → identical.
- [ ] **Step 2: Default** — set the default to `true`; build; a fresh `settings.ini` starts Classic.
- [ ] **Step 3: Commit** — "Classic battle menus by default".
