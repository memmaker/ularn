/* RVIP additions for Ularn (from ~/Games/larn/port/rvip.c), called from
 * parse() (main.c):
 *   ~        auto-explore: one step per turn over what the player knows
 *   < >      off the stairs: walk to the nearest known one, take it there;
 *            on stairs (or a shaft, elevator, the entrance): take it.
 * Ularn has no stair commands: stepping on stairs asks "(d) go down?", and
 * wc_answer() replies to that prompt. */
#include <string.h>
#include "../src/header.h"
#include "../src/player.h"
#include "../src/itm.h"
#include "../src/monst.h"
#include "../src/extern.h"
#include "curses.h"

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

/* The key parse() runs next, 0 = ask the player (yylex()). A key pressed
 * while walking stops the walk and is read as a command. */
int rvip_auto(void)
{
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
