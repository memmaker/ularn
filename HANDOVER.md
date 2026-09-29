# Ularn 1.7.0: handover

RVIP import, all stages done. Live: https://ruzzoli.de/roguelikes/ularn/ · repo
https://github.com/memmaker/ularn (remote `memmaker`, branch `master`) · shrine
`roguelikes-index/shrine/ularn.html`.

## Source

- Upstream https://github.com/ularn/ularn master @ ef42184 (Josh Bressers, 2025-02-08), GPL-2;
  upstream history kept, our commits on top. Compare: `…/compare/ef42184...master`.
- Lineage (checked for the shrine): written 1987 by Phil Cordier (UC Santa Cruz), from Larn 12.

## Build and test

- Web: `sh web/build.sh` → `web/dist` (emcc, `-DULARN_PORT -DLIBDIR='"/ularn"'`, Asyncify, IDBFS;
  data Umaps/Ufortune/Uhelp preloaded at `/ularn-data`). Runs `port/mktiles.py` if `web/tiles.png`
  is missing, `web/make-help.py` (Docs entry `ularn.html` in `~/Desktop/Games/Roguelikes/Docs`),
  `web/mksounds.py`. The page loads `../rvip-wm.js`, `../rvip-app.js`, `../rvip-sound.js`.
- `sh web/deploy.sh` (refuses a dirty or unpushed tree).
- Native headless test: `make -C port` → `port/ularn-test` (keys from stdin; run in a dir with the
  data files, `HOME=` that dir); `make -C port asan`; `ULARN_DUMP=<file>` dumps panes; beacons
  print `[beacon …]` to stderr. Upstream `./configure` is not used (`port/config.h` hand-written).
- Wizard mode `=` needs a password via `fgets(stdin)`: native only.

## Port (case R, but termcap, not curses)

- All output goes through `lprc/lprcat/lprintf` into `lpbuf` (codes in `src/header.h`); with
  `ULARN_PORT`, `lflush()` calls `wc_write()`.
- `port/wcurses.c`: 80×24 in-memory screen + pane routing (map rows 0-16 × cols 0-66, effects
  column 69-79, status rows 17-18, message rows 19-23). Modes: CLEAR → full-screen pop-up,
  `wc_overlay()` (in `cl_up()`) → changed cells, `wc_dungeon()` (end of `drawscreen()`) → map.
  Key queue `wc_push()`; `wc_answer(k)` replies once to the next non-command prompt; arrows/keypad
  arrive as `0x100|hjklyubn` (menus see them raw via `wc_raw`); inventory clicks as `0x200|row`.
- Text windows are HTML lines (`be_line`, `be_rows`, `wc_rowattr`); runs `"\x05#rrggbb"` …
  `"\x06"`, bold `"\x05*#rrggbb"` (palette in `run()`). Only the map is a canvas.
- `port/panes.c`: Status/Inventory from `c[]`/`iven[]`, `wc_css()` colours, `wc_mapcell()` map
  colours, `tile_for()` (Amiga tile per map cell, only where the screen shows the game's own char),
  termcap/tty stubs, `clearvt100()` → `be_end()`.
- `port/rvip.c`: explore `~`, `<`/`>` walk to known stairs (never shafts/elevators), Enter menu
  (parsed from `data/Uhelp` page 2), inventory browser + item menus (actions via `wc_push("qa")`),
  `rvip_whatitem()` list for `whatitem()`/`qwhatitem()`, `rvip_askname()` (writes `.Ularnopts`).
  Hooked in `parse()` (`src/main.c`).
- Other game edits (`#ifdef ULARN_PORT`): no fork (checkpoint, shell, mailer off); `more()` /
  `retcont()` take any key; death/quit → "press any key for a new game" → page reloads.
- Upstream bugs fixed: `hit*flag` char vs int, five tables indexed past the end, `movemt()` off
  the map, `time(&long)` on wasm32, `cast()` loop with negative spells.
- Tiles: larn.org Amiga set (primeau, MIT, `port/amiga/`), 100% (65 monsters, 98 objects);
  `port/mktiles.py` → `web/tiles.png` + `port/tilemap.h`. Tiles/None choice in IndexedDB.
- Saves: IDBFS `/ularn` = HOME, LIBDIR, cwd (`Ularn.sav`, `Uscore`, `.Ularnopts`,
  `web-layout.json`). Autosave at the first command prompt, every 2 min, on hide; `be_end()`
  removes the save unless saved with S.
- Sound: `SOUND(e)` (`src/header.h`) → `be_sound()`; `web/mksounds.py` synthesizes one wav per
  event at build time (triangle tones made for Ularn). No music.
- Stage 6 sound search (2026-09-29): Ularn never shipped sounds or music (upstream, larn.org,
  the 1.6 Amiga/Windows release); no fan pack found. Nothing to use.
- Beacon: `died()` (`src/scores.c`) before any `exit()`; every end except S. `ev` win (263),
  quit (300/256), else death; score = gold + bank. Wizard runs are sent too. Killer art:
  `ularn()` in `roguelikes-index/killers/make.py`. Browser death not run (native path verified).

## Open

- After Q (no scoreboard) the final line sits over the old screen.
- After taking stairs Ularn asks again on arrival ("(u) go up?"; native behaviour, answer `s`);
  walking over items to stairs stops at their prompt.
- Explore stops on every gold pickup and often on level 1's breeding lemmings (game design).
- After reading create monster the inventory list reopened (monster not yet seen that tick).
- Shop buys other than the DND store have no sound; `pickup` also sounds after a DND buy.
- Hard mode (`-d`) not exposed.
