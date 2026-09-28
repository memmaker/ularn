/* In-memory screen for Ularn, routed to Angband-style panes.
 *
 * Ularn writes termcap-style codes (header.h: CLEAR, CL_LINE, CL_DOWN,
 * ST_START/ST_END, CURSOR x y) into its output buffer; lflush() passes the
 * buffer to wc_write(), which draws one 80x24 screen: map (rows 0-16,
 * cols 0-66), effects column (cols 69-79), status lines (17-18) and a
 * five-line scrolling message area (19-23, new lines at 23). Panes:
 *   Map        the map area, tiled (tile_for)
 *   Status     built from the game's data (wc_status)
 *   Messages   history + the live row 23
 *   Inventory  built from the pack (wc_inv)
 *   Pop-up     anything else (lists, help, stores, spells), sized to its
 *              content.
 * Mode comes from hooks in the game:
 *   CLEAR code       -> full screen text: pop-up = all non-blank text
 *   wc_overlay()     -> text over the map (cl_up): pop-up = changed cells
 *   wc_dungeon()     -> the map was drawn again (end of drawscreen) */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "curses.h"

/* header.h output codes */
#define ST_START 1
#define ST_END 2
#define CLEAR 5
#define CL_LINE 6
#define CL_DOWN 14
#define CURSOR 15

extern char enable_scroll; /* data.c: 1 = the message area scrolls */
void lflush(void);          /* io.c */

WINDOW *stdscr;
static int LINES = 24, COLS = 80;

enum { M_DUNGEON, M_OVERLAY, M_FULL };
static int mode = M_FULL;
static WINDOW *base; /* the screen when the overlay started */
static WINDOW *pn[NPANES];
static chtype shown[17 * 67]; /* what the Map pane has, per cell */
static int shown_tile[17 * 67];

#define MAP_H 17
#define MAP_W 67
#define MSG_Y 19         /* first row of the message area */
#define LIVE 23          /* the row new messages are written to */
#define HIST 18          /* message history rows */
#define ST_W 42
#define ST_H 15
#define INV_W 44
#define INV_H 31

WINDOW *newwin(int rows, int cols, int y, int x)
{
    WINDOW *w = calloc(1, sizeof(WINDOW));
    int i;
    (void)y; (void)x;
    w->maxy = rows;
    w->maxx = cols;
    w->c = malloc(sizeof(chtype) * rows * cols);
    w->first = malloc(sizeof(short) * rows);
    w->last = malloc(sizeof(short) * rows);
    for (i = 0; i < rows * cols; i++) w->c[i] = ' ';
    for (i = 0; i < rows; i++) { w->first[i] = 0; w->last[i] = (short)(cols - 1); }
    return w;
}

static void delwin(WINDOW *w)
{
    if (!w) return;
    free(w->c); free(w->first); free(w->last); free(w);
}

WINDOW *initscr(void)
{
    if (!stdscr) {
        stdscr = newwin(LINES, COLS, 0, 0);
        base = newwin(LINES, COLS, 0, 0);
        memset(shown, 0xff, sizeof shown);
        pn[P_STATUS] = newwin(ST_H, ST_W, 0, 0);
        pn[P_MSG] = newwin(HIST + 1, COLS, 0, 0);
        pn[P_INV] = newwin(INV_H, INV_W, 0, 0);
        be_init(P_MAP, MAP_W, MAP_H);
        be_init(P_STATUS, ST_W, ST_H);
        be_init(P_MSG, COLS, HIST + 1);
        be_init(P_INV, INV_W, INV_H);
    }
    return stdscr;
}

static void touch(WINDOW *w, int y, int x)
{
    if (w->first[y] < 0 || x < w->first[y]) w->first[y] = (short)x;
    if (x > w->last[y]) w->last[y] = (short)x;
}

static void untouch(WINDOW *w)
{
    int y;
    for (y = 0; y < w->maxy; y++) w->first[y] = w->last[y] = -1;
}

int wmove(WINDOW *w, int y, int x)
{
    if (y < 0 || x < 0 || y >= w->maxy || x >= w->maxx) return ERR;
    w->cury = y;
    w->curx = x;
    return OK;
}

static void set(WINDOW *w, int y, int x, chtype ch)
{
    if (y < 0 || x < 0 || y >= w->maxy || x >= w->maxx || w->c[y * w->maxx + x] == ch) return;
    w->c[y * w->maxx + x] = ch;
    touch(w, y, x);
}

static chtype at(WINDOW *w, int y, int x) { return w->c[y * w->maxx + x]; }

/* a printable character at the cursor; the terminal wraps at the margin */
int waddch(WINDOW *w, chtype ch)
{
    set(w, w->cury, w->curx, (ch & ~A_CHARTEXT) | w->attr | (ch & A_CHARTEXT));
    if (++w->curx >= w->maxx) {
        w->curx = 0;
        if (w->cury + 1 < w->maxy) w->cury++;
    }
    return OK;
}

int wclrtoeol(WINDOW *w)
{
    int x;
    for (x = w->curx; x < w->maxx; x++) set(w, w->cury, x, ' ');
    return OK;
}

static void clrtobot(WINDOW *w)
{
    int y, x;
    wclrtoeol(w);
    for (y = w->cury + 1; y < w->maxy; y++)
        for (x = 0; x < w->maxx; x++) set(w, y, x, ' ');
}

/* ---- message history ---- */

/* add stdscr's row to the history (repeats: "line (xN)"); it fills from
 * the top (nhist rows in use, the live row below them), then scrolls */
static int nhist;
static void hist(int row)
{
    WINDOW *p = pn[P_MSG];
    int y, x, n = COLS;
    static char prev[128];
    static int reps;
    char r[128], sfx[16];
    while (n > 0 && (at(stdscr, row, n - 1) & A_CHARTEXT) == ' ') n--;
    if (n == 0) return;
    for (x = 0; x < n; x++) r[x] = (char)(at(stdscr, row, x) & A_CHARTEXT);
    r[x] = 0;
    if (!strcmp(r, prev)) {
        snprintf(sfx, sizeof sfx, " (x%d)", ++reps);
        for (x = 0; sfx[x] && n + x < p->maxx; x++) set(p, nhist - 1, n + x, (unsigned char)sfx[x]);
        return;
    }
    reps = 1;
    strcpy(prev, r);
    if (nhist == HIST) {
        for (y = 0; y < HIST - 1; y++)
            for (x = 0; x < p->maxx; x++) set(p, y, x, at(p, y + 1, x));
        nhist--;
    }
    for (x = 0; x < p->maxx; x++) set(p, nhist, x, x < n ? at(stdscr, row, x) : ' ');
    nhist++;
}

int wc_msgs; /* messages so far (explore will stop on a new one) */

/* '\n' on the last row of the message area: the terminal's delete-line at
 * row 19 (io.c lflush); the finished line goes to the history */
static void scroll_msgs(void)
{
    int y, x;
    wc_msgs++;
    hist(LIVE);
    for (y = MSG_Y; y < LIVE; y++)
        for (x = 0; x < COLS; x++) set(stdscr, y, x, at(stdscr, y + 1, x));
    for (x = 0; x < COLS; x++) set(stdscr, LIVE, x, ' ');
}

static const char *rowfg[64]; /* stdscr rows: colour set by the port (wc_rowfg) */
void wc_rowfg(WINDOW *w, int y, const char *css)
{
    if (w == stdscr && y >= 0 && y < 64) rowfg[y] = css && *css ? css : NULL;
}

/* ---- the game's output (io.c lflush) ---- */

void wc_write(const char *buf, int n)
{
    WINDOW *w = initscr();
    const char *e = buf + n;
    for (; buf < e; buf++) {
        int ch = (unsigned char)*buf;
        if (ch >= 32) { waddch(w, (chtype)ch); continue; }
        switch (ch) {
        case CLEAR:
            memset(rowfg, 0, sizeof rowfg);
            w->cury = w->curx = 0;
            clrtobot(w);
            mode = M_FULL;
            break;
        case CL_LINE: wclrtoeol(w); break;
        case CL_DOWN: clrtobot(w); break;
        case ST_START: w->attr = A_BOLD; break;  /* Ularn's "bold objects", highlights */
        case ST_END: w->attr = 0; break;
        case CURSOR:
            if (buf + 2 >= e) return;
            wmove(w, (unsigned char)buf[2] - 1, (unsigned char)buf[1] - 1);
            buf += 2;
            break;
        case '\n':
            if (w->cury == LIVE && enable_scroll > 0) scroll_msgs();
            else if (w->cury + 1 < w->maxy) w->cury++;
            w->curx = 0;
            break;
        case '\r': w->curx = 0; break;
        case '\b': if (w->curx > 0) w->curx--; break;
        case '\t': w->curx = (w->curx + 8) & ~7; if (w->curx >= w->maxx) w->curx = w->maxx - 1; break;
        default: break; /* bell, BOLD/END_BOLD: nothing */
        }
    }
}

void wc_dungeon(void) { lflush(); mode = M_DUNGEON; }

void wc_overlay(void)
{
    lflush();
    if (mode != M_DUNGEON) return;
    memcpy(base->c, stdscr->c, sizeof(chtype) * LINES * COLS);
    mode = M_OVERLAY;
}

/* ---- panes ---- */

static int pop_h, pop_w;

/* Text panes go out as whole lines (RVIP W0 rules 5, 6): each changed row
 * once, trimmed, with the row's colour and icon tile (wc_rowattr); and the
 * rows in use (to the last non-blank row or the cursor), so the page shows
 * no empty lines at the bottom. Inside a row: standout between \x01 and
 * \x02, a cell colour other than the row's as a run "\x05#rrggbb" ... "\x06"
 * ("\x05*#rrggbb": bold), so coloured and bold text keeps its look. */
#define RMAX 128
static const char *rcss[NPANES][RMAX];
static int rtile[NPANES][RMAX], rows_sent[NPANES], cur_p = -1, cur_y;

void wc_rowattr(int p, int y, const char *css, int tile)
{
    WINDOW *w = pn[p];
    if (!css) css = "";
    if (!w || y < 0 || y >= w->maxy || y >= RMAX) return;
    if (rcss[p][y] && !strcmp(rcss[p][y], css) && rtile[p][y] == tile) return;
    rcss[p][y] = css; rtile[p][y] = tile;
    touch(w, y, 0);
}

static void cursor(int p, int y, int x) { cur_p = p; cur_y = y; be_cursor(p, y, x); }

static int blank(chtype ch) { return (ch & A_CHARTEXT) == ' ' && !(ch & A_STANDOUT); }

/* a cell's colour run: curses colours 0-7, +8 bold (as the map's palette in
 * ularn.js); bold without a colour keeps the row's colour, bold weight */
static const char *pal[16] = { "#000000", "#cd3131", "#0dbc79", "#e5e510", "#4c7eff", "#bc3fbc", "#11a8cd", "#d7d7d7",
    "#666666", "#f14c4c", "#23d18b", "#f5f543", "#6ea0ff", "#d670d6", "#29b8db", "#ffffff" };
static int run(chtype ch, const char *row, char *out)
{
    int bold = (ch & A_BOLD) != 0, col = ch & 0x800 ? (ch >> 8) & 7 : 0;
    const char *css;
    if (!col && !bold) return 0;
    css = col ? pal[col + 8 * bold] : *row ? row : pal[15];
    return sprintf(out, "\x05%s%s", bold ? "*" : "", css);
}

static void pflush(int i)
{
    WINDOW *p = pn[i];
    int y, x, used = 0;
    if (!p) return;
    for (y = 0; y < p->maxy; y++)
        for (x = 0; x < p->maxx; x++)
            if (!blank(p->c[y * p->maxx + x])) used = y + 1;
    if (cur_p == i && cur_y >= used) used = cur_y + 1;
    if (used != rows_sent[i]) be_rows(i, rows_sent[i] = used);
    for (y = 0; y < p->maxy; y++) {
        char buf[12 * 256 + 4], r[16], cr[16] = "";
        const char *row = y < RMAX && rcss[i][y] ? rcss[i][y] : "";
        int n = 0, so = 0, end = p->maxx;
        if (p->first[y] < 0) continue;
        p->first[y] = p->last[y] = -1;
        while (end > 0 && blank(p->c[y * p->maxx + end - 1])) end--;
        for (x = 0; x < end && x < 256; x++) {
            chtype ch = p->c[y * p->maxx + x];
            int c = ch & A_CHARTEXT, s = (ch & A_STANDOUT) != 0;
            r[run(ch, row, r)] = 0;
            if (c == ' ' && !s) strcpy(r, cr);       /* a blank doesn't break a run */
            if (s != so || strcmp(r, cr)) {
                if (*cr) buf[n++] = 6;
                if (s != so) buf[n++] = (so = s) ? 1 : 2;
                n += sprintf(buf + n, "%s", r);
                strcpy(cr, r);
            }
            buf[n++] = c < 32 || c > 126 ? ' ' : c;
        }
        if (*cr) buf[n++] = 6;
        if (so) buf[n++] = 2;
        buf[n] = 0;
        be_line(i, y, buf, row, y < RMAX && rcss[i][y] ? rtile[i][y] : -1);
    }
}

static void close_popup(void)
{
    if (pop_h) be_popup(0, 0);
    pop_h = pop_w = 0;
}

static void map_refresh(void)
{
    int y, x;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++) {
            int i = y * MAP_W + x;
            chtype ch = wc_mapcell(y, x, at(stdscr, y, x));
            int t = tile_for(y, x, ch);
            if (shown[i] == ch && shown_tile[i] == t) continue;
            shown[i] = ch;
            shown_tile[i] = t;
            be_put(P_MAP, y, x, ch, t);
        }
}

static void msg_refresh(void)
{
    int x, y;
    char r[COLS + 1];
    for (y = nhist + 1; y <= HIST; y++)
        for (x = 0; x < COLS; x++) set(pn[P_MSG], y, x, ' ');
    for (x = 0; x < COLS; x++) {
        set(pn[P_MSG], nhist, x, at(stdscr, LIVE, x));
        r[x] = (char)(at(stdscr, LIVE, x) & A_CHARTEXT);
    }
    r[x] = 0;
    be_prompt(r);                   /* the prompt line over the map */
}

/* Pop-up: bounding box of the text that isn't the game screen underneath. */
static void pop_refresh(void)
{
    int y0 = 99, y1 = -1, x0 = 99, x1 = -1, y, x;
    int cy = stdscr->cury, cx = stdscr->curx;
    int rows = mode == M_OVERLAY ? MSG_Y : LINES;
    for (y = 0; y < rows; y++)
        for (x = 0; x < COLS; x++) {
            chtype ch = at(stdscr, y, x);
            if ((ch & A_CHARTEXT) == ' ') continue;
            if (mode == M_OVERLAY && ch == at(base, y, x)) continue;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
        }
    if (y1 < 0) { close_popup(); return; }
    /* room for the cursor of a prompt ("Which one? _") */
    if (cy >= y0 && cy <= y1 && cx > x1 && cx < COLS) x1 = cx;
    if (y1 - y0 + 1 != pop_h || x1 - x0 + 1 != pop_w) {
        pop_h = y1 - y0 + 1;
        pop_w = x1 - x0 + 1;
        be_popup(pop_h, pop_w);
        delwin(pn[P_POP]);
        pn[P_POP] = newwin(pop_h, pop_w, 0, 0);
        memset(rcss[P_POP], 0, sizeof rcss[P_POP]);
        rows_sent[P_POP] = 0;
    } else {
        untouch(pn[P_POP]);
    }
    for (y = y0; y <= y1; y++) {
        for (x = x0; x <= x1; x++) set(pn[P_POP], y - y0, x - x0, at(stdscr, y, x));
        wc_rowattr(P_POP, y - y0, rowfg[y], -1);
    }
    if (cy >= y0 && cy <= y1 && cx >= x0 && cx <= x1) cursor(P_POP, cy - y0, cx - x0);
}

static void dump(FILE *f, const char *name, WINDOW *p)
{
    int y, x;
    fprintf(f, "== %s\n", name);
    for (y = 0; p && y < p->maxy; y++) {
        for (x = 0; x < p->maxx; x++) fputc((int)(at(p, y, x) & A_CHARTEXT), f);
        fputc('\n', f);
    }
}

int wrefresh(WINDOW *w)
{
    int cy = stdscr->cury, cx = stdscr->curx, i;
    const char *d;
    if (w != stdscr) return OK;
    cursor(-1, 0, 0);
    if (mode != M_FULL) msg_refresh();
    if (mode == M_DUNGEON) close_popup();
    else pop_refresh();
    if (mode != M_FULL) {
        if (mode == M_DUNGEON) map_refresh();
        wc_status(pn[P_STATUS]);
        wc_inv(pn[P_INV]);
        if (cy == LIVE) cursor(P_MSG, nhist, cx);
        else if (mode == M_DUNGEON && cy < MAP_H && cx < MAP_W) cursor(P_MAP, cy, cx); /* the player */
    }
    untouch(stdscr);
    for (i = P_STATUS; i < NPANES; i++)
        if (i != P_POP || pop_h) pflush(i);
    be_flush();
    if ((d = getenv("ULARN_DUMP"))) { /* testing: panes as text */
        FILE *f = fopen(d, "w");
        if (f) {
            fprintf(f, "mode %d cursor %d,%d\n", mode, cy, cx);
            dump(f, "SCREEN", stdscr);
            dump(f, "STATUS", pn[P_STATUS]);
            dump(f, "MSG", pn[P_MSG]);
            dump(f, "INV", pn[P_INV]);
            dump(f, "POP", pop_h ? pn[P_POP] : NULL);
            fclose(f);
        }
    }
    return OK;
}

static char queue[64]; /* keys fed before the keyboard (rvip.c) */
static int answer;     /* rvip.c: the reply to the next non-command prompt */
int wc_raw;            /* rvip.c menus: keep cursor keys (0x100|hjkl.) apart */
int wc_click;          /* Inventory row clicked at the command prompt (0 = none) */

void wc_push(const char *keys)
{
    size_t n = strlen(queue);
    snprintf(queue + n, sizeof queue - n, "%s", keys);
}

void wc_answer(int k) { answer = k; }
int wc_queued(void) { return queue[0] != 0; }

int wc_getch(int at_cmd)
{
    int k;
    if (queue[0]) {
        k = (unsigned char)queue[0];
        memmove(queue, queue + 1, strlen(queue));
        return k;
    }
    if (at_cmd) answer = 0;
    else if (answer) {
        k = answer;
        answer = 0;
        return k;
    }
    lflush();
    wrefresh(initscr());
    k = be_getkey(at_cmd);
    if (k & 0x200) {                 /* a click on Inventory row k & 0xff */
        if (wc_raw) return k;        /* rvip.c's inventory list */
        if (!at_cmd) return wc_getch(0); /* never an answer to a question */
        wc_click = k & 0xff;
        return 'i';                  /* rvip_command() opens that item's menu */
    }
    return wc_raw ? k : k & 0xff; /* the game gets hjkl for cursor keys */
}

/* auto-explore: show the step, then a key pressed meanwhile? (it stays queued) */
int wc_kbhit(void)
{
    lflush();
    wrefresh(initscr());
    if (!queue[0]) {
        int k = be_poll(); /* -1 = no key (masking it first made 255: every walk stopped after a step) */
        if (k > 0 && !(k & 0x200)) { /* a click doesn't stop a walk */
            char s[2] = { (char)(k & 0xff), 0 };
            wc_push(s);
        }
    }
    return queue[0] != 0;
}

void wc_nap(int ms)
{
    lflush();
    wrefresh(initscr());
    be_sleep(ms);
}
