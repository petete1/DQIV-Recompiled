# Classic launch menu

## Goal

With Classic menus on, the menu shown when the game starts (`MaterielMenu_LOAD`)
looks like the other Classic menus, and its adventure log list becomes one
DQ11-style list of cards: the autosave and the quick save at the top, then all
six logs, each with the date and time it was saved. No more pages of three.

## When it shows

`battleMenus` = Classic with the wide picture, the same rule as the battle and
field menus. Original (and Classic without the wide picture) keeps today's
drawing and today's pages of three, L/R included.

## The commands

The root entries as today (Continue, Create, Delete, Settings, Achievements,
Quit) in a Classic panel at the top left, 8 in: brown, tan border, cream text,
the game's animated cursor. Mouse: one hit area per entry.

## The log list

After Continue, Create or Delete: one large Classic window, from 8 in at the
top left to 8 in from the right and the bottom.

- **Header bar**: the command's name (`0x80000196`/`197`/`198`, as today).
- **Cards**, one per row, in this order:
  1. Autosave (labelled with `pc::autosaveLabel`)
  2. Quick save (`pc::quickSaveLabel`)
  3. Logs 1–6 (numbered)
- **A card** (framed, brown):
  - left: the face of the chapter's lead (chapter 1 Ragnar, 2 Alena,
    3 Torneko, 4 Meena, 5 and after the hero), the 48×48 menu portrait;
  - first line: the number (logs) or the label (resume saves), the name, and
    the play time at the right;
  - second line: the chapter and the place, and the save's date and time at
    the right (`YYYY-MM-DD HH:MM`, local time);
  - a save with no date (made before this change) shows the rest without one;
  - an empty slot: the number or label and the game's empty text, no face;
  - a log saved as "chapter end" (`savetype_` 4) shows as today (no chapter,
    level or place).
- **Cursor**: the animated cursor on the chosen card, whose frame is lighter.
  It starts on the most recent log of the adventure's page (as today), or on
  the first used card.
- **Which cards can be picked**:
  - Continue: all eight; the resume cards resume that save (as the Resume
    line does today), a log loads it (an empty log asks as today);
  - Create and Delete: only the six logs; the resume cards are drawn dimmed
    and the cursor skips them.
- **Too short**: when the window cannot hold all eight cards, the list scrolls
  to keep the cursor's card in view, with ▲/▼ marks at the edges.

The game's messages and yes/no prompts, name entry and the gender pick stay as
they are.

## Eight saves in one list

The game sees three logs at a time; `pc::SavePages` swaps the file it reads
(page 1 `dqiv_eur.sav`: logs 1–3 and the quick save; page 2 `dqiv_eur-2.sav`:
logs 4–6 and the autosave). The list keeps that:

- **Reading**: when the list opens (and after a delete or a create), the game
  side shows each page in turn and reads its catalogue
  (`profile::SaveLoad::getCatalogView`) and its resume save
  (`getResumeView`), copying the names out (the catalogue's names live in a
  buffer the next read overwrites), then shows the adventure's page again.
- **Picking log n (1–6)**: show page (n−1)/3, re-read that page's catalogue,
  and hand the game slot (n−1)%3 (`activeDiaryNo_`); everything after (load,
  create, delete, the messages' macros) runs as today on that slot.
- **Picking a resume save**: as today's `LOAD_RESUME`, with the slot picked
  (quick or autosave) instead of the page's.

## Save dates

The DS format has no timestamp, and a file's modification time covers four
saves. Both files leave 0xF400–0xFFFF unused (zeros in the player's files).

- `SavePages::write` stamps the time (Unix seconds, 64-bit little-endian) into
  that file at 0xFF00 + 8×k when a write lands in bank k (k = 0–2: the page's
  logs; 3: its resume save). Banks are 0x3C00 bytes from 0x400
  (`BANK_OFFSET`).
- A zero stamp means no date. Deleting a log does not clear its stamp; an
  empty log shows no date anyway.
- The game never reads 0xF400–0xFFFF, so old versions and the DS format are
  unaffected.

## Structure

- `pc::BattleMenuState` gains a launch-menu part (`TitleMenu`): whether it
  shows, the header, the cards (kind, number or label, name, chapter text,
  place, play time, date, face, pickable), the cursor and the scroll.
- `native/BattleWindows`: the layout and drawing of the commands panel and
  the list (cards, scroll, the too-short fallback to today's drawing), with
  hit areas.
- `GameWindow`: draws it in place of the field menu group while it shows.
- Game side (`MaterielMenu_LOAD.cpp`): publishes the panel and the list each
  frame; the eight-card list's input (cursor, picking, cancel) when Classic
  shows; today's paged list otherwise.
- `native/SavePages`: the stamps, plus `saveTime(page, bank)` to read one.

## Testing

- Unit (`tests/battle/battle_windows_tests.cpp`): the panel and the list inside
  640×480, 1280×720 and 1920×1080; eight cards where they fit, scrolling where
  they don't; dimmed resume cards for Create/Delete; one hit area per card.
- Unit (save tests): a write into bank k stamps 0xFF00 + 8×k of that page's
  file only; a write elsewhere stamps nothing; reads of 0xF400–0xFFFF by the
  game see the stamps but nothing in the game reads them.
- Screen checks with `battleMenus=1`: the commands; Continue with both resume
  saves and logs on both pages; load log 5 (page 2); resume the autosave;
  Create into an empty log 4; Delete a log; dates after a save; Original
  unchanged with L/R pages.

## Risks

- The faces assume the chapter's lead; a chapter 5+ log shows the hero even
  when the party has others.
- Reading both pages each time the list opens swaps the shown page twice; the
  session page must be put back exactly or a save could land on the wrong
  page (tested on screen: save after loading log 5 lands in log 5).
- The stamp is the PC's clock; saves copied from another machine keep that
  machine's dates.
