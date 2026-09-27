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
EM_JS(void, js_invfg, (int y, const char *c, int t), { Module.ln.invfg(y, UTF8ToString(c), t); });
EM_JS(void, js_rowfg, (int p, int y, const char *c), { Module.ln.rowfg(p, y, UTF8ToString(c)); });
EM_JS(int, js_icons, (void), { return Module.ln.icons(); });
EM_JS(void, js_sound, (const char *s), { Module.ln.sound(UTF8ToString(s)); });
EM_JS(void, js_vis, (const char *s), { if (Module.ln.vis) Module.ln.vis(UTF8ToString(s)); });

void be_init(int p, int cols, int rows) { js_init(p, cols, rows); }
void be_put(int p, int y, int x, chtype ch, int tile) { js_put(p, y, x, (int)ch, tile); }
void be_cursor(int p, int y, int x) { js_cursor(p, y, x); }
static const char *rowfg_sent[64]; /* be_rowfg: colour last sent per pop-up row */
void be_popup(int rows, int cols) { memset(rowfg_sent, 0, sizeof rowfg_sent); js_popup(rows, cols); }
void be_rowfg(int p, int y, const char *css)
{
    if (y < 64 && rowfg_sent[y] != css) { rowfg_sent[y] = css; js_rowfg(p, y, css); }
}
int be_icons(void) { return js_icons(); }
void be_prompt(const char *s) { js_prompt(s); }
void be_sleep(int ms) { emscripten_sleep(ms); }
void be_sound(const char *event) { js_sound(event); }

/* RVIP step 12: one beacon per finished run, through the rvip-wm.js outbox */
EM_JS(void, js_beacon, (const char *g, const char *ev, const char *name, const char *killer, int depth, int score, int turns, int lvl), {
    try {
        var p = [['g', UTF8ToString(g)], ['ev', UTF8ToString(ev)], ['name', name ? UTF8ToString(name) : ''],
                 ['killer', killer ? UTF8ToString(killer) : ''], ['depth', depth], ['score', score], ['turns', turns], ['lvl', lvl]];
        var q = p.filter(function (a) { return a[1] !== '' && !(a[1] < 0); })
                 .map(function (a) { return a[0] + '=' + encodeURIComponent(a[1]); }).join('&');
        if (window.RvipWM && RvipWM.report) RvipWM.report(q); else fetch('/roguelikes/beacon?' + q, { keepalive: true, mode: 'no-cors' }).catch(function () {});
    } catch (e) {}
});
void be_beacon(const char *ev, const char *name, const char *killer, int depth, int score, int turns, int lvl)
{
    js_beacon("ularn", ev, name, killer, depth, score, turns, lvl);
}

void be_invfg(int y, const char *css, int tile)
{
    static const char *last[64];
    static int lastt[64];
    if (y < 64 && (last[y] != css || lastt[y] != tile)) { last[y] = css; lastt[y] = tile; js_invfg(y, css, tile); }
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
                n += snprintf(buf + n, sizeof buf - n, "M%c%s\t\t%d\n", monstnamelist[m], monster[m].name, wc_montile(m));
    for (y = 0; y < MAXY; y++)
        for (x = 0; x < MAXX; x++)
            if (know[x][y] && (o = item[x][y]) && objnamelist[o] > ' ' && n < 3900 && !strchr("#.", objnamelist[o]))
                n += snprintf(buf + n, sizeof buf - n, "I%c%s\t%s\t%d\n", objnamelist[o], objectname[o], wc_css(o), wc_objtile(o));
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
    emscripten_sleep(40); /* shows the step (RVIP-Finetuning: 40 ms); the page's keys arrive */
    return js_key(1);
}

void be_end(int saved)
{
    if (!saved) remove(savefilename); /* died or quit: no character to come back */
    js_end(saved);
}
