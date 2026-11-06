# Overlay Menus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A third battle menus value, Overlay: the game's own menus drawn as on the DS but on the port's overlay, every frame, each menu placed anywhere on the wide window.

**Architecture:** Each open menu draws into its own copy of the menu BG buffers (redirected in `MenuSubManager::draw`) and menu sprites go to two full-screen OAM buffers, so nothing reaches the DS screens. The native side renders each layer with the DS tiles and palettes, places it by a per-menu rule, draws it on the overlay, and maps the mouse back to the layer's DS coordinates for the game's touch.

**Tech Stack:** C++20, CMake preset `linux64-game` (in distrobox `vibecode`), SDL3, OpenGL `GpuRenderer::drawOverlay`, the decompiled menu code (`game/src/main/menu`).

**Spec:** `docs/superpowers/specs/2026-11-06-overlay-menus-design.md`

**Base:** branch `classic-battle-menus` (the setting and `BattleWindows` live there).

## Global Constraints

- Setting: `GameSettings::battleMenus` (int, 0 Original, 1 Classic, 2 Overlay), key `battleMenus`; an old `classicBattleMenus=1` line loads as 1. Values shown "Original" / "Classic" / "Overlay" (`pick`: "Overlay", "Superposés", "Überlagert", "Sovrapposti", "Superpuestos"; Japanese table "オーバーレイ").
- Original (0) runs exactly today's code: every new branch is `#ifdef DQIV_NATIVE` and tests `overlayMenus()` or `classicBattleMenus()`.
- DS pixel scale on the window = the presented picture's DS pixel size (`canvasPixels` height / 384), not `battleScale`.
- Layers: at most 8 (`MenuSubManager` has 8 slots); a 9th menu in a frame keeps today's path.
- Layout of the menu buffers: window map and back map and icon map are 32×48 `unsigned short` cells; window characters 32×48 4-bit tiles (0x20 bytes each); window map characters ≥ 0x80 come from the layer's character buffer at tile `(c - 0x80) + (row >= 24 ? 24*32 : 0)`, others from BG VRAM.
- Build: `distrobox enter vibecode -- sh -c 'cd <repo> && cmake --build --preset linux64-game'`; tests: add `&& cmake --build --preset linux64-game --target dqiv_battle_tests && ./build64/dqiv_battle_tests`.
- Screen checks: `tests/screen/game.sh start 64 battleMenus=2 windowSize=0` (settings by start arguments, never through the menus).

## Review Focus

1. Two menus whose windows overlap on the DS (Commands and the spell list) placed apart: both must show whole, no hole or stray piece — Task 2 test `layersKeepTheirOwnCells`, Task 5 screen check.
2. A menu closes while another opens in the same frame: its layer must disappear that frame, never linger — Task 3 test step (layer freed in `menuClose`), Task 5 screen check.
3. A click on a moved window must hit the game's touch rectangle under it, not what lies at that window spot on the DS — Task 2 test `pointerMapsBackToLayer`.
4. The ▶ cursor and faces (global `g_MenuDisplays` sprites) must follow their menu's window — Task 2 test `spriteGoesToContainingLayer`.
5. Original must be pixel-identical to the build before this plan — Task 6 comparison.

---

### Task 1: The setting's third value

**Files:**
- Modify: `native/Settings.hpp` (`int battleMenus=0;` replaces `bool classicBattleMenus`), `native/Settings.cpp` (load with the old key, save, value text, cycle 0→1→2→0, help line gains "Overlay: the DS menus anywhere on the screen.")
- Modify: `game/src/ov015/btl/BattleMenuNative.cpp` (`classicBattleMenus()` returns `battleMenus==1`; new `bool overlayMenus()` returns `battleMenus==2`), its header
- Modify: `native/GameWindow.cpp` (`drawBattleWindows` tests `battleMenus==1`)
- Test: `tests/battle/battle_windows_tests.cpp`

**Interfaces:** Produces `GameSettings::battleMenus`, `bool overlayMenus()` (game side, `BattleMenuNative.hpp`).

- [ ] **Step 1: Failing test** `settingCyclesThree`: from 0, `changeSetting(settingBattleMenus,1)` three times gives 1, 2, 0 and `settingValue(...,1)` is "Classic", "Overlay", "Original"; replace `settingToggles`.
- [ ] **Step 2: Run** → compile error (`battleMenus` missing).
- [ ] **Step 3: Implement** the int setting, the old-key load (`classicBattleMenus` → `battleMenus=value?1:0`), the texts.
- [ ] **Step 4: Run tests** → `battle windows: all passed 0`; full build succeeds.
- [ ] **Step 5: Commit** "Battle menus: a third value, Overlay".

### Task 2: `OverlayMenus` core (areas, placement, pointer, sprites)

**Files:**
- Create: `native/OverlayMenus.hpp`, `native/OverlayMenus.cpp` (add to `dqiv_core` in `CMakeLists.txt`)
- Test: `tests/battle/battle_windows_tests.cpp` (same target; name stays)

**Interfaces:** Produces (namespace `pc`):

```cpp
enum class OverlayMenu {other,battleRoot,battleCommands,battleTarget,battleSpells,battleItems,battleUseOrEquip};
struct MenuLayerMaps {           // one menu's buffers, as the game drew them this frame
    OverlayMenu menu=OverlayMenu::other;
    const std::uint16_t* window=nullptr;   // 32x48 cells
    const std::uint16_t* back=nullptr;     // 32x48 cells
    const std::uint16_t* icon=nullptr;     // 32x48 cells (this layer's icon cells only)
    const std::uint8_t* chars=nullptr;     // 32x48 tiles of 0x20 bytes
};
struct DsRect {int x=0,y=0,w=0,h=0;};      // DS pixels, y 0..383 (top screen first)
DsRect drawnArea(const MenuLayerMaps&);    // bounding box of non-zero cells of the three maps; w 0 when empty
struct OverlayRule {int anchorX=0,anchorY=0;int offsetX=0,offsetY=0;}; // anchor -1 left/top, 0 centre, 1 right/bottom; offsets in DS pixels
const OverlayRule* overlayRule(OverlayMenu);  // nullptr: stays at its DS place
struct LayerPlacement {DsRect area;int x=0,y=0;int scale=1;}; // window pixels of the area's top-left
// All layers at once (a rule can depend on another layer's area); same order as the layers.
// dsOrigin: window pixels of DS pixel (0,0) as presented (top screen's corner); dsScale: window pixels per DS pixel.
std::vector<LayerPlacement> placeLayers(std::span<const MenuLayerMaps>,int windowWidth,int windowHeight,int dsOriginX,int dsOriginY,int dsScale);
// The menu system's frame, filled by the game (Task 3), read by the window (Task 5).
struct OverlayMenuState {
    bool active=false;                     // Overlay and the menu system drew this frame
    int count=0; MenuLayerMaps layers[8];
    const std::uint16_t* oam[2]={};        // top, bottom: 128 entries of 4 halfwords
    int windowBg=-1,backBg=-1,iconBg=-1;   // main-engine BG numbers of the three menu BGs
};
OverlayMenuState& overlayMenuState();
// Topmost placement under a window point (placements in drawing order); its DS coordinates.
bool pointerOnLayer(std::span<const LayerPlacement>,int x,int y,int& dsX,int& dsY);
// The layer a sprite (DS centre point) belongs to: the topmost whose area contains it, or -1.
int spriteLayer(std::span<const LayerPlacement>,int dsX,int dsY);
```

Rules (exact): `battleRoot`, `battleCommands`, `battleTarget`: anchor (-1, 1), offset (8, 8); `battleSpells`, `battleItems`, `battleUseOrEquip`: anchor (-1, 1), offset (8 + the Commands area's width + 4, 8) — from the `battleCommands` layer placed in the same call, else offset (8, 8).

- [ ] **Step 1: Failing tests:**
  - `areaOfCells`: a window map with cells (3,26) and (10,30) non-zero → `drawnArea` = {24, 208, 64, 40}; all-zero maps → `w==0`.
  - `ruleAnchorsBottomLeft`: `placeLayers` with one Root layer of area {16,272,96,48}, window 1920×1080, scale 4 → x 32, y 1080-32-48*4.
  - `noRuleStaysOnDs`: `other` area {40,200,80,30}, origin (640,0), scale 2 → x 640+80, y 400.
  - `pointerMapsBackToLayer`: Root placed as above; point at placement + (4*10, 4*5) → true, dsX 26, dsY 277.
  - `layersKeepTheirOwnCells`: two layers with overlapping cells and different rules → their placements' rectangles do not overlap and each area is its own map's box.
  - `spriteGoesToContainingLayer`: sprite centre inside the second of two overlapping areas → index 1; outside both → -1.
- [ ] **Step 2: Run** → compile error.
- [ ] **Step 3: Implement** the module (pure arithmetic; no game or GL code).
- [ ] **Step 4: Run tests** → all passed.
- [ ] **Step 5: Commit** "Overlay menus: areas, placement, pointer and sprites".

### Task 3: Per-menu layers on the game side

**Files:**
- Create: `game/src/main/menu/MenuLayers.cpp`, `game/include/main/menu/MenuLayers.hpp`
- Modify: `game/src/main/menu/MenuSubManager.cpp` (`draw`), `game/src/main/menu/MenuBase.cpp` (`close` frees the layer), `game/src/main/menu/MenuBgDraw.cpp` (`setupMenuBgDraw` clears the layers on redraw), `game/src/main/menu/MenuBg.cpp` (`transferMenuBg` skips the menu buffers; icon requests record `currentMenuLayer()`), `game/src/main/menu/MenuDisplayManager.cpp` (`drawMenuDisplays` uses the two full-screen OAM buffers)

**Interfaces:**
- Consumes: Task 1 `overlayMenus()`, Task 2 `MenuLayerMaps`, `OverlayMenu`.
- Produces (game side, `#ifdef DQIV_NATIVE`):

```cpp
struct MenuLayer {menu::MenuBase* menu; MenuWindowBg window; MenuBackBg back; unsigned short icon[0x600];};
MenuLayer* menuLayerFor(menu::MenuBase*);   // pool of 8; nullptr when full (today's path)
void freeMenuLayer(menu::MenuBase*);
int currentMenuLayer();                      // index drawing now, -1 outside a menu's draw
```
After the menu system's frame the game fills `pc::overlayMenuState()` (Task 2): one `MenuLayerMaps` per used layer, the two OAM buffers' entries, the BG numbers; `active=false` and `count=0` at the start of `MenuManager::execute`.

`OverlayMenu` per layer from the menu pointer: `&gBattleMenu_ROOT` → battleRoot, `&gBattleMenu_ACTIONMENU` → battleCommands, ATTACK / MAGIC2ENEMY / MAGIC2PARTY / ITEMUSE2ENEMY / ITEMUSE2PARTY → battleTarget, MAGIC → battleSpells, ITEM → battleItems, ITEMUSEEQUIP → battleUseOrEquip, else other. `windowBg`/`backBg`/`iconBg`: the main-engine BG numbers `getTextBgAtY(192)`, `unkfunc_02081608(192)`, and the icon BG's number from `MenuIconBg` (read where `flush` transfers it).

- [ ] **Step 1: Implement the pool and the redirect.** In `MenuSubManager::draw`, when `overlayMenus()`: for each menu, `MenuLayer* l = menuLayerFor(menu)`; set `g_menuWindowChars/Screen` and `g_menuBackScreen` to `l`'s buffers, call `menuBaseDraw`, restore the shared ones. On a redraw frame (`g_menuBgDrawEnabled`), clear `l`'s buffers before its draw, as `setupMenuBgDraw` clears the shared ones.
- [ ] **Step 2: Nothing on the DS.** `transferMenuBg` in Overlay: no window/back copies, no merge into the text BG; `drawMenuDisplays` in Overlay passes the two full-screen buffers (y < 192 top, else bottom) and nothing to the DS OAM.
- [ ] **Step 3: Build** → succeeds.
- [ ] **Step 4: Screen check** — `game.sh start 64 battleMenus=2`, quick save, debug fight: the DS screens show the battle without any menu; with `battleMenus=0` the menus are back (shots of both).
- [ ] **Step 5: Commit** "Overlay menus: one layer per menu, none on the DS".

### Task 4: Rendering a layer

**Files:**
- Modify: `native/Display.hpp/.cpp` (a public method), `native/FramePresenter.cpp` (call it once per frame when Overlay and keep the pictures)
- Test: `tests/battle/battle_windows_tests.cpp`

**Interfaces:**
- Consumes: Task 2 `MenuLayerMaps`, `OverlayMenuState` (filled by Task 3).
- Produces:

```cpp
// Display: one layer's 256x384 picture (ARGB, 0 transparent): back, window, icon maps in that
// order, then the sprites the placements give it (spriteLayer), all with the main engine's
// BG VRAM, OBJ VRAM and palettes of this frame.
std::vector<std::uint32_t> menuLayerPicture(NitroVideo&,const MenuLayerMaps&,int windowBg,int backBg,int iconBg,
    std::span<const std::uint16_t> oamTop,std::span<const std::uint16_t> oamBottom,std::span<const int> spriteIds);
```

- [ ] **Step 1: Failing test** `rendersRamAndVramTiles`: a `NitroVideo` with VRAM bank A mapped to main BG (`write(0x04000240,{0x81})`), BG palette colour 1 = 0x001f (red), BG0CNT char base 0, VRAM tile 1 all index 1; a layer whose window map has cell (0,0) = char 1 and cell (1,0) = char 0x80 with chars tile 0 all index 1 → pixels (0,0) and (8,0) both 0xffff0000-ish (`rgb(0x001f)`), pixel (16,0) transparent.
- [ ] **Step 2: Run** → compile error.
- [ ] **Step 3: Implement** with the same tile and palette decoding as `Display::background` (text BG, 4-bit, flips, palette from the entry's top 4 bits) and `Display::objects` (regular sprites only; affine sprites drawn unscaled).
- [ ] **Step 4: Run tests** → all passed.
- [ ] **Step 5: Commit** "Overlay menus: layer pictures".

### Task 5: Placing, drawing and clicking

**Files:**
- Modify: `native/GameWindow.hpp/.cpp` (`drawOverlayMenus(width,height)` after `drawBattleWindows`; touch mapping), overlay ids `overlayMenu0`…`overlayMenu8` (8 layers + the unassigned sprites)

- [ ] **Step 1: Draw** the layers of `overlayMenuState()`: `placeLayers` with `dsOriginX/Y` and `dsScale` from `layout_` (the presented DS screens, as the touch code computes them), crop the picture to its `area`, `drawOverlay(..., placement.x, placement.y, width, height, area.w*scale, area.h*scale)`, in layer order, the unassigned-sprite layer last at its DS place.
- [ ] **Step 2: Touch** — when Overlay and `pointerOnLayer` finds a layer, `setTouch(down, dsX, dsY - 192)` for bottom-screen points (top-screen points: no touch); otherwise today's mapping.
- [ ] **Step 3: Build.**
- [ ] **Step 4: Screen checks** (`battleMenus=2`, 2-group debug fight via a temporary uncommitted `DebugTools.cpp` change):
  - Root and Commands at the bottom left in the DS look, the ▶ moving with the keys, at 60 fps (shots 50 ms apart differ in the cursor blink only, not by half-screen alternation);
  - Spells beside Commands, both whole where they overlapped on the DS (Review Focus 1);
  - mouse click on "Magic" in the moved Commands opens the spells (Review Focus 3);
  - closing the spell list removes its layer the same frame (Review Focus 2).
- [ ] **Step 5: Commit** "Overlay menus: placed on the wide window, clicks mapped back".

### Task 6: Original and Classic untouched

- [ ] **Step 1:** Build the commit before Task 1 in a scratch worktree; with `battleMenus=0` (and the old build's defaults) take Root, Commands and items shots with the same keys; compare the menu area pixel for pixel → 0 differences.
- [ ] **Step 2:** `battleMenus=1`: Root and Commands shots match the Classic windows (visual).
- [ ] **Step 3:** Ledger the results; nothing to commit unless a fix was needed.
