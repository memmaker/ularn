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
- **Item prompts:** `rvip_whatitem(verb)` (`port/rvip.c`), called first in `whatitem()` and
  `qwhatitem()` (`src/action.c`, `#ifdef ULARN_PORT`): list of the items that fit (drop: all),
  cursor, letter or Enter picks, `-` none (wield), `.` gold (drop), `*` as before, Esc/Space
  cancel ("Aborted."). Returns -1 while `wc_queued()` (keys pushed by an item action), so the
  game's own prompt reads them without a list flash. Tested: `q`, Enter menu → `q`, `r` (cursor),
  `w` with no weapon, `d` letter, Esc; inventory actions skip the list. ASan 30 × 3000 keys clean.
- **Open problems:** no mouse click on inventory rows: `web/ularn.js` delivers no pane clicks
  to C (keys only); Enter is not listed in `Uhelp`; after read-scroll of create
  monster the list reopened (monster not seen by `monster_in_view()` in that tick).

**Next: stage 4 (tiles).** Decision from stage 1: the Amiga Larn set (larn.org / primeau, MIT,
8×16) already in `~/Games/larn/port/amiga/`, one set, no fallback: monsters `m0`–`m65` (+`m1u`,
`m19u`, `m34u`, `m39v`, `m57v`–`m65v`), objects `oN` by id with remaps 15↔16, 80↔82, Ularn
93–98 → o95–o100; walls `wN` by neighbour bits as Larn. Hook: `tile_for()` in `port/panes.c`
(returns -1 = text now), called per map cell from `map_refresh()` in `port/wcurses.c` after
`wc_mapcell()` (panes.c) coloured the cell; only cells showing the game's own char should get a
tile (menus/overlays draw text over the map).

### Stage 4 (tiles) — done 2026-09-26

- **Set:** Amiga Larn tiles, larn.org / primeau (https://github.com/primeau/Larn `src/img`, MIT,
  `port/amiga/LICENSE`), 8×16, copied from `~/Games/larn/port/amiga/` into `port/amiga/`. One set,
  no fallback. Credit it on the Help page (stage 5/6).
- **Id map:** `port/mktiles.py` (run from the repo root; `web/build.sh` runs it if `web/tiles.png` is
  missing) → `web/tiles.png` (32 per row, 181 tiles) + `port/tilemap.h` (`mon_tile[66]`,
  `obj_tile[99]`, `wall_tile[16]`, `PLAYER_TILE`). ULarn art: `m1u`, `m19u`, `m34u`, `m39v`,
  `m57v`–`m65v`; objects `oN` with 15↔16, 80↔82, 93–98 → o95–o100; `OWALL` by neighbour bits
  (wall, open/closed door count as wall) as Larn.
- **Coverage (script, counts `src/data.c` `monster[]` and `src/itm.h` ids):** monsters 65/65,
  objects 98/98 = **100%**; the script asserts every id has a tile file.
- **Loader:** `tile_for()` in `port/panes.c` decides per map cell (the screen char must be the
  game's own: `monstnamelist[mitem.mon]` / `objnamelist[item]`; unknown, blank and text over the
  map stay text; a disguised mimic gets the tile of the monster it shows; hero = `@` at
  `playerx/playery` → `player.png`, Larn's green block). JS (`web/ularn.js` `draw()`) only blits
  `tiles.png`; `imageSmoothingEnabled = false` (nearest-neighbour), cell = tile height × half.
- **Page:** `Tiles`/`Text` button (`btn-tiles`), choice in `localStorage` `ularn-tiles`.
- **Fixed on the way (stage 3 regression):** `wc_kbhit()` masked `be_poll()`'s -1 to 255, so every
  `~`/`<`/`>` walk stopped after one step. Now masks only real keys.
- **Tested (browser, own tab, localhost):** town (shops, bank, home, entrance, hero), `>` walked into
  level 1, `~` explore (walls, doors, gold, scroll, potion, book, dagger, ruby, down stairs, lemming,
  hobgoblin), stairs to level 2 (gnome, up stairs, pit); canvas pixels at those cells are exact
  palette colours (no smoothing), text mode gives the grey antialiased glyphs; toggle both ways.
  No console errors. ASan: 20 × 3000 random keys incl. `~<>`: clean. IDBFS `/ularn` deleted.
- **Open problems:** Ularn draws the object, not `@`, under the hero while an item/stairs prompt is
  up (native); the Amiga player tile is a plain green block (Larn has the same); water/lava do not
  exist in Ularn, so no text-only cells remain on the map.

**Next: stage 5 (web page).** Open problems from stage 1 that stage 5 must handle: character name
defaults to `web_user` → a proper name prompt (no `.Ularnopts`); `more()`/`retcont()` "press
space/return" screens; death shows the scoreboard then `exit()` without a key wait; no Help page
(`help.html`, credit the Amiga tiles there); inventory click not wired in `web/ularn.js` (keys only).

### Stage 5 (web page) — done 2026-09-26

- **Windows** (`web/index.html`, `web/ularn.js` on `rvip-wm.js`, copied by `build.sh`): Map, Messages
  (history), Status, Inventory, Visible, plus the text pop-up (`#pop`, pane 4) over the map. Top bar:
  title `ULARN` (stage 8 links it to the shrine), Help, Zoom −/+, Tiles/Text, Windows ▾ (reset,
  one/multi-window), Export/Import save, New game. Colours, tiles and pane contents come from C (W0).
- **Layout file:** IDBFS `/ularn/web-layout.json` (splits, tile size, fonts, `wm` state); survives
  reload (tested: dragged split kept).
- **Name:** `rvip_askname()` (`port/rvip.c`), called in `main()` (`src/main.c`, `#ifdef ULARN_PORT`)
  before `makeplayer()` on a new game: "What is your name?" in the pop-up, prefilled with the last
  name (Backspace edits, Enter takes it, max 20 chars). Written as `name: "<name>"` to
  `.Ularnopts` in HOME (IDBFS `/ularn`), which the game's own `readopts()` reads. `web_user` is no
  longer shown (it stays `loginname`, used only by the disabled mailer).
- **Key waits:** `more()` (player.c) and `retcont()` (help.c) take any key under `ULARN_PORT`
  ("press any key to continue"); the help pager and stores keep their own keys.
- **Death / quit:** `clearvt100()` (`port/panes.c`) shows "--- press any key for a new game ---" on
  row 24 of the last screen (the scoreboard after a death) when the game ends without S, then
  `be_end(0)`; `ularn.js` `end()` syncs IDBFS and reloads the page → new game, name prompt with the
  last name. S still shows the "saved, Play again" overlay. `died()`'s early `exit(0)` (x 256 or a
  negative code) now calls `clearvt100()` first. No `-sEXIT_RUNTIME`.
- **Inventory click:** mousedown on an Inventory row sends `0x200|row` (ularn.js); `wc_getch()`
  (`port/wcurses.c`) turns it at the command prompt into `i` + `wc_click`, and `inventory_browse()`
  opens that row's item menu (row r = r-th carried item, as `wc_inv()` lists them). Inside the list a
  click moves the cursor and opens the menu; other prompts and walks ignore clicks.
- **Help:** `build.sh` writes a stub `dist/help.html` (keys + Amiga tile credit); the page fetches
  `help.html` on first Help click (`toggleHelp()` in ularn.js).
- **deploy.sh:** `web/deploy.sh` → `ruzzoli.de/roguelikes/ularn/` with the roguelikes-index guard
  (refuses a dirty tree or one not equal to its upstream). **Not deployed, no repo:** dry run printed
  "commit + push first".
- **Tested (browser, own tab, localhost):** name prompt → "Zorba the Klingon" in Status, `.Ularnopts`
  written; welcome any key; inventory click at the prompt and inside the list opens the right item
  menu; split drag saved and restored after reload; S → overlay → reload restores position; random
  fighting died → page reloaded into a new game with the name offered, save gone; Q → final screen
  → key → new game. Native: death on level 18 (wizard) → scoreboard → final screen. ASan: 30 × 3000
  random keys incl. name, `Q`, prompts: clean. IDBFS `/ularn` deleted afterwards.
- **Open problems:** after Q (no scoreboard) the final line sits over the old screen; the browser
  scoreboard screen itself was only seen natively (the browser death was reached, the reload
  followed); hard mode (`-d`) not exposed; no sound, no og tags (step 5b needs the index card).

**Next: stage 6 (docs + sound).** Sound hook: messages go through `wc_write()` →
`scroll_msgs()`/`hist()` in `port/wcurses.c`; better, add `SOUND("event")` calls in the game as Larn
does (`~/Games/larn/larnfunc.h`, `port/be_web.c` `Module.ln.sound`, `web/sounds.py`, top-bar
Sound/Music buttons in `~/Games/larn/web/larn.js`, off by default, state in `web-layout.json`
`audio`). Help: replace the stub in `web/build.sh` with `web/make-help.py` (copy Larn's, `PAGE =
'ularn.html'`), fetched by `toggleHelp()` in `web/ularn.js`. Docs: add an Ularn entry to
`~/Desktop/Games/Roguelikes/Docs` (`build-docs.py` GAMES + `guides.py` GUIDES, with a Tips section);
credit the Amiga tiles (primeau, MIT) there and on the Help page.

### Stage 6 (docs + sound) — done 2026-09-26

- **Docs** (`~/Desktop/Games/Roguelikes/Docs`, not a git repo): `build-docs.py` GAMES entry
  `ularn.html` (after Larn's) with `parse_ularnhelp()` (39 keys from `data/Uhelp` page 2: after
  "Help File for", tabs expanded, columns 0/27/56, `< >` split in two), Tips, "In the browser"
  notes; `guides.py` GUIDES `'ularn.html'` ("How Ularn differs from Larn", "Your first steps",
  "Staying alive") and SAVING `'ularn.html'`. `python3 build-docs.py` → 32 pages, `ularn.html` +
  index card. Credits: Ularn 1.7.0 (Phil Cordier 1992; Josh Brandt, Josh Bressers et al.), GPL-2;
  Amiga tiles larn.org/primeau MIT; Dubtrain sound pack.
- **Help:** `web/make-help.py` (Larn's, `PAGE='ularn.html'`, pops "How Ularn differs from Larn",
  keys-to-remember `~ Enter i < > g S`, About-this-version with ef42184 + compare link
  `memmaker/ularn/compare/ef42184...master`). `web/build.sh` writes `dist/help.html` with it (stub
  gone), so a Docs change needs a rebuild (or `python3 web/make-help.py > web/dist/help.html`).
- **Sound:** `SOUND(e)` in `src/header.h` (`ULARN_PORT` → `be_sound()`, else no-op); `be_sound` in
  `port/be_web.c` (→ `Module.ln.sound`), stub in `port/be_tty.c`, prototype in `port/curses.h`.
  Events (Dubtrain names): `hit`, `miss`, `kill`, `mon_hit`, `cast_spell` (monster.c), `level`,
  `pickup` (player.c), `money1`, `opendoor`, `shutdoor`, `stairs_up`, `stairs_down` (object.c),
  `store5` (store.c, DND buy), `death` (scores.c, not for a win). `web/sounds.py` (Larn's) copies the
  used Dubtrain samples to `dist/sound` + `sounds.json`; `pickup` has no Dubtrain event →
  `plm_chest_latch.wav`. `web/ularn.js` picks a random file per event and plays it through the shared
  `rvip-sound.js` (copied by build.sh). Music: Larn's town loop (`new_town.ogg` from heavenAndHell)
  at level 0. Buttons Sound/Music in the top bar, off by default, stored in `web-layout.json`
  `audio`; `Module.ln.sounds()` counts played sounds (testing).
- **Tested (browser, own tab, localhost):** Help shows the uLarn guide (class list, Tips, credits);
  fresh load Sound/Music off; Sound on → money1, miss, hit, kill, pickup counted and their wavs
  fetched; reload keeps Sound on; Music on in town fetched the ogg. No console errors. ASan
  (`make -C port asan`): 15 × 3000 random keys clean. IDBFS `/ularn` deleted.
- **Open problems:** Music toggled by a script click is blocked by autoplay (a real click works);
  shop buys other than the DND store (trading post, college, bank) have no sound; `pickup` also
  sounds after a DND buy (take() prints "You pick up").

**Next: stage 7 (publish).** Needs: the orchestrator creates `memmaker/ularn` first. README.md (top):
upstream = https://github.com/ularn/ularn at ef42184 (Ularn 1.7.0, Josh Bressers et al.), compare
link `…/compare/ef42184...master`; lineage Larn (Noah Morgan 1986) → Larn 12.x → Ularn (Phil
Cordier 1992) → Josh Brandt 1.5 → Josh Bressers 1.7.0; the port = termcap output (`lflush()`) into
the pane shim `port/wcurses.c` + `panes.c`, `rvip.c` (explore `~`, `<`/`>`, Enter menu, inventory),
web build `web/build.sh`, native test `make -C port`; controls (keys above, click inventory rows);
credits (Ularn GPL-2, Amiga tiles primeau MIT, Dubtrain sounds, town music). Index: card in
`~/Games/roguelikes-index/index.html` + `ularn.png` (see commit 17d4c04 DynaHack: card + png) and a
tree `<li>` under Larn → "Larn 12.4" beside `Larn (RL_M 26.4)` (Ularn forks Larn 12). Then
`web/deploy.sh` (needs pushed, clean tree), check og tags (step 5b), add the repo to RVIP.md W2.

### Stage 7 (publish) — done 2026-09-26

- **Live:** https://ruzzoli.de/roguelikes/ularn/ (deployed from pushed `3815bfd`; name prompt →
  class list, Help guide opens; test IDBFS `/ularn` on ruzzoli.de deleted). Repo
  https://github.com/memmaker/ularn (branch `master`), `README.md` = port notes + upstream
  ef42184 + compare link; upstream `README` kept.
- **Index** (`~/Games/roguelikes-index`, commit `3ec5693` "Add uLarn", deployed, curl diff empty):
  card after Larn's with `ularn.png` (63 Amiga monster tiles, 24x5 at 2x, 384x160), tree `<li>`
  "uLarn" under Larn → Larn 12.4, beside Larn (RL_M 26.4); og count 27 → 28.
- **og tags:** `<!--og-->` block in `web/index.html` (written for uLarn only, as og.py's game loop
  would); live `og:image` = `roguelikes/ularn.png`. Rerun `og.py` after the shrine exists (it
  also adds the shrine page's tags).
- **RVIP.md:** W2 row, case R example, stage 7 lesson (commit `b3a8762`, not pushed).
- **Open problems:** tree lineage from the handover/README (Cordier 1992 from Larn 12), not
  cross-checked on the web yet; stage 8 should verify it (RogueBasin).

**Next: stage 8 (shrine).** Page `~/Games/roguelikes-index/shrine/ularn.html` + folder
`shrine/ularn/` (manual, licence, changelog), styles only `shrine/shrine.css`. Templates:
`shrine/dynahack.html` (structure) and `shrine/larn.html` (closest game, reuse facts on Larn's
history). Sources here: manual = `data/Uhelp` (in-game help text) + upstream `README`;
spoilers/strategy = `README.spoilers` (Phil Cordier's web site guide, predates 1.7.0); licence =
`LICENSE` (GPL-2); changelog = `CHANGES.text` (+ `TODO`). Stats from `src/data.c` / `itm.h` /
`header.h` (classes, monsters, objects, spells, 15+5 levels). Walkthrough: none known; check
RogueBasin/web, else rules of thumb + README.spoilers. Cheats: wizard mode `=` (password read with
`fgets(stdin)`, native only; not reachable in the web build). Then link it: card
`<a class="play info" href="shrine/ularn.html">Info</a>`, tree ✦ after the uLarn link,
`#bar h1` in `web/index.html` → `<a href="../shrine/ularn.html">` (+ `#bar h1 a` CSS); run
`og.py` (or its shrine part), commit + push both repos, both `deploy.sh`, check the three links live.
