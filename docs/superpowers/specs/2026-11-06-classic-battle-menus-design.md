# Classic battle menus

## Goal

The first step of a native interface: the battle menus of the DS bottom
screen redrawn as classic Dragon Quest windows, laid out over the whole game
window like the console games and drawn every frame (full frame rate on the
whole picture). The game's menu logic stays as it is: the new windows only
show what the game's menus hold and pass input back to them. The original
menus stay available, unchanged, behind a setting.

What the player sees, with "Battle menus: Classic":

- Windows along the bottom of the game window: dark navy panels, a white
  border two pixels wide with rounded corners, white text in the game's own
  font, grey for entries that can't be chosen, a blinking ▶ cursor.
- Commands as text lists instead of the DS touch icons; spell, item and
  target lists open beside them.
- With "Battle menus: Original" the DS menus are exactly as today.

In scope: every battle menu of the bottom screen (`game/src/ov015/btl`):

| Menu (class) | Window |
|---|---|
| Root (`BattleMenu_ROOT`) | Fight / Flee, or Fight / Formation / Tactics / Flee (late game) |
| Commands (`BattleMenu_ACTIONMENU`) | Titled with the active member's name; Attack / Spells / Defend / Items in the game's order and enabled state |
| Attack target (`BattleMenu_ATTACK`) | Monster groups with their counts ("Slime ×3"), in the game's group order |
| Spells (`BattleMenu_MAGIC`) | Two columns, the game's page size, MP cost at the right, "MP left" at the bottom, "◀ 1/3 ▶" with several pages |
| Spell targets (`BattleMenu_MAGIC2ENEMY`, `BattleMenu_MAGIC2PARTY`) | Monster groups, or member names |
| Items (`BattleMenu_ITEM`, `BattleMenu_ITEMUSEEQUIP`) | Like spells, with the item counts or equipped marks the DS shows |
| Item targets (`BattleMenu_ITEMUSE2ENEMY`, `BattleMenu_ITEMUSE2PARTY`) | Monster groups, or member names |
| Tactics (`BattleMenu_TACTICSMENU`) | The members, then the tactics to choose from |
| Formation (`BattleMenu_ARRAYMENU`, `_ARRAY_CHANGE`, `_ARRAY_ALL`) | The menu's own entries as lists |
| Blocked choice (`BattleMenu_NGMESSAGE`) | Its message in a small window above the commands |
| Stadium abort (`BattleMenu_StadiumAbort`) | Its entries as a list |

Not in scope (they keep the DS look): the party status windows on the top
screen, the battle message window (`TownMenu_MESSAGE`), the command history
panel (`BattleMenuSub_HISTORY`), the DS face and HP-percent panel of the
commands menu (the top status windows already show HP), monster name plates,
menus outside battle. `BattleMenu_TEST` (debug) is left alone.

## Structure

Three parts; data goes one way, game → description → windows, and input
goes back through one narrow path.

1. **Game-side hooks** in each menu's `menuDraw` and `menuUpdate`, under
   `#ifdef DQIV_NATIVE` and only while the setting is Classic.
2. **The description** (`native/BattleWindows.hpp`): a plain struct rebuilt
   every frame, holding what the open menu shows.
3. **The windows** (`native/BattleWindows.cpp`): turn the description into a
   picture placed on the interface layer (`GpuRenderer::drawOverlay`, as the
   settings screen and FPS counter are), drawn every presented frame.

The windows never read game structures; the hooks never draw.

## The description

```cpp
namespace pc {
enum class BattleMenuKind {none,root,commands,targetEnemy,targetParty,spells,items,
    tacticsMember,tacticsSelect,formation,message,stadiumAbort};
struct BattleMenuEntry {
    int text=0;          // the game's message value (as drawMenuText takes it)
    int value=0;         // a right-hand number (MP cost, count), or 0
    int valueText=0;     // a right-hand message value instead (equipped mark), or 0
    int count=0;         // monster groups: how many are left
    short index=0;       // the game's menu index (MenuItem::active_) for this entry
    bool enabled=true;
};
struct BattleMenuState {
    BattleMenuKind kind=BattleMenuKind::none;
    int title=0;                         // the window's title message value (member name), or 0
    std::vector<BattleMenuEntry> entries; // the current page
    int cursor=-1;                       // entries index of MenuItem::active_
    int page=0,pages=1;
    int mpLeft=-1;                       // spells: the caster's MP, -1 when not shown
    int message=0;                       // message kind: its message value
    bool waiting=true;                   // waiting for input (the cursor blinks)
};
// Set by the game's battle menus each frame; none when no battle menu draws.
BattleMenuState& battleMenuState();
}
```

A hook fills the state from the same values the DS drawing uses (for
example `BattleMenu_MAGIC` from `haveAction_`, `count_` and
`navigator_.getPageNo()`, the costs from `getActionNameMessage` and the
caster's status). The state is cleared once per game frame before the menus
draw (`kind = none`), so the windows close when the game's menu closes.

A menu whose hook isn't written yet sets nothing and keeps drawing its DS
version and using its DS input: it is never left without a menu.

## Drawing

- **Scale:** everything is laid out in DS pixels and scaled up by a whole
  number: `max(1, windowHeight / 270)` (×2 at 720p, ×4 at 1080p, ×8 at 4K), so
  the font's pixels stay even. `drawOverlay` gets an optional destination
  size; its texture filter is already nearest.
- **Placement:** windows are anchored to the bottom of the game window (not to
  a DS screen) with a margin of 8 DS pixels; the commands window at the left,
  lists to its right, the blocked-choice message above the commands. Window
  widths fit the longest entry (measured with `getBgTextWidth`); a window
  wider than the game window allows cuts its entries with "…".
- **Text:** each entry is drawn by the game's `drawBgText` (with
  `g_text_extractor.extract_text` for message values, as `drawMenuText`
  does) into a private 4-bit `BgCharBuffer`, never the DS menu buffers, then
  coloured with the game's menu text palette (`g_menuTextColors`). The 12-dot
  font is used for entries and titles, as the DS menus do. Names, tags, the
  German monster exceptions and every language come out as in the game.
- **Frames and cursor:** drawn as pixel art at DS scale: navy fill, white
  2-pixel border, rounded corners; the ▶ cursor blinks (on 20 frames, off 10)
  while `waiting` is set.
- **Cost:** the picture is rebuilt only when the state or the cursor blink
  changes; the overlay is drawn again every frame.

## Input

Keyboard and controller:

- The game reads input as usual (`MenuItem::execInput()`). Right after it, in
  each converted menu's `menuUpdate`, a native step looks at `result_`:
  - 2 (confirm) and 3 (cancel) pass through unchanged;
  - 5–8 (directions) are taken over: the cursor moves in the window's own
    layout (a list, or two columns) and `active_` is set to that entry's
    game `index`; `result_` becomes 1 (cursor moved, as the game's own
    movement reports);
  - Left in the left column or Right in the right column of a paged list
    passes through, so the game's navigator turns the page.

Mouse:

- Pointing at an entry moves the cursor to it; left click confirms (as
  `result_` 2), right click cancels (as 3). Window rectangles come from the
  last built layout, in game window pixels.
- The battle menus' DS touch rectangles (`MenuTemplate_battle`, including the
  cancel area `BATTLE_RECT_ENEMY`) are not set up while the setting is
  Classic, so clicks on the battle scene don't hit invisible DS buttons.

## Setting

- `GameSettings::classicBattleMenus` (bool), Game page, shown as
  "Battle menus: Classic / Original", saved like the other settings, in all
  six languages.
- Original skips every new branch: the code that runs is today's.
- It takes effect at once (the menu logic is the same either way); the state
  is rebuilt the next frame.
- Default Original until every menu in scope is converted, then Classic.
- Works with all three battle layouts (Large, Panorama, DS screens).

## Testing

- A small assert test (`tests/battle/battle_windows_tests.cpp`, like
  `tests/save/save_pages_tests.cpp`) for what runs without the game: window
  placement from a state and a window size, the scale, and cursor movement
  between list positions and game indexes (including page turns at the
  column edges).
- Screen tests (`tests/screen/game.sh`) from a battle save: one screenshot per
  menu with Classic; the same with Original, compared with the current build's
  screenshots, to show the original menus are untouched.
- Each converted menu checked by hand once: choosing, cancelling, paging,
  greyed-out entries, the mouse.

## Risks

- Some menus may keep state the DS drawing computes as it draws (paging,
  enabled flags); their hooks then have to compute the same values without
  drawing. Found per menu while converting.
- `drawBgText` uses shared font state (`g_BgText`); drawing into the private
  buffer happens at a moment no DS text is being drawn, and the font size is
  restored after.
