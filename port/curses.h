/* Minimal in-memory curses for Ularn (RVIP port). Ularn itself writes
 * termcap codes into its output buffer; io.c's lflush() hands that buffer
 * to wc_write(), which draws into this 80x24 stdscr. The shim routes the
 * screen to panes (map, messages, status, inventory, pop-up) for the
 * frontend (be_web.c; be_tty.c for native tests). Adapted from
 * ~/Games/larn/port. */
#ifndef ULARN_WCURSES_H
#define ULARN_WCURSES_H
#include <stdio.h>

typedef unsigned int chtype;
#define ERR (-1)
#define OK 0

/* char in bits 0-7, colour (0-7) in 8-10, colour set flag 11, attributes */
#define A_CHARTEXT 0xffu
#define COLOR_PAIR(n) ((chtype)(((n) & 7) | 8) << 8)
#define A_STANDOUT 0x1000u
#define A_BOLD 0x2000u
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_YELLOW 3
#define COLOR_BLUE 4
#define COLOR_MAGENTA 5
#define COLOR_CYAN 6
#define COLOR_WHITE 7

typedef struct {
    int maxy, maxx, cury, curx;
    chtype attr;
    short *first, *last; /* changed range per line, -1 = none */
    chtype *c;
} WINDOW;

extern WINDOW *stdscr;

WINDOW *initscr(void);
WINDOW *newwin(int, int, int, int);
int wmove(WINDOW *, int, int);
int waddch(WINDOW *, chtype);
int wclrtoeol(WINDOW *);
int wrefresh(WINDOW *);

/* Hooks the game calls (io.c, display.c, show.c, tok.c, scores.c) */
void wc_write(const char *buf, int n); /* lflush(): the terminal output */
int wc_getch(int at_cmd);   /* a key; at_cmd: the command prompt (yylex) */
void wc_nap(int ms);        /* nap()/sleep(): show the screen, wait */
void wc_dungeon(void);      /* the map was drawn: back from text screens */
void wc_overlay(void);      /* text is about to be drawn over the map */
extern int wc_saved;
void wc_push(const char *keys);  /* rvip.c: keys read before the keyboard */
void wc_answer(int k);           /* rvip.c: reply to the next prompt (stairs, door) */
int wc_queued(void);             /* rvip.c: queued keys wait (item actions) */
int wc_kbhit(void);              /* rvip.c: a key is waiting */
extern int wc_msgs;             /* messages so far */
extern int wc_raw;              /* rvip.c: wc_getch() keeps 0x100|key for cursor keys */
chtype wc_mapcell(int y, int x, chtype ch); /* panes.c: map colours */        /* died(): the game ends because it was saved */

/* Frontend: panes. Text goes to text panes and a pop-up box sized to its
 * content (stage 4 tiles the map). */
enum { P_MAP, P_STATUS, P_MSG, P_INV, P_POP, NPANES };
void be_init(int pane, int cols, int rows);
void be_put(int pane, int y, int x, chtype ch, int tile);
void be_cursor(int pane, int y, int x);  /* pane -1: no cursor */
void be_prompt(const char *s);           /* live message row (rvip-wm.js prompt line) */
void be_popup(int rows, int cols);       /* 0: close */
void be_flush(void);
int be_getkey(int at_cmd);               /* waits for a key */
int be_poll(void);                       /* a key or -1, after a short wait */
void be_sleep(int ms);
void be_sound(const char *event);       /* web: play a sound event */
void be_end(int saved);                  /* the game is over (or saved) */
void be_invfg(int y, const char *css);   /* inventory row colour */
int tile_for(int y, int x, chtype ch);   /* panes.c: -1 = draw as text */
void wc_status(WINDOW *);                /* panes.c: Status pane */
void wc_inv(WINDOW *);                   /* panes.c: Inventory pane */
const char *wc_css(int obj);             /* panes.c: an object's colour */
#endif
