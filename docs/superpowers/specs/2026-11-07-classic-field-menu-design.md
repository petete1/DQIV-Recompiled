# Classic field menu

## Goal

The menu opened outside battle takes Classic's DQ11 look, drawn by the port with
the game's own font, icons and texts, reusing the Classic battle windows: a
command panel, a gold panel, the party cards and the world map. This step covers
the root screen and the Items and Magic flows; every other command (Talk,
Examine, Attributes, Misc and what they open) keeps the DS screens for now.
The map shown here is a stand-in, to be redone later.

## Setting

- "Battle menus" is renamed **"Menus"** (Original / Classic / Overlay), in every
  language; its help says Classic covers battle and field menus. The ini key
  stays `battleMenus`.
- Classic in the field follows the battle rule: only with the wide picture
  (`classicBattleMenusOn`), otherwise Original. Overlay changes nothing in the
  field.

## Look

Exactly the Classic battle look (`BattleWindows.cpp`): brown panels, tan border,
corner curls, cream text, the golden hand, page header with ◀ n/m ▶, the
description window, 16×16 icons, the party cards (faces, name, level, HP and MP
bars). Nothing new is drawn except the gold panel and the map frame below.

## Layout (DS pixels, `battleScale`, as in battle)

- **Commands**: top left, 8 from the left and top. Two columns, in the DS
  order: Talk / Magic, Items / Examine, Attributes / Misc, each with its DS
  icon (the command icons the DS root shows, shrunk to 16).
- **Lists** opened from it (whose items, items, item commands, spells, targets,
  town lists): the same panel, below the commands panel (4 apart), left
  aligned; the description window at its right as in battle, or below the
  list when there is no room at its right.
- **Gold**: top right, 8 in; a panel the height of a row: the amount and "G"
  (the DS money window's text), right aligned.
- **Party cards**: bottom right, the battle panel unchanged, with the member's
  own HP/MP (`getHp`/`getMp`, maxima, level, condition, HP colour). No "can act"
  background. The member being chosen (whose items, the caster) slides out as
  the active card does in battle.
- **Map**: bottom left, 8 in, only where the game shows its world map while
  walking (the overworld; not in towns or dungeons).

## The map (stand-in)

While walking on the overworld the game draws its world map as 3D on the top
screen, which the port shows as an inset at the top right
(`FramePresenter`, `Mode::field`). Opening the menu, the game replaces it with
the status windows, so there is no map to draw. Classic keeps the inset's last
picture from the field and, while a Classic field menu is open, shows it at the
bottom left at the inset's size, with the same alpha. A part with no inset (a
town, a dungeon) shows none. Closing the menu, the inset returns to the top
right as now.

## Flows

All screens publish the menu state each frame from their `menuDraw` (and skip
the DS drawing) when Classic is on, as the battle menus do; input keeps the
game's `menuUpdate`, with `battleMenuSelect` / `battleListInput` for the cursor
and mouse.

- **Root** (`TownMenu_ROOT`): the commands. Messages it opens (e.g. "Nobody can
  use magic") show in the Classic message window.
- **Items**:
  - whose items (`TownMenuItemSelectChara`): the party members and the bag, one
    column; the chosen member's card slides out;
  - the list (the item list menu): the battle items list (header with the
    member's name, pager, icons, "E" marks, description); the bag has its own
    pages (`FukuroItemInfo`), shown the same way;
  - the item's commands (`TownMenuItemSelectCommand`): a one-column panel
    titled with the item's name;
  - targets (`TownMenuItemSelectTargetChara`, `TownMenu_ITEM_USE`): a member
    list; swapping (`TownMenuItemSelectTargetItem`): the other member's list;
  - Chimaera Wing towns (`TownMenu_ITEM_MOVE`): a one-column paged list.
- **Magic**:
  - the caster and spells (`TownMenu_MAGIC_ROOT`, its two modes): first the
    members who know spells, then the spells list as in battle (kind icons,
    description, "MP cost/MP");
  - target (`TownMenu_MAGIC_CHARSELECT`): a member list;
  - Zoom towns (`TownMenu_MAGIC_MOVE`): a one-column paged list.
- **Messages**: the menu's message window (`g_TownMenuMessage`) while a Classic
  field menu is open: the Classic message window, bottom centre, left of the
  party cards.

## Structure

- `pc::BattleMenuState` gains where its menu group sits (`top` for the field,
  bottom for battle) and `gold` (a `BattleText`, empty: no gold panel); the
  party rows and message are reused as they are.
- `native/BattleWindows`: the menu group placed from the top when asked, lists
  stacked below the commands; `layoutBattleGold` / `composeBattleGold`.
- `GameWindow::drawBattleWindows` draws the gold panel and, in a field menu,
  asks the presenter for the kept map at the bottom left.
- The game side: a `publishFieldParty()` beside `publishBattleParty()` sharing
  its row building (only the HP/MP source differs); the field menus call the
  same `beginBattleMenu` / `addBattleEntry` / `setBattleHelp` / … helpers.

## Testing

- Unit (`tests/battle/battle_windows_tests.cpp`): the field group at the top
  left; lists below the commands; the gold panel at the top right inside a
  640×480 and a 1920×1080 window; the map rectangle at the bottom left, clear
  of the party cards.
- Screen checks with `battleMenus=1`: the menu on the overworld (map shown) and
  in a town (no map); Items: a member's list, the bag, use a medicinal herb on
  a member, the message; Magic: cast Heal on a member, Zoom's town list;
  Original and Overlay unchanged in the field.

## Risks

- The kept map picture is the last field frame's; the map cannot change while a
  menu is open, so it stays right, but a menu opened before the map faded in
  shows none.
- The bag's paging and the town lists' pages differ from the 6-per-page lists.
- Screens not covered here (Talk, Examine, Attributes, Misc and theirs) open
  as today, on the DS screens: the root closes when they open, so no Classic
  panel shows with them; back at the root, the Classic panels return.
