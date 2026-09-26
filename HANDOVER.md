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

### Stage 2 (explore + stairs) — done 2026-09-26

- **Keys:** `~` auto-explore; `<` / `>` walk to the nearest known stairs and take them
  (Ularn has no stair commands of its own: stepping on stairs asks "(d) go down?").
  In the help page (`data/Uhelp`, page 2; the `Ularn -r` line made room).
- **Code:** `port/rvip.c` (ported from `~/Games/larn/port/rvip.c`): `rvip_auto()`,
  `rvip_command()`, `first_step()` (BFS), `auto_step()`, `stairs_at()`, `passable()`,
  `target()`, `monster_in_view()`, `loot()` (= anything `wc_css()` colours, + chest).
- **Main-loop hook:** `parse()` in `src/main.c` (`#ifdef ULARN_PORT`):
  `k = rvip_auto()` (next step while walking; a waiting key stops it), else
  `k = rvip_command(yylex())`; 0 → `nomove=1; return`, -1 → turn used.
- **Shim (`port/wcurses.c`):** key queue `wc_push()` read first by `wc_getch()`;
  `wc_answer(k)` replies once to the next *non-command* prompt (`wc_getch(0)`) —
  explore/stairs answer the door ("o"), stairs ("d"/"u"), entrance ("g") and level-1
  exit ("i") prompts this way; cleared at every command prompt. `wc_kbhit()` flushes the
  screen and polls `be_poll()` (web: `emscripten_sleep(30)` + `js_key(1)`; tty: -1).
- **"Known grid" test:** `know[x][y]` (level 0 is all known, so town explore says
  "Nothing left"); frontier = passable known cell with an unknown neighbour, not yet
  stood on (`visited[level][x][y]`); items are targets until stood on.
- **Stops:** new message (`wc_msgs`, except the expected door-open messages without HP
  loss), level change, blind/confused, a monster letter on screen **within 5 cells**
  (Ularn leaves sleeping monsters drawn after you walk away), a step that didn't move
  (not after a door try: stuck doors are retried), a key.
- **`<`/`>` never walk to shortcuts:** targets are only `OSTAIRSDOWN`, `OSTAIRSUP`, the
  town's `OENTRANCE` (`>`) and level 1's exit cell (33,16) (`<`). Stood on a shaft
  (`OVOLDOWN`/`OVOLUP`), elevator or the entrance, `<`/`>` answers its prompt via
  `lookforobject()`. V1's only way up is the volcano shaft: `<` there says "no way up".
- **Map colours (`wc_mapcell()` in `port/panes.c`, called from `map_refresh()`):**
  monsters red, stairs/shafts/elevators bright yellow, doors/chests yellow, fountains/
  altars/thrones/statues cyan, shops green, known traps red, items as their inventory
  colour (`wc_css()` mapped to the 8 curses colours). Only cells showing the game's own
  char are coloured, so text over the map stays white.
- **Tested:** browser (own tab, localhost): explore on level 1 (gold, items, door
  prompts, stops on monsters), `>` from town walks to the entrance and enters, `<` walks
  to level 1's exit into town, `>` stood on the entrance; save → reload restores. Native
  (`port/ularn-test`, keys piped, wizard teleport): `>` walked to `OSTAIRSDOWN` on
  level 4 → level 5, `<` stood on the up stairs → level 4. ASan: 30 × 3000 random keys
  incl. `~<>`: clean. IDBFS `/ularn` deleted afterwards.
- **Open problems:** after taking stairs Ularn asks again on arrival ("(u) go up?") —
  native behaviour, the player answers `s`; walking over items to stairs stops at their
  prompt (press `>` again); Ularn's level 1 has breeding lemmings that stop explore a lot
  (game design); explore stops on any gold pickup message (as Larn).

**Next: stage 3 (Enter menu + inventory).** Keys arrive in `parse()` (`src/main.c`, one
`switch(k)`; `yylex()` in `src/tok.c` handles repeat-count digits) through
`rvip_command()` in `port/rvip.c` — add Enter (`'\n'`, 10) and `i` there as Larn does
(`cmd_menu()`, `inventory_browse()` in `~/Games/larn/port/rvip.c`). Item actions: push
the keys with `wc_push("qa")` etc. (read by `wc_getch()` before the keyboard, for command
and prompt reads alike). Ularn verbs prompt through `getcharacter()` (io.c) and
`whatitem()`-style code in `src/object.c`/`src/main.c`; floor items ask "(t) take it"
when stepped on. Panes: Map, Status, Messages, Inventory (`wc_inv()` in `port/panes.c`,
`item_name()` there matches show.c), pop-up (`P_POP`); list pop-ups can draw over the
map after `wc_overlay()`. Help lines to parse for the menu: `data/Uhelp` page 2
(three tab-separated columns).

### Stage 3 (Enter menu + inventory) — done 2026-09-26

- **Code:** `port/rvip.c` (ported from `~/Games/larn/port/rvip.c`): `cmd_menu()`, `load_cmds()`,
  `draw_list()`, `inventory_browse()`, `item_menu()`, `actions()`, `run_action()`. Hooked in
  `rvip_command()` (Enter `'\n'`/`'\r'` → `cmd_menu()`, whose key runs as the command; `i` →
  `inventory_browse()`) and `rvip_auto()` (reopens the inventory after an item action unless a
  monster is within 5 cells). `item_name()` in `port/panes.c` is no longer static.
- **Enter menu:** parsed at first use from `data/Uhelp` page 2 (lines after "Help File for" up to
  the first blank; tabs expanded, columns 0/27/56, a column counts only after a blank so long
  entries don't split); listed column by column (moves, runs/info, actions); `< >` becomes two
  entries. 39 commands incl. `~ < >`. Drawn into stdscr's map area (rows 0-16) after
  `wc_overlay()`, so the pane router shows a content-sized pop-up; `close_list()` =
  `draws(0,MAXX,0,MAXY)` + `wc_dungeon()`. Scrolls at 16 rows.
- **Item actions run through the key queue:** `wc_push("qa")` = verb + slot letter, read by
  `yylex()` (command) and `getcharacter()` in io.c (`whatitem()`/`qwhatitem()` prompts, action.c).
  Quaff `q`, read `r`, eat `e`, wield `w` (`w-` puts it away), wear `W`, take off `T` (no letter),
  drop `d`; Examine prints the name. Classes as `whatitem()` sorts them. No floor offers in Ularn.
  Keys in the list: letter = main action, Shift = drop, Ctrl = examine, Enter/→/Numpad5 = menu,
  `+ - *`, Esc/←/0/. close, any other key runs as a command.
- **Cursor keys:** `web/ularn.js` sends arrows/keypad as `0x100|hjklyubn.`; `wc_getch()` masks to
  the plain letter for the game, the menus set `wc_raw` (`port/wcurses.c`) and see the cursor
  codes, so `j`/`k` stay commands in the menu.
- **Tested (browser, own tab, localhost):** Enter menu lists all commands; picked `d` by cursor
  (drop prompt), `g` by letter (ran); inventory: quaff (menu), read (letter), drop (Shift),
  take off + wear (Klingon), put away + wield dagger (Rogue, inventory reached via Enter menu →
  `i`), Ctrl examine, other key moves; arrows/Numpad8 still move. No console errors. ASan:
  30 × 3000 random keys incl. Enter/`i`/item keys: clean. IDBFS `/ularn` deleted.
- **Open problems:** no mouse click on inventory rows (keys only); item prompts ("What do you
  want to quaff [a]?") are still Ularn's letter prompts, no cursor list (Larn's
  `rvip_whatitem()` not ported); Enter is not listed in `Uhelp`; after read-scroll of create
  monster the list reopened (monster not seen by `monster_in_view()` in that tick).

**Next: stage 4 (tiles).** Decision from stage 1: the Amiga Larn set (larn.org / primeau, MIT,
8×16) already in `~/Games/larn/port/amiga/`, one set, no fallback: monsters `m0`–`m65` (+`m1u`,
`m19u`, `m34u`, `m39v`, `m57v`–`m65v`), objects `oN` by id with remaps 15↔16, 80↔82, Ularn
93–98 → o95–o100; walls `wN` by neighbour bits as Larn. Hook: `tile_for()` in `port/panes.c`
(returns -1 = text now), called per map cell from `map_refresh()` in `port/wcurses.c` after
`wc_mapcell()` (panes.c) coloured the cell; only cells showing the game's own char should get a
tile (menus/overlays draw text over the map).
