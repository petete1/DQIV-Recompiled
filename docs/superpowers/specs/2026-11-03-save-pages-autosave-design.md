# Save pages and autosave

## Goal

A safety net against losing progress (a crash, a wipe-out, quitting by
mistake): the game keeps one rolling autosave, written at each map change.
While adding it, the load and save screens get a second page, so there are six
adventure logs instead of three.

What the player sees:

- Main (root) menu: Continue / Create / Delete, then the PC's Settings,
  Achievements and Quit. The original "Resume" line is gone.
- The Continue screen has four lines and two pages, switched with L / R:
  - page 1: logs 1, 2, 3 and **Quick save**;
  - page 2: logs 4, 5, 6 and **Autosave**.
- The Create and Delete screens and the church save screen page the same way,
  with three lines (no quick save or autosave line).
- Walking through a door, stairs or any other map change writes the autosave
  once the player is back in control.

Not in scope: several rotating autosaves, an autosave on/off setting, loading
the autosave from anywhere but the title's load menu.

## Storage

Two 64 KiB cartridge images in the `saves` folder, each with the original
layout (header 0x000-0x3FF, logs at `0x400 + n * 0x3C00` for n = 0, 1, 2,
bank 3 at 0xB800-0xF3FF):

| File | Logs | Bank 3 |
| --- | --- | --- |
| `dqiv_eur.sav` (page 1, the existing file, unchanged layout) | 1-3 | quick save |
| `dqiv_eur-2.sav` (page 2) | 4-6 | autosave |

Each image stays a plain DS save.

### Routing

The game keeps addressing one cartridge (logs 0-2 and bank 3).
`native/NitroBackup.cpp` holds one `SaveStore` per page and routes each read
and write by offset:

- **Header and logs** (0x000-0xB7FF): the *shown page*, the page the load or
  save screen displays, or during play the *session page*.
- **Bank 3** (0xB800-0xF3FF): the *resume slot*: page 1's file (quick save) by
  default; page 2's file (autosave) while the Autosave line is shown or chosen,
  and while the autosave is being written.

A request that spans both ranges is split at 0xB800. `getCatalogView` reads
0x400-0xB7FF in one request, so it only touches logs.

A small native interface (`native/SavePages.hpp`) exposes:
`showPage(int)`, `shownPage()`, `sessionPage()`, `setSessionPage(int)`,
`useResumeSlot(Quick|Auto)` and `writeAutosave()`, plus the page tag reads and
writes below. The game's code calls them under `#ifdef DQIV_NATIVE`.

### Page tag

A quick save or autosave stores only the log number 0-2 (`BOOKNO`). Its page
is kept in the last 4 bytes of the header (0x1FC-0x1FF) of the file that holds
that bank 3: page 1's header for the quick save, page 2's for the autosave. The
game only writes 0x000-0x1FF when it formats the header (`checkBackup`, magic
at 0x000-0x007, the rest zero), and 0x200-0x3FF (`setSaveBank`); it never reads
0x1FC. The tag is the page number (1 or 2) and is written natively right after
bank 3 is saved. A tag of 0 or anything other than 1 and 2 means page 1, which
covers quick saves made before this change.

### Session page

- Loading a log, or creating one, sets the session page to the shown page.
- Resuming the quick save or the autosave sets it from that bank's page tag.
- Church saves default to the session page; saving on another page makes that
  page the session page.
- During play, header and log accesses go to the session page.

### Last-used log (0x200)

`savebank` calls `setSaveBank`, which writes the last-used log into the header
of the routed file. The autosave runs `savebank(3, …)` with the header routed
to the session page, as a quick save does today, so it updates the same
last-used log the player's own save would.

## Menus

### Main menu (`MaterielMenu_LOAD`, native build)

`resume_` is always 0: the root list is Continue / Create / Delete / Settings /
Achievements / Quit, whatever bank 3 holds. The `rootCount()` and
`ROOT_RESUME_COUNT` branches stay for the original build.

### Log list (Continue, Create, Delete)

- L / R switch pages (wrapping). Each switch calls `showPage`, then reloads the
  list through the game's own path (`getCatalogView`, `diary_` refresh) and
  redraws.
- Slot numbers: `drawJournalSlot` prints `index + 1`; it is given
  `shownPage * 3 + i`, so page 2 shows 4, 5, 6. Positions still use `i`.
- A native page label, `L ◀ Page 1/2 ▶ R`, is drawn with `drawMenuParts`, the
  way the Settings entry is, translated in `native/Settings.cpp` into every
  language its own texts have (`pick`).
- Continue only: a fourth line, "Quick save" (page 1) or "Autosave" (page 2).
  - It shows the same details as a log (name, chapter, level, place, play
    time), read from bank 3 of that page with the same code as
    `getCatalogView` (a native helper taking the bank).
  - Empty or invalid (`checkbank(3) != 1`): "Quick save — empty" /
    "Autosave — empty", and it can't be chosen.
  - Choosing it: `useResumeSlot`, then the game's `LOAD_RESUME` state
    (`loadbank(3)`), then `setSessionPage` from the tag.
- `MATERIEL_DIARY_LOAD` gets a fourth item in Continue mode. The list is three
  rows of 26 px from y = 90 inside a frame ending at y = 176; four rows need
  tighter spacing (about 22-24 px) or the title box moved up, to stay inside
  the 192-pixel screen. Checked by screenshot.
- The list opens on the session page (page 1 at the title) each time it is
  entered.

### Church save (`MaterielMenu_SAVE`)

Same L / R paging, page label and slot numbers, three lines. It opens on the
session page. `savebankAsync(active, …)` is unchanged; routing sends it to the
shown page, and a successful save sets the session page to it.

### In-game quick save (`TownMenu_OPERATION_ROOT`)

Unchanged for the player. Bank 3 routes to page 1's file, and the page tag is
set to the session page.

## Autosave

### Trigger

The native frame loop (`native/GameBoot.cpp`, next to the scene journal)
compares `g_Global.getMapName()` with the previous frame's. A change while an
adventure is being played sets *autosave pending*. Arriving from the title
(loading a log, the quick save or the autosave, or starting a new log) clears
it instead, since the player is already at a save point.

### Writing

The autosave is written on the first frame after the change where the player
could open the menu:

- towns and dungeons: `TownPlayerManager::execute()`, on the `!isLock()` path,
  no encounter starting, `isSaveAndBattleOK()` true;
- world map: `FieldPlayerManager`, at the equivalent spot.

There, if pending: `writeAutosave()` routes bank 3 to page 2's file, calls
`savebank(3, SAVETYPE_RESUME)`, sets page 2's tag to the session page,
restores the routing and clears pending. A change that leads into a cutscene
waits for the cutscene to end; a chain of maps without player control gives
one autosave at the end. Battles do not change the map.

Cost: one 15 KiB capture and a 64 KiB atomic file write, a few milliseconds on
one frame.

## Errors

- `dqiv_eur-2.sav` missing: created at start-up as an image with page 1's
  0x000-0x007 magic and every other byte zero (three empty logs, empty bank 3).
  Wrong size or unreadable: the game stops with the same error as for page 1.
  An existing file is never replaced.
- Autosave write failure: `NitroBackup` already turns a `SaveStore` exception
  into a card error, so `savebank` returns false. `writeAutosave` logs a line
  in the journal and the game goes on. Church and quick saves keep today's
  messages.
- Missing or invalid page tag: page 1.
- Empty or corrupted bank 3: its line reads "empty" and can't be chosen.
- `dqiv_eur-2.sav` gets the same `.lock` file as page 1.

## Testing

- Host tests (`tests/cheats/host_tests.cpp`):
  - header and logs follow the shown and session page;
  - bank 3 follows the resume slot; a request across 0xB800 is split;
  - page tags are written and read back, with 0 and invalid values as page 1;
  - `dqiv_eur-2.sav` is created when missing and refused when the wrong size.
- On screen (`tests/screen/game.sh`, copy of the saves):
  - the main menu has no Resume line;
  - Continue pages with L / R, shows 4-6 on page 2, the fourth line fits the
    screen (screenshots);
  - saving to log 4 creates `dqiv_eur-2.sav`;
  - walking through a door writes the autosave; its line shows that place;
    loading it puts the party there;
  - after resuming the autosave of a page-2 log, a church save lands on page 2;
  - a quick save made in log 5 resumes into log 5's page.
- `dqiv_eur.sav` is byte-identical after starting the game and only browsing
  the menus.
