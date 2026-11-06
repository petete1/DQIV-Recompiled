# Overlay menus

## Goal

A third value of the battle menus setting: **Overlay**. The game's own menus
are drawn exactly as on the DS (same code, same tiles, text, icons and
sprites), but on the port's interface overlay instead of the DS screens:
every frame (full frame rate), and each menu placed anywhere on the wide game
window, not only where the DS screens are.

The setting becomes "Battle menus: Original / Classic / Overlay"
(`GameSettings::battleMenus`, 0 / 1 / 2). Original is unchanged; Classic is
the native windows of `2026-11-06-classic-battle-menus-design.md`.

## How the DS menus draw today

- Each open menu (`menu::MenuBase`, up to 8 per `MenuSubManager`) draws in
  `MenuSubManager::draw` through `drawMenuParts` into three shared buffers:
  the window BG (`MenuWindowBg`: 32×48 tiles of text characters plus a 32×48
  screen map, both screens), the back BG (`MenuBackBg`, a 32×48 map) and the
  icon BG (`MenuIconBg`: a 32×48 map, characters cached in VRAM slots).
  Frame tiles are system characters already in BG VRAM; text characters come
  from the RAM character buffer (from character 0x80, VRAM offset 0x1000).
- `transferMenuBg` copies half of each buffer per frame to the DS (the half of
  the screen the frame is for), merging the window map into the text BG.
- Menu sprites (the cursor, faces, yes/no, frames, hopping numbers) are the
  global `g_MenuDisplays`, drawn by `drawMenuDisplays` into the main and sub
  OAM buffers; in the joined 3D modes one of them is the other half's and is
  dropped.

## Design

1. **One layer per menu.** In Overlay, `MenuSubManager::draw` points
   `g_menuWindowChars`, `g_menuWindowScreen` and `g_menuBackScreen` at the
   drawing menu's own buffers (a pool of 8 `MenuLayer`s keyed by the menu,
   freed when it closes) before its `menuBaseDraw`, and back after. Icon
   requests record the layer that made them. Overlapping windows of
   different menus therefore keep all their pixels; windows of one menu move
   together, so the overlaps a menu was designed with stay right.
2. **Nothing on the DS screens.** In Overlay `transferMenuBg` copies none of
   the menu buffers, and `drawMenuDisplays` draws into two full-screen OAM
   buffers of the port (top and bottom) instead of the DS ones. The message
   window and everything outside the menu system are untouched.
3. **Rendering.** Each frame the native side renders every layer to a
   256×384 ARGB picture with the DS colours: its window map (characters
   ≥ 0x80 from the layer's character buffer, others from BG VRAM), back map
   and its icon cells, through the BG palette. Menu sprites are rendered from
   the two OAM buffers (OBJ VRAM, OBJ palette) and given to the topmost layer
   whose drawn area contains the sprite's centre; the rest form a layer of
   their own.
4. **Placement.** A layer's drawn area is the bounding box of its non-empty
   map cells. A table of rules keyed by menu (`OverlayMenu` enum, from the
   game's menu globals) gives an anchor on the game window (left / right /
   centre, top / bottom), an offset in DS pixels, and a scale (the DS pixel
   size of the presented picture). Menus without a rule, and the
   unassigned-sprite layer, stay where the DS screens show them.
   First rules (battle): Root, Commands and the target menus at the bottom
   left; spells, items and use-or-equip beside them. The table is data in
   one file, to edit or later load from a file.
5. **Input.** The mouse over a placed layer is converted back to that
   layer's DS coordinates and sent as the DS touch, topmost layer first; the
   game's own touch rectangles keep working. Keys are unchanged.

## Not in scope

The battle message window (`TownMenu_MESSAGE` text on the text BG), the
party status windows on the top screen when they are not drawn by a menu,
monster name plates (OAM, not menu parts), an in-game layout editor or a
layout file, Classic's native windows.

## Risks

- Menus that read the shared buffers back (rare) would see their own layer.
- Some sprites belong to a menu but sit outside its drawn area (e.g. a page
  arrow below a window); they stay at the DS position until a rule widens
  the area.
- `MenuIconBg`'s VRAM cache is shared: icons stay correct only while every
  layer's icons are still cached — the cache holds 28 slots, enough for the
  battle menus.
