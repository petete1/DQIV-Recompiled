# Save Pages and Autosave Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A second save page (logs 4-6) and one rolling autosave written at each map change, loaded from the title's Continue screen.

**Architecture:** A new native `SavePages` unit owns two 64 KiB `SaveStore` images and routes the game's backup reads and writes by offset (header and logs to the shown page, bank 3 to the quick-save or autosave file). The game's load and save menus page with L / R and tell `SavePages` which page they show; a native `Autosave` unit marks map changes and writes `savebank(3, SAVETYPE_RESUME)` into page 2's bank 3 when the player is back in control.

**Tech Stack:** C++20, CMake presets (`linux64-game`, built inside the `vibecode` distrobox: `distrobox enter vibecode -- sh -c 'cd <worktree> && cmake --build --preset linux64-game'`), SDL3, the decompiled game sources under `game/` (native-only changes behind `#ifdef DQIV_NATIVE`).

**Spec:** `docs/superpowers/specs/2026-11-03-save-pages-autosave-design.md`

## Global Constraints

- Page 1 is `saves/dqiv_eur.sav` (`pc::defaultSavePath()`), unchanged layout; page 2 is `saves/dqiv_eur-2.sav`.
- Image layout: header 0x000-0x3FF, log n at `0x400 + n * 0x3C00` (n = 0..2), bank 3 at 0xB800-0xF3FF.
- Page tag: 4 bytes at 0x1FC-0x1FF of the file holding that bank 3, little-endian `uint32_t`; value 1 or 2 is the page, anything else means page 1.
- Header magic: `0x20345144`, `0x45545241` at 0x000 and 0x004 (little-endian `uint32_t`).
- Pages are 0-based in code (`0`, `1`), 1-based on screen and in the tag.
- Game-code changes go under `#ifdef DQIV_NATIVE`; the original branch stays compilable.
- Native menu text goes through `pick(language, en, fr, de, it, es)` in `native/Settings.cpp`.
- `dqiv_eur.sav` must stay byte-identical when the game is only started and the menus browsed.
- Test on a copy of the saves (`tests/screen/game.sh` copies `dist/saves`); never write the user's `dist/saves` or the main checkout's saves.

## Review Focus

1. Quit to title mid-session, then load a different log: no autosave of the previous adventure may land after the load (`autosaveArrived` clears pending) — Task 5 test.
2. Fresh install (page 1 missing, so its in-memory image is all 0xFF): page 2 must still get the real magic constants, not page 1's 0xFF bytes — Task 1 test.
3. A read or write spanning 0xB800 (e.g. a whole-image access) must split across the two routes — Task 1 test.
4. Resume the autosave of a page-2 log, then quick save and church save: both must follow page 2 (session page from the tag), and bank 3 routing must be back on the quick save afterwards — Task 3 and Task 4 steps.
5. A map change that drops straight into a random encounter: the autosave must wait for control after the battle, never be written from inside the battle — Task 5 screen check.

---

### Task 1: `SavePages` core

**Files:**
- Create: `native/SavePages.hpp`, `native/SavePages.cpp`
- Create: `tests/save/save_pages_tests.cpp`
- Modify: `CMakeLists.txt` (add `native/SavePages.cpp` to `dqiv_core`; add `add_executable(dqiv_save_tests EXCLUDE_FROM_ALL tests/save/save_pages_tests.cpp)` linked to `dqiv_core`)

**Interfaces:**
- Produces (namespace `pc`):
  - `enum class ResumeSlot {quick,autosave};`
  - `class SavePages` with
    - `SavePages(std::filesystem::path page1,std::filesystem::path page2);` — opens both `SaveStore`s; if `page2` does not exist, writes a 64 KiB image of zeros with the magic constants at 0x000/0x004.
    - `void read(std::uint32_t offset,std::span<std::uint8_t> destination) const;`
    - `void write(std::uint32_t offset,std::span<const std::uint8_t> source);` — same range errors as `SaveStore` (`std::out_of_range`).
    - `void showPage(int page); int shownPage() const;` (0 or 1, clamped)
    - `void setSessionPage(int page); int sessionPage() const;` (`setSessionPage` also calls `showPage`)
    - `void useResumeSlot(ResumeSlot); ResumeSlot resumeSlot() const;`
    - `int resumePage(ResumeSlot) const;` — reads the tag (0-based result)
    - `void tagResume(ResumeSlot,int page);` — writes the tag (`page + 1`)
    - `static constexpr int pageCount=2;`
  - `SavePages* mountedSavePages();` / `void mountSavePages(SavePages*);` — the instance the game uses (null when none).

- [ ] **Step 1: Write the failing tests** in `tests/save/save_pages_tests.cpp` (plain `main`, a `check(bool,const char*)` that prints and counts failures, returns nonzero on failure; each test uses a fresh temp directory under `std::filesystem::temp_directory_path()`):
  - `createsPage2WithMagic`: page 1 missing → after constructing, `dqiv_eur-2.sav` exists, is 65536 bytes, bytes 0..7 are `44 51 34 20 41 52 54 45`, every other byte is 0.
  - `keepsExistingPage2`: a page 2 file filled with 0xAB survives construction unchanged.
  - `logsFollowShownPage`: `showPage(1)`, write `{1,2,3}` at 0x400 → page 2 file has them at 0x400, page 1 file does not; `showPage(0)` → `read(0x400)` returns page 1 content.
  - `bank3FollowsSlot`: with `showPage(1)` and `useResumeSlot(quick)`, a write at 0xB800 lands in page 1's file; with `useResumeSlot(autosave)` and `showPage(0)` it lands in page 2's file.
  - `splitsAcrossBank3`: `showPage(1)`, `useResumeSlot(quick)`, write 0x20 bytes at 0xB7F0 → first 0x10 in page 2's file, last 0x10 in page 1's file at 0xB800.
  - `tagsRoundTrip`: `tagResume(autosave,1)` → page 2 file bytes 0x1FC..0x1FF are `02 00 00 00`, `resumePage(autosave)==1`; a tag of 0 or 7 reads as page 0.
  - `rejectsWrongSize`: a 100-byte page 2 file → constructor throws `std::runtime_error`.

- [ ] **Step 2: Run them to see them fail**

Run: `distrobox enter vibecode -- sh -c 'cd <worktree> && cmake --build --preset linux64-game --target dqiv_save_tests'`
Expected: build error (`SavePages.hpp` not found).

- [ ] **Step 3: Implement `SavePages`** in `native/SavePages.hpp/.cpp`. Routing: split the request at 0xB800 and 0xF400; `[0,0xB800)` → store of `shownPage()`, `[0xB800,0xF400)` → page 1 store for `quick`, page 2 store for `autosave`, `[0xF400,0x10000)` → store of `shownPage()`. Tags are read and written straight on the owning `SaveStore` at 0x1FC.

- [ ] **Step 4: Run the tests**

Run: `distrobox enter vibecode -- sh -c 'cd <worktree> && cmake --build --preset linux64-game --target dqiv_save_tests && ./build64/dqiv_save_tests'`
Expected: every check passes, exit code 0.

- [ ] **Step 5: Commit** — `git add native/SavePages.* tests/save CMakeLists.txt && git commit -m "Save pages: two save images routed by offset"`

### Task 2: Mount the pages in the game

**Files:**
- Modify: `native/NitroBackup.hpp`, `native/NitroBackup.cpp` (serve `SavePages` instead of `SaveStore`)
- Modify: `native/GameBoot.cpp:229-234`

**Interfaces:**
- Consumes: `SavePages`, `mountSavePages` (Task 1).
- Produces: `NitroBackup(SavePages&)`; while a `NitroBackup` exists, `mountedSavePages()` returns its pages.

- [ ] **Step 1: Change `NitroBackup`** to hold `SavePages*` (read/write calls unchanged in shape), mount it in its constructor and unmount in the destructor.
- [ ] **Step 2: In `GameBoot.cpp`** replace `pc::SaveStore save(savePath)` with `pc::SavePages save(savePath,pc::saveDirectory()/L"dqiv_eur-2.sav")`; the identity path stays derived from `savePath`.
- [ ] **Step 3: Build and boot check**

Run: build as above, then `tests/screen/game.sh start 64`, `tests/screen/game.sh keys wait:15000`, `tests/screen/game.sh shot <scratchpad>/title.png`, `tests/screen/game.sh stop`.
Expected: the title / adventure log menu is on screen; `~/.cache/dq4-screen/saves/dqiv_eur-2.sav` exists (65536 bytes); `cmp` of `~/.cache/dq4-screen/saves/dqiv_eur.sav` with `dist/saves/dqiv_eur.sav` reports no difference.
- [ ] **Step 4: Commit** — `git commit -am "The game reads and writes through the save pages"`

### Task 3: Title menu and Continue / Create / Delete paging

**Files:**
- Modify: `game/src/ov016/MaterielMenu_LOAD/MaterielMenu_LOAD.cpp` (`menuSetup`, `menuDraw`, `menuUpdate` states `LOAD_LOADCHECK`, `LOAD_WRITECHECK`, `LOAD_RESUME`, `rootUpdate`, `dataSelectUpdate`, `updateActiveDiary`), its header for new members
- Modify: `game/src/ov016/MenuTemplate/MenuTemplate_materiel.cpp` (a 4-row variant for Continue)
- Modify: `game/src/ov016/UnkMaterielMenuDraw/MaterielMenuDrawExtra.cpp` (+ its header) — slot number separate from row
- Modify: `game/src/main/profile/SaveLoad.cpp` (+ `SaveLoad.hpp`) — resume-bank view
- Modify: `native/Settings.hpp`, `native/Settings.cpp` — labels

**Interfaces:**
- Consumes: `mountedSavePages()`, `SavePages::showPage/shownPage/setSessionPage/sessionPage/useResumeSlot/resumePage` (Task 1).
- Produces:
  - `void drawJournalSlotNumbered(int row,int number,char* name,int chapter,int level,int town,int time,int y,int flag);` — `number <= 0` hides the number; `drawJournalSlot(index,…)` becomes `drawJournalSlotNumbered(index,index+1,…)`.
  - `static bool profile::SaveLoad::getResumeView(CatalogView* view);` — fills `view` from bank 3 with the same decoding as `getCatalogView` (factor the per-record body into one static helper both use); returns `view->useFlag_ != 0`.
  - `const char* pc::savePageLabel(int language,int page);` → `"< L   Page 1/2   R >"` (fr `Page`, de `Seite`, it `Pagina`, es `Página`); `const char* pc::quickSaveLabel(int language);` → `"Quick save"` (fr `Sauvegarde rapide`, de `Schnellspeicherung`, it `Salvataggio rapido`, es `Guardado rápido`); `const char* pc::autosaveLabel(int language);` → `"Autosave"` (fr `Sauvegarde auto`, de `Autospeicherung`, it `Salvataggio automatico`, es `Autoguardado`).

- [ ] **Step 1: Remove Resume from the root menu** — native build: `menuSetup` leaves `resume_ = 0`; the original `checkbank(3)` stays under `#else`.
- [ ] **Step 2: Paging in the log list** — in `dataSelectUpdate`, `dss::g_Pad.edge() & PAD_BUTTON_R` → `showPage((shownPage()+1)%pageCount)`, `PAD_BUTTON_L` → previous; then `updateActiveDiary()`, `redraw_ = 1`. When entering the list from the root (`LOAD_DATACHECK`), `showPage(sessionPage())` (page 0 at the title, since `sessionPage()` starts at 0).
- [ ] **Step 3: Numbers and label** — draw rows with `drawJournalSlotNumbered(i, shownPage()*3+i+1, …)`; draw `savePageLabel` below the rows with `drawMenuParts`, as the Settings entry is drawn in `menuDraw`.
- [ ] **Step 4: The fourth line (Continue only, `menuMode_ == 0`)** — before drawing, `useResumeSlot(shownPage()==0 ? quick : autosave)` and `getResumeView`; row 3 drawn with `number = 0` and the slot label (`quickSaveLabel` / `autosaveLabel`) via `drawMenuParts`; empty view → the game's empty slot, and choosing it does nothing. Continue uses a 4-item `MenuItem` template whose rows start at y = 34 (the church screen's `MATERIEL_DIARY_SELECT` origin) and the slot draw origin `0x22`, so four 26-px rows end inside the 192-px screen. Choosing the line → `changeStatus(LOAD_RESUME)`.
- [ ] **Step 5: Session page on success** — `LOAD_LOADCHECK` and `LOAD_WRITECHECK` success: `setSessionPage(shownPage())`. `LOAD_RESUME`: keep the slot chosen in Step 4 for `loadbank(3)`; on success `setSessionPage(resumePage(slot))`; in every case `useResumeSlot(quick)` afterwards.
- [ ] **Step 6: Build and screen check** (copy of saves; `game.sh keys` to open Continue, `R`, `L`):

Expected screenshots: root without Resume; page 1 rows numbered 1-3 plus "Quick save"; after `w` (R) rows numbered 4-6 plus "Autosave", both empty; nothing drawn below y = 192; the page label visible. Create on page 2 makes log 4 (then `cmp` shows `dqiv_eur.sav` unchanged and `dqiv_eur-2.sav` changed).
- [ ] **Step 7: Commit** — `git commit -am "Title menu: two pages of logs, the quick save and the autosave"`

### Task 4: Church save paging and quick-save tag

**Files:**
- Modify: `game/src/main/menu/MaterielMenu_SAVE.cpp` (`menuDraw`, `getSaveData`, `dataSelectUpdate`, `MENU_SOUNDWAIT` success)
- Modify: `game/src/ov016/TownMenu_OPERATION/TownMenu_OPERATION_ROOT.cpp:139`

**Interfaces:**
- Consumes: Task 1 `SavePages`, Task 3 `drawJournalSlotNumbered`, `savePageLabel`.

- [ ] **Step 1: Church save opens on the session page** — at the first `getSaveData()` of a menu (`menuSetup`), `showPage(sessionPage())`.
- [ ] **Step 2: Paging** — in `dataSelectUpdate`, L / R as in Task 3 Step 2, then `getSaveData()`, `redraw_ = 1`; rows numbered `shownPage()*3+i+1`; page label drawn. Three rows, no fourth line.
- [ ] **Step 3: Session page on save** — when `saveResult_ == 1` (`MENU_SUCCESS`), `setSessionPage(shownPage())`.
- [ ] **Step 4: Quick save tag** — around `savebank(3, SAVETYPE_RESUME)` in `TownMenu_OPERATION_ROOT`: `showPage(sessionPage())`, `useResumeSlot(quick)` before; `tagResume(quick, sessionPage())` after a true result.
- [ ] **Step 5: Screen check** (copy of saves): load log 4 (made in Task 3), church save → the screen opens on page 2 with the cursor on log 4; save; `dqiv_eur-2.sav` changes, `dqiv_eur.sav` does not. Quick save in log 4, back to title, Continue page 1: "Quick save" shows log 4's hero; load it, church save → opens on page 2.
- [ ] **Step 6: Commit** — `git commit -am "Church save: two pages; the quick save remembers its page"`

### Task 5: Autosave

**Files:**
- Create: `native/Autosave.hpp`, `native/Autosave.cpp` (compiled into the `DragonQuestIV` executable, next to `native/Cheats.cpp`)
- Modify: `native/GameBoot.cpp` (frame loop near the scene journal, line ~290)
- Modify: `game/src/ov000/town/TownPlayerManager.cpp` (`execute()`, `if (!isLock())` before `getPlayerCommand()`)
- Modify: `game/src/ov001/fld/FieldPlayerManager.cpp` (`execute()`, `if (!isLock())` before `getPlayerCommand()`, line ~211)
- Modify: `game/src/ov016/MaterielMenu_LOAD/MaterielMenu_LOAD.cpp` (call `autosaveArrived` at the three success points of Task 3 Step 5)

**Interfaces:**
- Consumes: Task 1 `SavePages`; `profile::SaveLoad::savebank`.
- Produces (namespace `pc`):
  - `void autosaveMapChanged();` — sets pending, unless an arrival is being absorbed (then clears that and stays idle).
  - `void autosaveArrived();` — clears pending and absorbs the next map change.
  - `void autosaveIfPending();` — if pending: clear it; `showPage(sessionPage())`; `useResumeSlot(autosave)`; `savebank(3, SAVETYPE_RESUME)`; on success `tagResume(autosave, sessionPage())`, else `std::cout << "Autosave failed\n"` (the journal); always `useResumeSlot(quick)`.
  - `bool autosavePending();` (for checks)

- [ ] **Step 1: GameBoot map watch** — keep the previous `g_Global.getMapName()` as a `std::string`; when it differs (and the new one is non-empty), call `autosaveMapChanged()`.
- [ ] **Step 2: Hooks** — Town: inside `if (!isLock())`, `if (isSaveAndBattleOK()) pc::autosaveIfPending();` before `getPlayerCommand()`. Field: inside `if (!isLock())`, `pc::autosaveIfPending();` before `getPlayerCommand()`. Both under `#ifdef DQIV_NATIVE`.
- [ ] **Step 3: Arrivals** — `pc::autosaveArrived()` at `LOAD_LOADCHECK`, `LOAD_WRITECHECK` and `LOAD_RESUME` success.
- [ ] **Step 4: Screen checks** (copy of saves, `Autosave failed` must not appear in the journal):
  - load a log in a town, walk through a door → `dqiv_eur-2.sav` bank 3 changed and its tag is 1 (page 1); back to title, Continue, R: "Autosave" shows that hero and the new place; load it → the party stands inside the building;
  - load, walk out of town onto the world map → one autosave (journal scene changes, file timestamp);
  - quit to title right after a map change and load another log: the autosave still shows the first adventure's place, not overwritten by the second until its own first transition;
  - walk onto the world map until an encounter starts right after a transition (fast-forward allowed): the autosave is written only after the battle, when walking again.
- [ ] **Step 5: Commit** — `git commit -am "Autosave at each map change, into page 2's bank 3"`

### Task 6: Final checks

- [ ] **Step 1:** `./build64/dqiv_save_tests` passes; full build has no new warnings in the touched files.
- [ ] **Step 2:** With a fresh copy of the user's saves, start the game, browse Continue pages 1 and 2, Create and Delete without confirming, quit: `cmp` shows `dqiv_eur.sav` byte-identical to the original.
- [ ] **Step 3:** Short play on the user's desktop (real audio, real GPU) through a few doors and a battle; ask the user to confirm the autosave line and load.
