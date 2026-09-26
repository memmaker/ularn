# Ularn 1.7.0: handover

## RVIP progress

### Stage 1 (get + build) — done 2026-09-26

- **Folder:** `~/Games/ularn` (no remote, not pushed). History: upstream's own
  commits (unshallowed), top = `upstream Ularn 1.7.0 @ ef42184`
  (ef421842add954c55639656c5973042963c9b258, https://github.com/ularn/ularn master,
  Josh Bressers, 2025-02-08; version from `configure.ac`). Our commits after it:
  `port:` (bug fixes), `port:` (shim), `web:`, `RVIP:`.
- **Case R**, but not curses: Ularn is a **termcap** game (Larn 12 style). All output
  goes through `lprc/lprcat/lprintf` into `lpbuf` with codes CLEAR, CL_LINE, CL_DOWN,
  ST_START/ST_END, CURSOR x y (`src/header.h`); `lflush()` (io.c) translated them.
  With `-DULARN_PORT`, `lflush()` calls `wc_write()` instead (files still get raw bytes).
- **Frontend files:**
  - `port/wcurses.c` — 80×24 in-memory screen + pane routing (from Larn's). Layout:
    map rows 0-16 × cols 0-66, effects column 69-79, status rows 17-18, message
    area rows 19-23 (new lines at row 23; `'\n'` there = delete-line at 19 = one
    line into the history). Modes: CLEAR → full-screen text pop-up; `wc_overlay()`
    (in `cl_up()`, io.c) → changed cells pop-up; `wc_dungeon()` (end of
    `drawscreen()`) → map. Hooks call `lflush()` first (the buffer is in-band).
  - `port/panes.c` — Status/Inventory panes from `c[]`/`iven[]` (`bot_effect()` added
    in display.c), `wc_css()` Angband colours by object id, `tile_for()` (returns -1:
    text), stubs for tty.c, nap.c and termcap (`tgetent`… ), `setupvt100`/`clearvt100`
    (→ `be_end(wc_saved)`), `nap()` → `wc_nap()`.
  - `port/be_web.c` (EM_JS → `Module.ln`), `port/be_tty.c` (native test: keys from
    stdin, `ULARN_DUMP=<file>` dumps panes), `port/curses.h`, `port/config.h`
    (hand-written; `src/config.h` is configure output), `port/sgtty.h` (empty).
  - `web/index.html`, `web/ularn.js` (from Larn's; no sound buttons yet),
    `web/build.sh`. `Module.ln.text(pane)` / `.hero()` for tests (0 map, 1 status,
    2 msg, 3 inv, 4 pop-up).
- **Other game edits (`#ifdef ULARN_PORT`):** `getcharacter()`/`yylex()` read
  `wc_getch(at_cmd)` (yylex passes 1 = command prompt); no fork: checkpoint
  (`ckpflag=0`), shell escape and mailer (`mail=0`) off; `flushall()` keeps typeahead;
  `sleep(n)` → `nap(n*1000)` (header.h); `died()` sets `wc_saved` for `x==257` (S).
- **Build:** `sh web/build.sh` → `web/dist`: `emcc -O2 -std=gnu89 -fcommon
  -DULARN_PORT -DLIBDIR='"/ularn"' -Iport -w` src (Makefile.in list minus tty.c,
  nap.c) + wcurses/panes/be_web, `-sUSE_ZLIB=1 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536
  -sSTACK_SIZE=1048576 -sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=32MB
  -sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ENV,HEAPU8,addRunDependency,removeRunDependency
  -sFORCE_FILESYSTEM -lidbfs.js -sENVIRONMENT=web --preload-file web/stage@/ularn-data`
  (Umaps, Ufortune, Uhelp). No `-sEXIT_RUNTIME`. Zero wasm-ld signature warnings.
  Copies `rvip-wm.js` from `~/Games/rvip-tools/web/`.
- **Files on the web:** IDBFS `/ularn` = HOME (`ENV.HOME`), LIBDIR and cwd: `Ularn.sav`,
  `Uscore` (made by `makeboard()`), `web-layout.json`; data symlinked from
  `/ularn-data` on every start. Restore deletes the save → autosave at the first
  command prompt (JS `saveReq` on init), every 2 min, on hide; `be_end()` removes it
  unless saved with S.
- **Native:** `make -C port` (headless test binary `port/ularn-test`, run in a dir
  holding the data files with `HOME=` that dir; LIBDIR "."), `make -C port asan`,
  `make -C port clean`. Upstream `./configure` needs a modern `config.sub` and
  `-std=gnu89`; not used.
- **ASan (native, 2026-09-26):** 100 runs × 4000 random keys from new games, 5 save
  (S) → restore → 3000 random keys runs, explicit save/restore status+inventory
  compare: clean after fixes. Objects removed.
- **Upstream bugs fixed** (commit `port: fix upstream memory bugs…`): `hit*flag`
  char vs extern int (main wrote over globals); five probability tables indexed one
  past the end; `movemt()` read outside the map; `time(&long)` overflow on wasm32;
  `cast()` looped with spells going negative.
- **Browser test (own tab, localhost):** class pick → welcome → map, Status,
  Inventory, Messages filled; `i`, `?` (help pages), bank (`$`, store screen, Esc
  back), S → "saved" overlay → reload → same character and position; autosave +
  reload restores; 2×300 random keys: no console errors (only Chrome's
  beforeunload notice from test reloads). IDBFS `/ularn` deleted afterwards.
- **No `--More--`:** Ularn's messages scroll in the 5-line ring without stops.
  Remaining key waits are pagers/screens, not message stops: `more()` ("press space"
  after lists, player.c), `retcont()` ("Press return", help.c: welcome, help end),
  store/bank prompts, `seepage()`. Stage 5: decide whether to shorten these.

**Tiles decision (stage 4 finalises): Amiga Larn set** (larn.org / primeau, MIT, 8×16,
already in `~/Games/larn/port/amiga/`, the same set Larn uses). primeau's larn.org has
a ULarn mode, so the set was drawn for Ularn too: monsters **66/66** (`m0`–`m65`;
`m1u`, `m19u`, `m34u` for lemming/bitbug/lama nobe, `m39v` and `m57v`–`m65v` for the
visible stalker/demons with the Eye), objects **98/98** (`oN` by id; remap 15↔16,
80↔82, Ularn 93–98 → o95–o100). 100% ≥ 95%: no NetHack fallback, one set. Walls
`wN` by neighbours as Larn. Checked by name against primeau's `monsterdata.js`
(`ULARN_monsterlist`) and `object.js`. Stage 1 runs in text mode.

**Quirks:** the player is drawn as `@` by the game (cursor on it); objects are
"bold" (ST_START → A_BOLD); Ularn has no colours of its own (text map is white).
Enter must send `'\n'` (10). Digits are repeat counts, so arrows/keypad send hjklyubn.
Direction tables: `diroffx[]/diroffy[]` in `src/display.c:547` (index 1 = south, like
Larn). `mitem[x][y]` is a struct (`.mon`). Name defaults to the login (`web_user`).

**Open problems:** character name (no `.Ularnopts`; stage 5: ask or write one);
`more()`/`retcont()` screens (above); death shows the scoreboard then `exit()`
without a final key wait — check in stage 5; no Help page (`help.html`) yet;
wizard/`-d` difficulty not exposed.

**Next: stage 2 (explore + stairs).** Reuse Larn's `~/Games/larn/port/rvip.c`:
`first_step()`/`auto_step()`/`start()`/`stop()`/`passable()`/`target()`/
`monster_in_view()`, hooked from `parse()` via `rvip_command()`; its key queue
`wc_push()` lives in Larn's `port/wcurses.c` (not ported yet — add to `wc_getch`).
Use `diroffx/diroffy` from `src/display.c`, `know[][]`, `item[][]`, `mitem[][].mon`;
stop on a new message (`wc_msgs` counter already in wcurses.c) or a key. `>`/`<`
must **never auto-walk into level-skipping shortcuts** (volcanic shaft `OVOLDOWN`,
elevators `OELEVATORDOWN/UP`, trapdoors): only when stood on (R-Larn lesson).
Ularn stairs: `OSTAIRSDOWN`/`OSTAIRSUP` (13/5), dungeon entrance `OENTRANCE` (54).
