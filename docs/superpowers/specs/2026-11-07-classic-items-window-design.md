# Classic items window

## Goal

With Classic menus on, the field Items flow shows as one large window in DQ11's
layout instead of today's separate panels under the commands: a "whose items"
pane with faces, the item list, and a detail pane with the item's big icon,
description, count and equipment comparison, plus a "Do What?" popup. The look
stays Classic's (brown panels, tan border, cream text, the game's animated
cursor); DQ11's parchment texture is not copied. Magic and every other command
keep today's Classic panels.

## When it shows

Every screen of the Items flow (the ones that publish a Classic field menu
today): whose items (`TownMenuItemSelectChara`), the list
(`TownMenuItemSelectItem`), the commands (`TownMenuItemSelectCommand`), targets
(`TownMenuItemSelectTargetChara`, `TownMenu_ITEM_USE`), swaps
(`TownMenuItemSelectTargetItem`) and Chimaera Wing towns (`TownMenu_ITEM_MOVE`).
Same setting and rule as the field menu: `battleMenus` = Classic with the wide
picture.

While it shows, the commands panel, the gold panel and the map are hidden. The
party cards stay as they are, and the chosen member's card slides out as now.
The Classic message window draws over the window's bottom as now.

## Layout (DS units, `battleScale`)

- **The window**: from 8 in at the top left to the party cards' left edge less 8,
  and down to 8 above the bottom. A header bar across its top reads the root
  command's name ("Items").
- **Whose items** (left pane): the members in the game's order, then the Bag.
  A member row is the name and, at its right, a strip of their face (the
  portrait cropped at the eyes, 32×16, faded on its left); the Bag has no face.
  Rows are 20 high.
- **The list** (middle pane): a header with the owner's name and, when more than
  one page, ◀ n/m ▶; then up to 6 rows in one column: "E" (equipped) at the
  left, the 16-px icon, the name. Empty when no member is chosen yet (the pane
  stays, blank).
- **The detail** (right pane), for the item under the list cursor:
  - a header: the item's kind as the DS equipment window names it
    (`MenuDataCommon::getEquipKind`), or the item's name when it is not
    equipment;
  - the item's 32-px icon at full size, centred in a framed box;
  - the description, wrapped (as the description window today);
  - for the Bag, the count it holds, with the DS bag's count text;
  - for equipment, the owner's line as the DS equipment window gives it
    (`getEquipCompareValues`): the stat's name (`getAbilityKind`) and
    "now ▶ new", or "equipped" and the value, or "can't equip", with the
    game's texts. Nothing for the Bag's equipment.
  Blank while no item is under the cursor.
- **Too narrow**: when the three panes don't fit side by side (the panes'
  minimum widths: whose 76, list 130, detail 100, plus gaps), the detail goes
  below the list, inside the window.
- **"Do What?"** (the item's commands): a small panel at the window's top right
  corner, overlapping the area the gold panel used, titled with the game's
  question, one column.

## The cursor in each screen

The active pane has the animated cursor; a pane already chosen keeps a still
frame on its choice.

| Screen | Active | Still |
|---|---|---|
| whose items | whose | — |
| the list | list | whose |
| commands | popup | whose, list |
| target (use on, transfer to) | whose, titled with the game's question | list |
| swap | list (the target's items, its name in the header) | whose |
| Chimaera Wing towns | list (the towns, titled) | whose |

## Structure

- `pc::BattleMenuState` gains an items-window part: whether it shows, its title,
  the whose rows (name, face strip, the still or active cursor), the detail
  (header, 32-px icon, words, count line, compare line), and the popup's
  entries. The list keeps using `entries`/`title`/`pageText`.
- `native/BattleWindows`: `layoutItemsWindow` / `composeItemsWindow` (the panes,
  the narrow fallback, the popup); entries keep their hit areas so the mouse
  works in every pane as today.
- `GameWindow` draws the items window in place of the field menu group, and
  skips the gold panel and the map while it shows.
- Game side (`BattleMenuNative.cpp`): the Items screens fill the new part
  instead of the parent/list panels; the detail is built from the existing item
  helpers plus `getEquipCompareValues`, `getEquipKind`, `getAbilityKind`.

## Testing

- Unit (`tests/battle/battle_windows_tests.cpp`): the window inside 640×480,
  1280×720 and 1920×1080 left of the party cards; the three panes side by side
  where they fit and the detail below the list where they don't; the popup at
  the top right inside the window; the gold and map hidden while it shows; one
  hit area per entry in the active pane.
- Screen checks with `battleMenus=1`: a member's list, the Bag (count), an
  equippable, an equipped and an un-equippable item, Use a medicinal herb on a
  member, Transfer into a full inventory (swap), a Chimaera Wing; Magic still in
  today's panels; Original unchanged.

## Risks

- At 1080p the window is about 320 DS units wide, so the detail sits below the
  list there; the window then looks less like DQ11's.
- The face strip's crop height is a guess per portrait (eyes at a fixed row);
  some faces may sit off.
