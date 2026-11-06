# Classic battle menus: the DQ11 look

## Goal

Classic ("Battle menus: Classic") takes the layout and feel of Dragon Quest
XI's battle screen, drawn by the port with the game's own font and art: a
brown command panel at the bottom left, the party stacked on the right, and a
restyled message window. It replaces Classic's navy windows
(`2026-11-06-classic-battle-menus-design.md`); Original and Overlay are
unchanged.

Not copied: DQ11's font, ornaments or any of its art — panels, borders,
flourishes and the cursor are drawn in code as pixel art; text is the game's
12-dot font. Not in DQ4, so not shown: Abilities, Equipment, the "Y Tactics"
hint.

## Look

All drawn at DS pixel size and scaled by a whole number (`battleScale`:
window height / 270), so it matches the pixel font.

- **Panel:** fill `0xe0281810` (dark brown, see-through), a 1-pixel tan
  border `0xffb89870` inset 2 pixels from the edge, 1-pixel rounded outer
  corners, and a 5×5 flourish in each corner inside the border (a small curl
  in the border colour).
- **Text:** the game's font through `battleText` (BattleMenuNative.cpp),
  coloured cream `0xfff4ead8`; greyed-out entries at half brightness.
- **Cursor:** a golden pointing hand, 11×7 pixel art (`0xfff0c040` with a
  `0xff806010` outline), blinking as now (on 20 frames, off 10).

## Layout (window pixels at scale s; DS-pixel values below)

- **Command panel:** bottom left, 8 from the left and bottom. Root: one
  column (Fight / Formation / Tactics / Flee, or Fight / Flee). Commands: two
  columns (Attack, Spells / Defend, Items in the game's order), no title — the
  active member is shown in the party panel instead.
- **Lists** (spells, items, use or equip, enemy and party targets): the same
  panel, to the right of the command panel (4 apart), as Classic does now.
- **Party panel:** right edge, one row per member (up to 4), stacked from the
  top of the lower half of the window. Each row 40 high: the member's
  portrait (32×32, the game's own menu face, `getMenuIconData(0xf1000000,
  iconIndex_)` with the menu icon palette), the name in green `0xff90e070` at
  the top right, then "HP" and "MP" bars (64×4, green `0xff60d040` / blue
  `0xff4090f0` over `0xff202020`, with the numbers right-aligned above them)
  and "Lv. n" under the portrait. Behind each row a dark strip `0xa0000000`
  fading to transparent over its left 48 pixels. The member whose command is
  being chosen has a lighter strip `0xa0403020`.
  Values are the battle's own: HP `memberHP_[i]` and MP `memberMP_[i]`
  (`BattleMenuPlayerControl`, they count down as on the DS), maxima
  `getHpMax()`/`getMpMax()`, level `haveStatus_.level_`; the HP bar and the
  name take the game's HP colour (`getHPColor(i)`: yellow when low, red when
  very low, as the DS windows); a member's condition text (`getCondition(i)`,
  e.g. poisoned) replaces "Lv. n" while the DS shows it.
- **Message window:** bottom centre, 8 from the bottom, 240 DS pixels wide
  (cut to the window), height for its lines. The game's own rendered lines
  (the `MenuTextDisplay` lines of `g_MenuDisplays.message_`: their character
  buffer, visible width and position, so typing, colours and scrolling are
  the game's) are copied into the panel; a ▼ blinks at the bottom right while
  it waits for a button (`isWaitTrigger()`). The speaker's name plate, when
  shown, sits on the top border.

## What it hides (Classic, battles only)

- The top status windows, faces and action history: `BattleMenuSub_HISTORY::
  menuDraw` publishes the party instead of drawing.
- The message window's DS frame, lines and darkened window (in battles; towns
  keep the DS message window).

## Structure

- `pc::BattleMenuState` gains `party` (rows: portrait as indices + palette,
  name text, hp/hpMax/mp/mpMax, level text or condition text, colour index,
  active flag) and `message` (lines as `BattleText` with their DS y and
  visible width, the name plate, waiting flag), filled by the game side each
  frame like the menus (BattleMenuNative.cpp).
- `native/BattleWindows.cpp` gets the new look (panel, cursor) and lays out
  three groups: command and lists (bottom left), party (right), message
  (bottom centre); each composes into the same picture as now (a picture per
  group, so the party panel and message don't stretch one big picture).
- Mouse: unchanged (hover moves the cursor, click is A, right click B); the
  party panel and message take no clicks except that a click on the message
  window is A (next line).

## Testing

- Unit (`tests/battle/battle_windows_tests.cpp`): party panel rows stack from
  the right edge and stay inside a 1280×720 and a 640×480 window; the message
  panel is centred and inside the window; the two-column Commands layout
  (Attack/Spells left, Defend/Items right in the game's order); a party row's
  bar widths follow hp/hpMax.
- Screen checks with `battleMenus=1`: Root, Commands, spells, a target list,
  the party panel counting HP down after a hit, a battle message typing out
  and waiting; no DS status windows or message window left. Original and
  Overlay unchanged (Original pixel comparison of the menu area as before).

## Risks

- The portrait palette: the menu icon palette must be loaded in battle
  (`MenuIconBg::loadPalettes`); otherwise the portrait shows wrong colours.
- Messages outside the battle flow (e.g. a level-up with choices) keep the
  game's yes/no window on the DS.
