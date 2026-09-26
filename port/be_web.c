/* Browser frontend for the terminal shim (RVIP step 7): web/ularn.js draws
 * the panes (Module.ln); input waits with Asyncify. At the command prompt
 * the page can ask for an autosave. Ularn deletes the save when it
 * restores it, so the autosave keeps the game alive across reloads;
 * be_end() removes it when the game ends without 'S'. */
#include <emscripten.h>
#include <stdio.h>
#include <string.h>
#include "../src/header.h"
#include "../src/player.h"
#include "../src/monst.h"
#include "../src/extern.h"
#include "curses.h"

EM_JS(void, js_init, (int p, int c, int r), { Module.ln.init(p, c, r); });
EM_JS(void, js_put, (int p, int y, int x, int ch, int t), { Module.ln.put(p, y, x, ch, t); });
EM_JS(void, js_cursor, (int p, int y, int x), { Module.ln.cursor(p, y, x); });
EM_JS(void, js_popup, (int r, int c), { Module.ln.popup(r, c); });
EM_JS(void, js_flush, (int lvl, int hy, int hx), { Module.ln.flush(lvl, hy, hx); });
EM_JS(int, js_key, (int at_cmd), { return Module.ln.key(at_cmd); });
EM_JS(void, js_prompt, (const char *s), { Module.ln.prompt(UTF8ToString(s)); });
EM_JS(int, js_want_save, (void), { return Module.ln.wantSave(); });
EM_JS(void, js_end, (int saved), { Module.ln.end(saved); });
EM_JS(void, js_invfg, (int y, const char *c), { Module.ln.invfg(y, UTF8ToString(c)); });
EM_JS(void, js_vis, (const char *s), { if (Module.ln.vis) Module.ln.vis(UTF8ToString(s)); });

void be_init(int p, int cols, int rows) { js_init(p, cols, rows); }
void be_put(int p, int y, int x, chtype ch, int tile) { js_put(p, y, x, (int)ch, tile); }
void be_cursor(int p, int y, int x) { js_cursor(p, y, x); }
void be_popup(int rows, int cols) { js_popup(rows, cols); }
void be_prompt(const char *s) { js_prompt(s); }
void be_sleep(int ms) { emscripten_sleep(ms); }

void be_invfg(int y, const char *css)
{
    static const char *last[64];
    if (y < 64 && last[y] != css) { last[y] = css; js_invfg(y, css); }
}

/* Visible window: monsters next to the player (what Ularn shows of them
 * without awareness) and the objects drawn on the map */
static void send_visible(void)
{
    static char buf[4096];
    int n = 0, x, y, r = c[AWARENESS] ? 3 : 1, m, o;
    if (c[BLINDCOUNT]) r = -1;
    for (y = playery - r; y <= playery + r; y++)
        for (x = playerx - r; x <= playerx + r; x++)
            if (x >= 0 && y >= 0 && x < MAXX && y < MAXY && (m = mitem[x][y].mon) && n < 3900)
                n += snprintf(buf + n, sizeof buf - n, "M%c%s\n", monstnamelist[m], monster[m].name);
    for (y = 0; y < MAXY; y++)
        for (x = 0; x < MAXX; x++)
            if (know[x][y] && (o = item[x][y]) && objnamelist[o] > ' ' && n < 3900 && !strchr("#.", objnamelist[o]))
                n += snprintf(buf + n, sizeof buf - n, "I%c%s\t%s\n", objnamelist[o], objectname[o], wc_css(o));
    buf[n] = 0;
    js_vis(buf);
}

void be_flush(void)
{
    send_visible();
    /* the player's cell: the page scrolls a zoomed-in map to keep it in view */
    js_flush(c[HP] > 0 ? level : -1, playery, playerx);
}

int be_getkey(int at_cmd)
{
    for (;;) {
        int k = js_key(at_cmd);
        if (k >= 0) return k;
        if (at_cmd && c[HP] > 0 && js_want_save()) { /* the command prompt: a safe moment */
            char es = enable_scroll;
            savegame(savefilename);
            enable_scroll = es;             /* savegame() switches to literal output */
        }
        emscripten_sleep(10);
    }
}

int be_poll(void)
{
    emscripten_sleep(30); /* shows the step; the page's keys arrive */
    return js_key(1);
}

void be_end(int saved)
{
    if (!saved) remove(savefilename); /* died or quit: no character to come back */
    js_end(saved);
}
