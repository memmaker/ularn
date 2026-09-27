/* Ularn port glue: the Status and Inventory panes built from the game's
 * own data, the map tile hook, and what replaces tty.c / nap.c / termcap
 * (the shim is the terminal). */
#include <stdio.h>
#include <string.h>
#include "../src/header.h"
#include "../src/player.h"
#include "../src/itm.h"
#include "../src/monst.h"
#include "../src/extern.h"
#include "curses.h"

const char *bot_effect(int i); /* display.c */

/* ---- terminal: nothing to set up ---- */
short ospeed;
int tgetent(char *b, const char *n) { (void)b; (void)n; return 1; }
int tgetflag(char *id) { (void)id; return 1; }
char *tgetstr(char *id, char **a) { (void)id; (void)a; return "-"; }
char *tgoto(const char *cm, int x, int y) { (void)cm; (void)x; (void)y; return ""; }
int tputs(const char *s, int n, int (*f)(int)) { (void)s; (void)n; (void)f; return 0; }
int setctty(void) { return 0; }
int gettty(void) { return 0; }
int settty(void) { return 0; }
int setuptty(void) { return 0; }
int scbr(void) { return 0; }
int sncbr(void) { return 0; }
int setupvt100(void) { clear(); setscroll(); return 0; }
int wc_saved;
int clearvt100(void)
{
    resetscroll();
    if (!wc_saved) { /* died or quit: a last look at the scoreboard, then a new game */
        cursor(1, 24); cltoeoln();
        lprcat("  --- press any key for a new game --- ");
        getcharacter();
    }
    lflush();
    be_end(wc_saved);
    return 0;
}
extern int nonap;
int nonap = 0;
void nap(int ms) { if (ms > 0 && !nonap) wc_nap(ms); }
void ularn_napms(int ms) { nap(ms); }

/* ---- map ---- */
/* Map cell -> Amiga Larn tile (port/tilemap.h, port/mktiles.py). The char on
 * screen must be what the game draws there, so blindness, unknown cells,
 * invisible monsters and text over the map stay text. */
#include "tilemap.h"

static int is_wall(int x, int y)
{
    int o;
    if (x < 0 || y < 0 || x >= MAXX || y >= MAXY) return 0;
    o = item[x][y];
    return o == OWALL || o == OCLOSEDDOOR || o == OOPENDOOR;
}

int tile_for(int y, int x, chtype ch)
{
    int c = (int)(ch & A_CHARTEXT), m, o;
    if (x >= MAXX || y >= MAXY || c <= ' ') return -1;
    if (x == playerx && y == playery && c == '@') return PLAYER_TILE;
    if (!know[x][y]) return -1;
    if ((m = mitem[x][y].mon) > 0 && m < (int)(sizeof mon_tile / sizeof mon_tile[0])) {
        if (c == monstnamelist[m]) return mon_tile[m];
        if (m == MIMIC) /* disguised: the tile of the monster it looks like */
            for (o = 1; o < (int)(sizeof mon_tile / sizeof mon_tile[0]); o++)
                if (c == monstnamelist[o]) return mon_tile[o];
    }
    o = item[x][y];
    if (o < 0 || o >= (int)(sizeof obj_tile / sizeof obj_tile[0]) || c != objnamelist[o]) return -1;
    if (obj_tile[o] == -2)
        return wall_tile[(is_wall(x, y - 1) ? 1 : 0) | (is_wall(x + 1, y) ? 2 : 0) | (is_wall(x, y + 1) ? 4 : 0) |
                         (is_wall(x - 1, y) ? 8 : 0)];
    return obj_tile[o];
}

int wc_objtile(int o)
{
    return o > 0 && o < (int)(sizeof obj_tile / sizeof obj_tile[0]) && obj_tile[o] >= 0 ? obj_tile[o] : -1;
}

int wc_montile(int m)
{
    return m > 0 && m < (int)(sizeof mon_tile / sizeof mon_tile[0]) ? mon_tile[m] : -1;
}

/* ---- Status pane ---- */
static int ln;

static const char *trim(const char *s)
{
    while (*s == ' ') s++;
    return s;
}

static void line(WINDOW *w, chtype attr, const char *s)
{
    if (ln >= w->maxy) return;
    wmove(w, ln++, 0);
    w->attr = attr;
    while (*s && w->curx < w->maxx - 1) waddch(w, (unsigned char)*s++);
    w->attr = 0;
    wclrtoeol(w);
}

void wc_status(WINDOW *w)
{
    char b[128];
    const char *e;
    int i, x = 0;
    ln = 0;
    snprintf(b, sizeof b, "%s the %s", logname, char_class);
    line(w, A_BOLD, b);
    snprintf(b, sizeof b, "Level %ld %s", c[LEVEL], c[LEVEL] > 0 ? trim(class[c[LEVEL] - 1]) : "");
    line(w, 0, b);
    snprintf(b, sizeof b, "Exp %ld", c[EXPERIENCE]);
    line(w, 0, b);
    snprintf(b, sizeof b, "HP %ld(%ld)   Spells %ld(%ld)", c[HP], c[HPMAX], c[SPELLS], c[SPELLMAX]);
    line(w, c[HP] * 4 < c[HPMAX] ? COLOR_PAIR(COLOR_RED) | A_BOLD : 0, b);
    snprintf(b, sizeof b, "AC %ld   WC %ld", c[AC], c[WCLASS]);
    line(w, 0, b);
    snprintf(b, sizeof b, "STR %ld  INT %ld  WIS %ld", c[STRENGTH] + c[STREXTRA], c[INTELLIGENCE], c[WISDOM]);
    line(w, 0, b);
    snprintf(b, sizeof b, "CON %ld  DEX %ld  CHA %ld", c[CONSTITUTION], c[DEXTERITY], c[CHARISMA]);
    line(w, 0, b);
    snprintf(b, sizeof b, "Gold %ld", c[GOLD]);
    line(w, COLOR_PAIR(COLOR_YELLOW), b);
    snprintf(b, sizeof b, "Dungeon: %s", c[TELEFLAG] ? "?" : trim(levelname[(int)level]));
    line(w, 0, b);
    snprintf(b, sizeof b, "Time: %ld mobuls left", (TIMELIMIT - gtime) / 100);
    line(w, 0, b);
    line(w, 0, "");
    /* active effects (the column right of the map), as many per line as fit */
    b[0] = 0;
    for (i = 0; (e = bot_effect(i)); i++) {
        if (!*e) continue;
        if (x + (int)strlen(e) + 2 > w->maxx - 1) {
            line(w, COLOR_PAIR(COLOR_CYAN), b);
            b[0] = 0;
            x = 0;
        }
        x += snprintf(b + x, sizeof b - x, "%s%s", x ? ", " : "", e);
    }
    if (x) line(w, COLOR_PAIR(COLOR_CYAN), b);
    while (ln < w->maxy) line(w, 0, "");
}

/* ---- Inventory pane ---- */

/* Same text as show3()/show1() (show.c) */
void item_name(char *b, size_t n, int i) /* also rvip.c */
{
    int o = iven[i], a = ivenarg[i];
    int k = snprintf(b, n, "%c) %s", 'a' + i, objectname[o]);
    if (o == OPOTION && *potionname[a] && potionknown[a])
        k += snprintf(b + k, n - k, " of%s", potionname[a]);
    else if (o == OSCROLL && *scrollname[a] && scrollknown[a])
        k += snprintf(b + k, n - k, " of%s", scrollname[a]);
    else if (o != OPOTION && o != OSCROLL && o != OBOOK && o != OCHEST && o != OCOOKIE && o != OLARNEYE &&
             o != OSPIRITSCARAB && o != OCUBEofUNDEAD && o != ODIAMOND && o != ORUBY && o != OEMERALD &&
             o != OSAPPHIRE && o != OORB && o != OHANDofFEAR && o != OBRASSLAMP && o != OURN && o != OWWAND &&
             o != OSPHTALISMAN && o != ONOTHEFT) {
        if (a > 0 || wizard) k += snprintf(b + k, n - k, " +%d", a);
        else if (a < 0) k += snprintf(b + k, n - k, " %d", a);
    }
    if (c[WIELD] == i) k += snprintf(b + k, n - k, " (in hand)");
    if (c[WEAR] == i || c[SHIELD] == i) snprintf(b + k, n - k, " (worn)");
}

/* Angband's colour for an object id (RVIP W0: Ularn has no colours) */
const char *wc_css(int o)
{
    switch (o) {
    case OPOTION: return "#40a0ff";
    case OSCROLL: return "#ffffff";
    case OBOOK: return "#60e0e0";
    case OAMULET: case OORBOFDRAGON: case OSPIRITSCARAB: case OCUBEofUNDEAD: case ONOTHEFT: case OSPHTALISMAN:
    case OHANDofFEAR: case OORB: return "#ff9000";
    case ORING: case OSTUDLEATHER: case OSPLINT: case OPLATEARMOR: case OSSPLATE: case OSHIELD: case OELVENCHAIN:
    case OPLATE: case OCHAIN: case OLEATHER: return "#a07040";
    case ORINGOFEXTRA: case OREGENRING: case OPROTRING: case OENERGYRING: case ODEXRING: case OSTRRING:
    case OCLEVERRING: case ODAMRING: case OBELT: return "#ff4040";
    case OHAMMER: case OSWORD: case O2SWORD: case OSWORDofSLASHING: case OSPEAR: case ODAGGER: case OBATTLEAXE:
    case OLONGSWORD: case OFLAIL: case OLANCE: case OVORPAL: case OSLAYER: return "#b0b0b8";
    case OWWAND: return "#40d040";
    case OPSTAFF: case OCOOKIE: return "#d09050";
    case OBRASSLAMP: case OURN: return "#ffff90";
    case OSPEED: case OACID: case OHASH: case OSHROOMS: case OCOKE: return "#c080ff";
    case OGOLDPILE: case OMAXGOLD: case OKGOLD: case ODGOLD: return "#ffe040";
    case ODIAMOND: case ORUBY: case OEMERALD: case OSAPPHIRE: case OLARNEYE: return "#ff60ff";
    }
    return "";
}

/* Map colours (Ularn has none): monsters red, the rest by object class,
 * items as their inventory colour. Only cells that show what the game put
 * there are coloured, so text over the map stays plain. */
chtype wc_mapcell(int y, int x, chtype ch)
{
    static const struct { const char *css; int col; } inv[] = {
        { "#40a0ff", COLOR_BLUE }, { "#ffffff", COLOR_WHITE }, { "#60e0e0", COLOR_CYAN },
        { "#ff9000", COLOR_YELLOW }, { "#a07040", COLOR_YELLOW }, { "#ff4040", COLOR_RED },
        { "#b0b0b8", COLOR_WHITE }, { "#40d040", COLOR_GREEN }, { "#d09050", COLOR_YELLOW },
        { "#ffff90", COLOR_YELLOW }, { "#c080ff", COLOR_MAGENTA }, { "#ffe040", COLOR_YELLOW },
        { "#ff60ff", COLOR_MAGENTA },
    };
    int k = (int)(ch & A_CHARTEXT), m, o, col = 0, i;
    const char *css;
    if ((ch & 0x800) || y >= MAXY || x >= MAXX || k <= ' ') return ch;
    if ((m = mitem[x][y].mon) && k == monstnamelist[m]) return ch | COLOR_PAIR(COLOR_RED);
    o = item[x][y];
    if (!know[x][y] || k != objnamelist[o]) return ch;
    switch (o) {
    case OSTAIRSUP: case OSTAIRSDOWN: case OENTRANCE: case OVOLDOWN: case OVOLUP:
    case OELEVATORUP: case OELEVATORDOWN: return ch | COLOR_PAIR(COLOR_YELLOW) | A_BOLD;
    case OOPENDOOR: case OCLOSEDDOOR: case OCHEST: col = COLOR_YELLOW; break;
    case OALTAR: case OTHRONE: case ODEADTHRONE: case OFOUNTAIN: case ODEADFOUNTAIN: case OSTATUE:
    case OMIRROR: case OTELEPORTER: col = COLOR_CYAN; break;
    case ODNDSTORE: case OSCHOOL: case OBANK: case OBANK2: case OHOME: case OTRADEPOST: case OLRS:
    case OPAD: col = COLOR_GREEN; break;
    case OPIT: case OTRAPARROW: case ODARTRAP: case OTRAPDOOR: col = COLOR_RED; break;
    case OANNIHILATION: col = COLOR_MAGENTA; break;
    default:
        css = wc_css(o);
        for (i = 0; *css && i < (int)(sizeof inv / sizeof inv[0]); i++)
            if (!strcmp(css, inv[i].css)) col = inv[i].col;
    }
    return col ? ch | COLOR_PAIR(col) : ch;
}

/* Rows "a)   name" with a tile set (the frontend puts the icon on cols 2-4),
 * "a) ! name" in text mode (the item's own symbol) */
void wc_inv(WINDOW *w)
{
    char b[160], n[160];
    int i, icons = be_icons();
    ln = 0;
    line(w, A_BOLD, "Inventory");
    for (i = 0; i < IVENSIZE; i++) {
        if (!iven[i]) continue;
        int t = wc_objtile(iven[i]);
        item_name(n, sizeof n, i);
        if (icons && t >= 0) snprintf(b, sizeof b, "%.3s  %s", n, n + 3);
        else snprintf(b, sizeof b, "%.3s%c %s", n, objnamelist[iven[i]], n + 3);
        line(w, c[WIELD] == i || c[WEAR] == i || c[SHIELD] == i ? A_BOLD : 0, b);
        be_invfg(ln - 1, wc_css(iven[i]), icons ? t : -1);
    }
    for (i = ln; i < w->maxy; i++) be_invfg(i, "", -1);
    while (ln < w->maxy) line(w, 0, "");
}
