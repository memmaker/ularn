#!/bin/sh
# Build Ularn for the browser (Emscripten + Asyncify) into web/dist.
# Run with sh (zsh doesn't split lists).
set -e
cd "$(dirname "$0")/.."
OUT=web/dist
GAME="action bill create data diag display fortune player help io main monster moreobj
	movem object regen savelev scores show signal sphere store tok"   # src/Makefile.in minus tty, nap
rm -rf "$OUT" web/stage && mkdir -p "$OUT" web/stage
[ -f web/tiles.png ] || python3 port/mktiles.py
cp data/Umaps data/Ufortune data/Uhelp web/stage/
# -DULARN_PORT: the terminal is port/wcurses.c; LIBDIR and HOME are the IDBFS mount
emcc -O2 -std=gnu89 -fcommon -DULARN_PORT -DLIBDIR='"/ularn"' -Iport -w \
	$(for f in $GAME; do echo src/$f.c; done) port/wcurses.c port/panes.c port/rvip.c port/be_web.c \
	-o "$OUT/ularn-core.js" -sUSE_ZLIB=1 \
	-sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=1048576 \
	-sALLOW_MEMORY_GROWTH -sINITIAL_MEMORY=32MB \
	-sEXPORTED_FUNCTIONS=_main \
	-sEXPORTED_RUNTIME_METHODS=FS,IDBFS,ENV,HEAPU8,addRunDependency,removeRunDependency \
	-sFORCE_FILESYSTEM -lidbfs.js -sENVIRONMENT=web \
	--preload-file web/stage@/ularn-data
rm -rf web/stage
cp web/index.html web/ularn.js web/tiles.png "$OUT/"
# the in-page guide (from the desktop Docs, ~/Desktop/Games/Roguelikes/Docs)
python3 web/make-help.py > "$OUT/help.html"
# sound effects (events raised by SOUND() in the game) and the town music
python3 web/sounds.py "$OUT/sound"
mkdir -p "$OUT/music"
cp ~/Projects/heavenAndHell/files/mods/heavenandhell/music/new_town.ogg "$OUT/music/"
ls -la "$OUT"
