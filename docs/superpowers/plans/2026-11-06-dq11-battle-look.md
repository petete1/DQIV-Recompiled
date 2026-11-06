# Classic battle menus, the DQ11 look — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Classic's battle windows take DQ11's layout and feel: brown command panel bottom left, party panel on the right, restyled battle message window.

**Architecture:** The game side publishes three groups in `pc::battleMenuState()` each frame (menu, party, message); `native/BattleWindows` lays out and composes each group into its own picture at DS-pixel scale; `GameWindow::drawBattleWindows` draws the three pictures on the overlay. The DS status windows and battle message window are not drawn in Classic.

**Tech Stack:** C++20, CMake preset `linux64-game` (distrobox `vibecode`), the game's text engine.

**Spec:** `docs/superpowers/specs/2026-11-06-dq11-battle-look-design.md`

## Global Constraints

- Only `battleMenus==1` changes; Original (0) and Overlay (2) run today's code.
- Colours, sizes and placement exactly as the spec's Look and Layout sections.
- Build: `distrobox enter vibecode -- sh -c 'cd <repo> && cmake --build --preset linux64-game'`; tests: `--target dqiv_battle_tests && ./build64/dqiv_battle_tests`.
- Screen checks with `tests/screen/game.sh start 64 battleMenus=1 cheatBattleInvincible=1`.

## Review Focus

1. A long message line in a narrow window: the panel stays inside the window, the line is cut, not wrapped over the border — Task 4 test `messageInsideWindow`.
2. Four members in a 640×480 window: the party rows stay inside — Task 3 test `partyInsideWindow`.
3. HP 0 or max 0: bars don't divide by zero or overflow — Task 3 test `barWidths`.
4. A battle message while a command menu is open (a blocked choice): both shown, message above the menu group — Task 4 screen check.
5. Original/Overlay untouched — Task 5 comparison.

---

### Task 1: The DQ11 panel and cursor

**Files:** `native/BattleWindows.cpp` (`frame`, `triangle` → `panel`, `hand`), `tests/battle/battle_windows_tests.cpp`

- [ ] Failing test `panelLook`: compose a root state (2 entries) → pixel (0, h/2) of the picture is transparent-or-border, pixel (2, h/2) is the border `0xffb89870`, pixel (6, h/2) is the fill `0xe0281810`; somewhere in the cursor's column a pixel is `0xfff0c040` (blink frame 0).
- [ ] Implement: fill, inset border, 1-pixel rounded corners, 5×5 corner curls; the 11×7 hand (outline `0xff806010`) replacing the ▶ (cursor column 14 wide); text colour cream via the palette's text index mapped to `0xfff4ead8`.
- [ ] Tests pass; commit "Classic battle menus: the DQ11 panel and cursor".

### Task 2: Commands in two columns

**Files:** `game/src/ov015/btl/BattleMenu_ACTIONMENU.cpp`

- [ ] `beginBattleMenu(commands, 2)`, no title; entries in the order Attack(0), Defend(3), Spells(1), Items(2) so the columns read Attack/Spells and Defend/Items; the cursor and game indexes unchanged.
- [ ] Build; screen check (Commands: two columns, the hand moves Attack→Spells on Down, Attack→Defend on Right); commit.

### Task 3: Party panel

**Files:** `native/BattleWindows.hpp/.cpp`, `game/src/ov015/btl/BattleMenuNative.cpp/.hpp`, `game/src/ov015/btl/BattleMenuSub_HISTORY.cpp`, `game/src/main/menu/MenuManager.cpp` (clear), `native/GameWindow.cpp`, tests

**Interfaces:**
```cpp
struct BattlePartyRow {BattleText name,level;std::vector<std::uint8_t> portrait;  // 32x32 indices (0 transparent)
    int hp=0,hpMax=0,mp=0,mpMax=0;int colour=0;bool active=false;};             // colour: 0 normal, 1 yellow, 2 red
// in BattleMenuState: std::vector<BattlePartyRow> party; std::array<std::uint32_t,256> portraitPalette{};
struct BattleGroupLayout {int scale=1;BattleRect picture;};
BattleGroupLayout layoutBattleParty(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleParty(const BattleMenuState&,const BattleGroupLayout&);
int battleBarWidth(int value,int max,int width);   // clamped 0..width; 0 when max <= 0
```
- [ ] Failing tests `partyInsideWindow` (4 rows, 640×480 and 1280×720: picture inside the window, right edge 8×scale from the window's), `barWidths` (`battleBarWidth(50,100,64)==32`, `(0,0,64)==0`, `(150,100,64)==64`, `(-5,100,64)==0`).
- [ ] Implement layout (right edge, rows of 40 DS pixels, top at the window's middle) and compose (strip with left fade, portrait, name, HP/MP bars and numbers via `BattleText` digits drawn by the game side, level or condition).
- [ ] Game side `publishBattleParty()` in `HISTORY::menuDraw` when classic (skips `drawBattleHistory`): rows from `getCarriageOutCount()`, `memberHP_/memberMP_`, maxima, `getHPColor` (colour index by the game's yellow/red), `getCondition` (non-zero: that message as the level text), portrait from `getMenuIconData(0xf1000000, iconIndex_)` uncompressed, palette from `getBgPaletteBuffer(0)` (main BG) — check colours on screen, active = `activeChara_`. Numbers as `battleNumber`, carried in the row as texts (add `BattleText hpText, mpText`).
- [ ] Clear `party` with `kind` in `MenuManager::execute`; `GameWindow::drawBattleWindows` draws the party picture with its own overlay id whenever `party` is non-empty.
- [ ] Tests pass; build; screen check (portraits' colours, HP counting down after a hit, no DS status windows); commit.

### Task 4: Message window

**Files:** as Task 3 plus `game/src/main/menu/MessageWindow.cpp`

**Interfaces:**
```cpp
struct BattleMessageLine {BattleText text;int y=0;};   // y: DS pixels from the window's top
// in BattleMenuState: struct {bool shown=false,waiting=false;std::vector<BattleMessageLine> lines;BattleText name;int height=0;} message;
BattleGroupLayout layoutBattleMessage(const BattleMenuState&,int windowWidth,int windowHeight);
std::vector<std::uint32_t> composeBattleMessage(const BattleMenuState&,const BattleGroupLayout&,int blinkFrame);
```
- [ ] Failing test `messageInsideWindow`: a 400-wide line at 640×480 → picture inside the window, centred (left margin == right margin ±scale), width 240×scale or the window's width − 16×scale, whichever is smaller.
- [ ] Implement layout (bottom centre, 8 from the bottom; above the command group when both show) and compose (panel, lines cut at the panel's inner width, ▼ blinking when waiting, name on the top border).
- [ ] Game side: in `MessageWindow::draw`, when classic and in battle (`g_GlobalDQ4.unkfunc_02058114(BATTLE_PART)`): publish the lines (each `MenuTextDisplay`'s 4-bit `charData_` unpacked up to `visibleWidth_`, y = line y − window y), the name plate, `isWaitTrigger()`, then return without enabling the frame, lines or window 0.
- [ ] Tests pass; build; screen check (a message typing out and waiting, the ▼, no DS message window); commit.

### Task 5: Original and Overlay untouched

- [ ] Original pixel comparison of the menu area against the build before Task 1 (cursor frame aside); Overlay shots unchanged (visual). Ledger.
