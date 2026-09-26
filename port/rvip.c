/* RVIP additions for Ularn (from ~/Games/larn/port/rvip.c), called from
 * parse() (main.c):
 *   ~        auto-explore: one step per turn over what the player knows
 *   < >      off the stairs: walk to the nearest known one, take it there;
 *            on stairs (or a shaft, elevator, the entrance): take it.
 *   Enter    floating menu of every command (Uhelp page 2)
 *   i        inventory with a cursor and item menus; actions run as the
 *            keys the player would type (wc_push("qa")).
 *   item prompts (whatitem(), qwhatitem()) show the fitting items with a cursor
 * Ularn has no stair commands: stepping on stairs asks "(d) go down?", and
 * wc_answer() replies to that prompt. */
#include <stdio.h>
#include <string.h>
#include "../src/header.h"
#include "../src/player.h"
#include "../src/itm.h"
#include "../src/monst.h"
#include "../src/extern.h"
#include "curses.h"

void item_name(char *b, size_t n, int i); /* panes.c */
static int reopen; /* 2: an item action is queued, 1: reopen the inventory now */
static int monster_in_view(void);

static char auto_mode;     /* '~' explore, '<' / '>' walk to stairs */
static int auto_level = -1, auto_msgs, auto_x = -1, auto_y, auto_hp, door_step;
static unsigned char visited[MAXLEVEL + MAXVLEVEL][MAXX][MAXY];

/* moveplayer() directions 1-8 (display.c) and their keys in parse() */
#define dx diroffx
#define dy diroffy
static const char dkey[9] = { 0, 'j', 'l', 'k', 'h', 'u', 'y', 'n', 'b' };

/* things you walk onto without an effect (unknown traps look like floor) */
static int plain(int o)
{
    return o == 0 || o == OOPENDOOR || o == OIVTELETRAP || o == OIVDARTRAP || o == OIVTRAPDOOR || o == OTRAPARROWIV;
}

/* things lying around that explore visits (pick-ups, not fixtures):
 * everything wc_css() colours is an item */
static int loot(int o) { return o == OCHEST || *wc_css(o); }

/* where < / > walk to: stairs, the entrance, level 1's exit. Never a
 * shaft, elevator or trapdoor (they skip levels): those work only when
 * you stand on them. */
static int stairs_at(char mode, int x, int y)
{
    int o = item[x][y];
    if (!know[x][y]) return 0;
    if (mode == '<') return o == OSTAIRSUP || (level == 1 && x == 33 && y == MAXY - 1);
    return o == OSTAIRSDOWN || (level == 0 && o == OENTRANCE);
}

static int passable(char mode, int x, int y)
{
    int o = item[x][y];
    if (!know[x][y]) return 0;
    if (plain(o) || loot(o) || o == OCLOSEDDOOR) return 1;
    return mode != '~' && stairs_at(mode, x, y);
}

static int target(char mode, int x, int y)
{
    int i, j;
    if (mode != '~') return stairs_at(mode, x, y);
    if (visited[(int)level][x][y] || !passable(mode, x, y)) return 0;
    if (loot(item[x][y])) return 1;
    for (i = -1; i <= 1; i++)
        for (j = -1; j <= 1; j++)
            if (x + i >= 0 && y + j >= 0 && x + i < MAXX && y + j < MAXY && !know[x + i][y + j]) return 1;
    return 0;
}

/* a monster the player can see: the screen shows its letter nearby.
 * ponytail: radius 5; Ularn leaves sleeping monsters on the screen after
 * you walk away, and those must not stop explore across the map. */
#define VIEW 5
static int monster_in_view(void)
{
    int x, y, m;
    for (y = playery - VIEW; y <= playery + VIEW; y++)
        for (x = playerx - VIEW; x <= playerx + VIEW; x++)
            if (x >= 0 && y >= 0 && x < MAXX && y < MAXY)
            if ((m = mitem[x][y].mon) && (int)(stdscr->c[y * stdscr->maxx + x] & A_CHARTEXT) == monstnamelist[m])
                return 1;
    return 0;
}

/* direction (1-8) of the first step to the nearest target, 0 = none */
static int first_step(char mode)
{
    static short from[MAXX][MAXY], queue[MAXX * MAXY];
    static unsigned char done[MAXX][MAXY];
    int head = 0, tail = 0, d;
    memset(done, 0, sizeof done);
    queue[tail++] = (short)(playerx * MAXY + playery);
    done[(int)playerx][(int)playery] = 1;
    while (head < tail) {
        int x = queue[head] / MAXY, y = queue[head] % MAXY;
        head++;
        if ((x != playerx || y != playery) && target(mode, x, y)) {
            while (from[x][y] != playerx * MAXY + playery) {
                int p = from[x][y];
                x = p / MAXY;
                y = p % MAXY;
            }
            for (d = 1; d <= 8; d++)
                if (playerx + dx[d] == x && playery + dy[d] == y) return d;
            return 0;
        }
        for (d = 1; d <= 8; d++) {
            int nx = x + dx[d], ny = y + dy[d];
            if (nx < 0 || ny < 0 || nx >= MAXX || ny >= MAXY || done[nx][ny]) continue;
            if (!passable(mode, nx, ny) && !target(mode, nx, ny)) continue;
            done[nx][ny] = 1;
            from[nx][ny] = (short)(x * MAXY + y);
            queue[tail++] = (short)(nx * MAXY + ny);
        }
    }
    return 0;
}

static int stop(const char *why)
{
    auto_mode = 0;
    if (why) {
        cursors();
        lprcat(why);
    }
    return 0;
}

/* One step: a command key for parse(), 0 when stopped. */
static int auto_step(void)
{
    char mode = auto_mode;
    int d, x, y;

    wc_answer(0);
    visited[(int)level][(int)playerx][(int)playery] = 1;
    if (level != auto_level) return stop(NULL); /* new level */
    /* "You find a closed door ... open." is expected; damage is not */
    if (door_step && c[HP] >= auto_hp) auto_msgs = wc_msgs;
    door_step = 0;
    if (wc_msgs != auto_msgs) return stop(NULL); /* something happened */
    if (c[BLINDCOUNT] || c[CONFUSE]) return stop("\nYou are in no state to explore.");
    if (monster_in_view()) return stop("\nNot with a monster in view.");
    if (playerx == auto_x && playery == auto_y) return stop(NULL); /* the last step didn't move */
    d = first_step(mode);
    if (!d)
        return stop(mode == '~' ? "\nNothing left to explore." : mode == '<' ? "\nYou know of no way up." :
                                                                                "\nYou know of no way down.");
    x = playerx + dx[d];
    y = playery + dy[d];
    auto_msgs = wc_msgs;
    auto_hp = c[HP];
    auto_x = playerx;
    auto_y = playery;
    if (item[x][y] == OCLOSEDDOOR) { /* the door asks "(o) try to open it?" */
        wc_answer('o');
        door_step = 1;
        auto_x = -1; /* a stuck door stays shut: try again */
    } else if (mode != '~' && stairs_at(mode, x, y)) {
        /* level 1's exit puts you on the town's entrance: stay out */
        wc_answer(item[x][y] == OENTRANCE ? 'g' : level == 1 && mode == '<' ? 'i' : mode == '<' ? 'u' : 'd');
        auto_mode = 0; /* arrived: the answered prompt takes the stairs */
    }
    return dkey[d];
}

static int start(char mode)
{
    auto_mode = mode;
    auto_level = level;
    auto_msgs = wc_msgs;
    auto_x = -1;
    door_step = 0;
    return auto_step();
}

/* ---------------- Enter menu and inventory (from Larn's rvip.c) ---------------- */

#define ESC 27
#define UP (0x100 | 'k') /* cursor keys (ularn.js): arrows, keypad */
#define DOWN (0x100 | 'j')
#define LEFT (0x100 | 'h')
#define RIGHT (0x100 | 'l')
#define PAD5 (0x100 | '.')
#define PGUP (0x100 | 'u')
#define PGDN (0x100 | 'n')
#define LIST_ROWS 16 /* rows under the title in the map area (17 rows) */

struct entry {
    int key;
    char text[48];
};
static struct entry cmds[64];
static int ncmds;

static void add_cmd(int k, const char *t)
{
    if (ncmds >= (int)(sizeof cmds / sizeof cmds[0])) return;
    cmds[ncmds].key = k;
    snprintf(cmds[ncmds++].text, sizeof cmds[0].text, "%s", t);
}

/* Uhelp page 2: the lines after its title up to the first blank one, three
 * columns (0, 27, 56 after tab expansion) of "k  text". Listed column by
 * column, the help's own grouping (moves, runs/info, actions). */
static void load_cmds(void)
{
    static const int col[4] = { 0, 27, 56, 80 };
    static char cell[3][20][48];
    int cn[3] = { 0, 0, 0 }, on = 0, i, j;
    char raw[256], line[256];
    FILE *f = fopen(helpfile, "r");
    if (!f) return;
    while (fgets(raw, sizeof raw, f)) {
        char *r;
        int x = 0;
        for (r = raw; *r && *r != '\n' && x < 250; r++)
            if (*r == '\t') do line[x++] = ' '; while (x % 8);
            else line[x++] = *r;
        line[x] = 0;
        if (!on) { on = strstr(line, "Help File for") != NULL; continue; }
        if (!*line) { if (cn[0]) break; continue; }
        for (i = 0; i < 3; i++) {
            int a = col[i], b = col[i + 1];
            if (a >= x || cn[i] >= 20 || (a && line[a - 1] != ' ')) continue; /* a long entry runs on */
            if (b > x) b = x;
            memcpy(cell[i][cn[i]], line + a, b - a);
            cell[i][cn[i]][b - a] = 0;
            for (j = b - a; j > 0 && cell[i][cn[i]][j - 1] == ' ';) cell[i][cn[i]][--j] = 0;
            if (*cell[i][cn[i]]) cn[i]++;
        }
    }
    fclose(f);
    ncmds = 0;
    for (i = 0; i < 3; i++)
        for (j = 0; j < cn[i]; j++) {
            char *e = cell[i][j], *t = e + 1;
            if (!strncmp(e, "< >", 3)) {
                add_cmd('<', "walk to the nearest known stairs up");
                add_cmd('>', "walk to the nearest known stairs down");
                continue;
            }
            if (*t != ' ') continue;
            while (*t == ' ') t++;
            add_cmd((unsigned char)*e, t);
        }
}

/* Draws rows[] over the map (top left) under an optional title, row `cur`
 * highlighted, scrolled so it shows. Returns the first row shown. */
static int draw_list(const char *title, char rows[][64], int n, int cur, int top)
{
    int i, x, w = title ? (int)strlen(title) : 0, shown = n < LIST_ROWS ? n : LIST_ROWS, y = 0;
    for (i = 0; i < n; i++)
        if ((int)strlen(rows[i]) > w) w = (int)strlen(rows[i]);
    if (cur < top) top = cur;
    if (cur >= top + shown) top = cur - shown + 1;
    for (i = 0; i < MAXY; i++) /* the map area only: draws() restores it */
        for (wmove(stdscr, i, 0), x = 0; x < MAXX; x++) waddch(stdscr, ' ');
    if (title) {
        wmove(stdscr, y++, 0);
        for (x = 0; title[x]; x++) waddch(stdscr, (unsigned char)title[x] | A_BOLD);
    }
    for (i = top; i < top + shown; i++) {
        chtype a = i == cur ? A_STANDOUT : 0;
        wmove(stdscr, y++, 0);
        for (x = 0; x < w; x++) waddch(stdscr, (rows[i][x] && x < (int)strlen(rows[i]) ? (unsigned char)rows[i][x] : ' ') | a);
    }
    wmove(stdscr, (title ? 1 : 0) + cur - top, 0);
    return top;
}

static void open_list(void)
{
    lflush();
    wc_overlay();
}

static void close_list(void)
{
    draws(0, MAXX, 0, MAXY);
    wc_dungeon();
}

static int getkey(void)
{
    int k;
    wc_raw = 1;
    k = wc_getch(0);
    wc_raw = 0;
    return k;
}

static int move_cur(int k, int cur, int n)
{
    if (k == UP) return (cur + n - 1) % n;
    if (k == DOWN) return (cur + 1) % n;
    if (k == PGUP) return cur > LIST_ROWS ? cur - LIST_ROWS : 0;
    if (k == PGDN) return cur + LIST_ROWS < n ? cur + LIST_ROWS : n - 1;
    return -1;
}

/* Enter: the command menu. Returns the chosen key or 0. */
int cmd_menu(void)
{
    static char rows[64][64];
    int i, cur = 0, top = 0, k, c2, res = 0;
    if (!ncmds) load_cmds();
    if (!ncmds) return 0;
    for (i = 0; i < ncmds; i++) snprintf(rows[i], sizeof rows[i], " %c  %s ", cmds[i].key, cmds[i].text);
    open_list();
    for (;;) {
        top = draw_list("Commands (key or cursor + Enter, Esc closes)", rows, ncmds, cur, top);
        k = getkey();
        if (k == ESC || k == '0' || k == LEFT) break;
        if ((c2 = move_cur(k, cur, ncmds)) >= 0) { cur = c2; continue; }
        if (k == '\n' || k == '\r' || k == PAD5 || k == RIGHT || k == ' ') { res = cmds[cur].key; break; }
        for (i = 0; i < ncmds && cmds[i].key != k; i++) ;
        if (i < ncmds) { res = k; break; }
    }
    close_list();
    return res;
}

/* item classes as whatitem() (action.c) sorts them */
static int is_weapon(int o)
{
    switch (o) {
    case OSWORDofSLASHING: case OHAMMER: case OSWORD: case O2SWORD: case OSPEAR: case ODAGGER:
    case OBATTLEAXE: case OLONGSWORD: case OFLAIL: case OSLAYER: case OLANCE: case OVORPAL:
        return 1;
    }
    return 0;
}

static int is_armor(int o)
{
    switch (o) {
    case OPLATE: case OCHAIN: case OLEATHER: case ORING: case OSTUDLEATHER: case OSPLINT:
    case OPLATEARMOR: case OSSPLATE: case OSHIELD: case OELVENCHAIN:
        return 1;
    }
    return 0;
}

struct action {
    int key;
    const char *name;
};

/* actions for inventory slot i, main one first */
static int actions(int i, struct action *a)
{
    int o = iven[i], n = 0;
    if (o == OPOTION) a[n++] = (struct action){ 'q', "Quaff" };
    else if (o == OSCROLL || o == OBOOK) a[n++] = (struct action){ 'r', "Read" };
    else if (o == OCOOKIE) a[n++] = (struct action){ 'e', "Eat" };
    else if (is_weapon(o)) a[n++] = c[WIELD] == i ? (struct action){ 'w', "Put away (wield nothing)" } : (struct action){ 'w', "Wield" };
    else if (is_armor(o)) a[n++] = c[WEAR] == i || c[SHIELD] == i ? (struct action){ 'T', "Take off" } : (struct action){ 'W', "Wear" };
    a[n++] = (struct action){ 'd', "Drop" };
    a[n++] = (struct action){ '*', "Examine" };
    return n;
}

/* queue the keys the player would type: verb + slot letter ('-' puts
 * the weapon away, T takes off without asking) */
static void run_action(int key, int i)
{
    char k[3] = { (char)key, 0, 0 };
    if (key == '*') {
        char b[128];
        item_name(b, sizeof b, i);
        cursors();
        lprintf("\n%s", b + 3);
        return;
    }
    if (key == 'w' && c[WIELD] == i) k[1] = '-';
    else if (key != 'T') k[1] = (char)('a' + i);
    wc_push(k);
}

static int item_rows(char rows[][64], int *slot)
{
    int i, n = 0;
    for (i = 0; i < IVENSIZE; i++)
        if (iven[i]) {
            item_name(rows[n], 60, i);
            slot[n++] = i;
        }
    return n;
}

/* the action menu for slot i: the chosen key or 0 */
static int item_menu(int i)
{
    struct action a[4];
    char rows[4][64], title[64];
    int n = actions(i, a), cur = 0, k, j, c2;
    item_name(title, sizeof title, i);
    for (j = 0; j < n; j++) snprintf(rows[j], sizeof rows[j], " %c  %s ", a[j].key, a[j].name);
    for (;;) {
        draw_list(title + 3, rows, n, cur, 0);
        k = getkey();
        if ((c2 = move_cur(k, cur, n)) >= 0) cur = c2;
        else if (k == PAD5 || k == RIGHT || k == '\r' || k == '\n' || k == ' ') return a[cur].key;
        else if (k == ESC || k == LEFT || k == '0' || k == '.') return 0;
        else
            for (j = 0; j < n; j++)
                if (a[j].key == k) return k;
    }
}

/* 'i': the inventory with a cursor. Letter = main action, Shift+letter
 * drops, Ctrl+letter examines, Enter = menu. Any other key is a command. */
static int inventory_browse(void)
{
    static char rows[IVENSIZE][64];
    static int cur;
    int slot[IVENSIZE], n, k, i, top = 0, key = 0, c2;
    struct action a[4];
    reopen = 0;
    open_list();
    for (;;) {
        n = item_rows(rows, slot);
        if (!n) {
            close_list();
            cursors();
            lprcat("\nYou aren't carrying anything.");
            return 0;
        }
        if (cur >= n) cur = n - 1;
        top = draw_list("Inventory: letter uses, Shift drops, Ctrl examines, Enter menu", rows, n, cur, top);
        k = getkey();
        if (k == ESC || k == '0' || k == '.' || k == 'i' || k == LEFT) break;
        if ((c2 = move_cur(k, cur, n)) >= 0) { cur = c2; continue; }
        i = slot[cur];
        key = 0;
        if (k == '\r' || k == '\n' || k == PAD5 || k == RIGHT || k == ' ') key = item_menu(i);
        else if (k == '+') key = (actions(i, a), a[0].key);
        else if (k == '-') key = 'd';
        else if (k == '*') key = '*';
        else if ((k >= 'a' && k <= 'z') || (k >= 'A' && k <= 'Z') || (k >= 1 && k <= 26)) {
            int letter = k >= 'a' ? k - 'a' : k >= 'A' ? k - 'A' : k - 1;
            for (i = 0; i < n && slot[i] != letter; i++) ;
            if (i == n) break; /* not an item: a normal command */
            cur = i;
            i = slot[cur];
            key = k >= 'a' ? (actions(i, a), a[0].key) : k >= 'A' ? 'd' : '*';
        } else
            break;
        if (!key) continue; /* menu closed: back to the list */
        close_list();
        run_action(key, i);
        if (key != '*') reopen = 2; /* after the queued command */
        return 0;
    }
    close_list();
    if (k < 256 && k != ESC && k != '0' && k != '.' && k != 'i') {
        char s[2] = { (char)k, 0 };
        wc_push(s); /* any other key is a normal command */
    }
    return 0;
}

/* Item prompts: the items that fit the verb with a cursor. Returns what
 * whatitem() returns (a letter, '-', '.', '*' or ESC), or -1 when keys are
 * queued (an item action answers the game's own prompt). */
int rvip_whatitem(const char *verb)
{
    static char rows[IVENSIZE][64];
    char title[96];
    int slot[IVENSIZE], all[IVENSIZE], n = 0, i, k, c2, cur = 0, top = 0, key = ESC;
    int wld = !strcmp(verb, "wield"), drop = !strcmp(verb, "drop");
    if (wc_queued()) return -1;
    n = item_rows(rows, all);
    for (i = k = 0; i < n; i++) {
        int o = iven[all[i]];
        if (drop || (wld && is_weapon(o)) || (!strcmp(verb, "wear") && is_armor(o)) ||
            (!strcmp(verb, "quaff") && o == OPOTION) || (!strcmp(verb, "eat") && o == OCOOKIE) ||
            (!strcmp(verb, "read") && (o == OSCROLL || o == OBOOK))) {
            if (k != i) memcpy(rows[k], rows[i], sizeof rows[0]);
            slot[k++] = all[i];
        }
    }
    n = k;
    snprintf(title, sizeof title, n ? "%s which? (letter or Enter, %sEsc)" : "Nothing to %s. (%sEsc)",
             verb, wld ? "- none, " : drop ? ". gold, " : "");
    if (n) title[0] = (char)(title[0] - 'a' + 'A');
    open_list();
    for (;;) {
        top = draw_list(title, rows, n, cur, top);
        k = getkey();
        if (n && (c2 = move_cur(k, cur, n)) >= 0) cur = c2;
        else if ((k == '\r' || k == '\n' || k == PAD5 || k == RIGHT) && n) { key = 'a' + slot[cur]; break; }
        else if (k == ESC || k == '0' || k == LEFT || k == ' ') break;
        else if ((k >= 'a' && k <= 'z') || k == '*' || (k == '-' && wld) || (k == '.' && drop)) { key = k; break; }
    }
    close_list();
    cursors();
    if (key == ESC) lprcat("\nAborted.");
    return key;
}

/* The key parse() runs next, 0 = ask the player (yylex()). A key pressed
 * while walking stops the walk and is read as a command. */
int rvip_auto(void)
{
    if (reopen == 1) {
        reopen = 0;
        if (!monster_in_view()) inventory_browse();
    }
    if (!auto_mode) return 0;
    bottomdo(); /* what yylex() shows before a command */
    showplayer();
    if (wc_kbhit()) return stop(NULL);
    return auto_step();
}

/* The player's key: returns the key for parse(), 0 = nothing to run,
 * -1 = the turn was used here. */
int rvip_command(int k)
{
    int o = item[(int)playerx][(int)playery];
    auto_mode = 0;
    reopen = reopen == 2; /* the queued command itself keeps it */
    if (k == '\n' || k == '\r') k = cmd_menu();
    if (k == 'i') return inventory_browse();
    if (k == '~') return start('~');
    if (k != '<' && k != '>') return k;
    /* stood on: take it (shortcuts only this way) */
    if (k == '>' && (o == OSTAIRSDOWN || o == OVOLDOWN || o == OELEVATORDOWN || o == OENTRANCE))
        wc_answer(o == OSTAIRSDOWN ? 'd' : o == OVOLDOWN ? 'c' : 'g');
    else if (k == '<' && (o == OSTAIRSUP || o == OVOLUP || o == OELEVATORUP))
        wc_answer(o == OSTAIRSUP ? 'u' : 'c');
    else
        return start((char)k);
    lookforobject(); /* the stairs' own prompt, answered */
    return -1;
}
