/* Native test frontend (ASan runs): keys come from stdin, panes are not
 * drawn (set ULARN_DUMP=<file> to get them as text on every refresh).
 * End of input ends the game like a hang-up. */
#include <stdio.h>
#include <stdlib.h>
#include "curses.h"

void be_init(int p, int cols, int rows) { (void)p; (void)cols; (void)rows; }
void be_put(int p, int y, int x, chtype ch, int t) { (void)p; (void)y; (void)x; (void)ch; (void)t; }
void be_cursor(int p, int y, int x) { (void)p; (void)y; (void)x; }
void be_prompt(const char *s) { (void)s; }
void be_popup(int r, int c) { (void)r; (void)c; }
void be_flush(void) {}
/* text rows: not drawn (ULARN_DUMP reads the pane cells, attributes included) */
void be_line(int p, int y, const char *s, const char *c, int t) { (void)p; (void)y; (void)s; (void)c; (void)t; }
void be_rows(int p, int n) { (void)p; (void)n; }
int be_icons(void) { return 0; }
void be_sleep(int ms) { (void)ms; }
void be_sound(const char *e) { (void)e; }
void be_beacon(const char *ev, const char *name, const char *killer, int depth, int score, int turns, int lvl)
{
    fprintf(stderr, "[beacon g=ularn&ev=%s&name=%s&killer=%s&depth=%d&score=%d&turns=%d&lvl=%d]\n",
            ev, name, killer ? killer : "", depth, score, turns, lvl);
}
void be_end(int saved) { fprintf(stderr, "[be_end saved=%d]\n", saved); }

int be_poll(void) { return -1; } /* scripted keys: explore runs until it stops */

int be_getkey(int at_cmd)
{
    int k = getchar();
    (void)at_cmd;
    if (k == EOF) { fprintf(stderr, "[end of keys]\n"); exit(0); }
    return k;
}
