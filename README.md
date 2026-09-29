**RVIP port** of Ularn 1.7.0, upstream
[ularn/ularn @ `ef42184`](https://github.com/ularn/ularn/tree/ef42184)
(Josh Bressers et al.). Play: https://ruzzoli.de/roguelikes/ularn/
Our changes: https://github.com/memmaker/ularn/compare/ef42184...master

Lineage: Larn (Noah Morgan, 1986) → Larn 12.x → Ularn (Phil Cordier, 1992)
→ Ularn 1.5 (Josh Brandt) → Ularn 1.7.0 (Josh Bressers). The upstream
notes are in `README`, `README.spoilers` and `CHANGES.text`.

What this port adds (the game code in `src/` is nearly untouched):
- **Terminal shim**: Ularn writes termcap/VT100 output through `lflush()`;
  under `ULARN_PORT` that goes into a small curses-like pane shim
  (`port/wcurses.c`, `port/panes.c`) that splits the screen into map,
  messages, status and inventory windows.
- **Explore** `~`: walks to the nearest unexplored spot, stops for monsters
  and new messages. **`<` / `>`** take the stairs you stand on, or walk to
  the nearest known ones (`port/rvip.c`).
- **Enter menu**: a floating list of every command.
- **Inventory** `i`: cursor list with item menus; click an inventory row to
  open its menu.
- **Tiles**: the Amiga Larn tile set, scaled nearest-neighbour.
- **Sound** (off by default): effects synthesized for Ularn at build time; no music.
- **Saves** live in the browser's IndexedDB (`S` saves and quits).

Build: `sh web/build.sh` → `web/dist` (needs emcc, python3 + Pillow).
Deploy: `sh web/deploy.sh`. Native test build: `make -C port`
(`make -C port asan` for ASan). Notes: `HANDOVER.md`.

Credits: Ularn by Phil Cordier, Josh Brandt, Josh Bressers and contributors,
GPL-2 (`LICENSE`); Larn by Noah Morgan. Amiga Larn tiles from larn.org
(primeau), MIT. Sound effects: synthesized for this port (`web/mksounds.py`).
