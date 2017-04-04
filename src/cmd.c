#include "config.h"
#ifdef HAVE_NCURSESW_NCURSES_H
# include <ncursesw/ncurses.h>
#else
# include <ncurses.h>
#endif
#include <stdio.h>
#include <string.h>

typedef struct {
        const char* keys; // optional
        const char* desc; // mandatory until last entry
} cmd;

/* Printed Nethack-guidebook style in --help, 
 * vim-cheatsheet style in help screen. */

#define KB_SZ 3
// Try a help screen sort of like the vim cheatsheets, for better spacial
// awareness of key layout.
void cmd_render_help(cmd cs[]) {
        const char* const wasd_kb[KB_SZ] = {
                "qQ wW eE rR tT yY uU iI oO pP [{ ]} \\|",
                " aA sS dD fF gG hH jJ kK lL ;: '\"",
                "  zZ xX cC vV bB nN mM ,< .> /?",
        };

        erase();
        attr_set(A_NORMAL, -1, NULL);
        
        for (int y=0; y<KB_SZ; y++) mvprintw(y, 0, wasd_kb[y]);

        unsigned pal = 2; // ghetto-palette in basic 16 colors
        for (int i=0, y=KB_SZ+1; cs[i].desc; i++, y++) {
                if (!cs[i].keys) {
                        // group entry, do not advance palette
                        y++; 
                        mvprintw(y, 4, "%s:", cs[i].desc);
                } else {
                        for (const char* c=cs[i].keys; *c; c++) {
                                for (int y=0; y<KB_SZ; y++) {
                                        for (int x=0; wasd_kb[y][x]; x++) {
                                                if (wasd_kb[y][x] == *c) {
                                                        mvchgat(y, x, 1, A_NORMAL, pal, 0);
                                                }
                                        }
                                }
                        }
                        mvaddstr(y, 0, cs[i].keys);
                        mvaddstr(y, 8, cs[i].desc);
                        mvchgat(y, 8, strlen(cs[i].desc), A_NORMAL, pal, 0);
                        pal++;
                }
        }
        
        refresh();
        getch();
}

void cmd_print_help(cmd cs[]) {
        for (int i=0; cs[i].desc; i++) {
                if (!cs[i].keys) {
                        printf("\n    %s:\n", cs[i].desc);
                } else { 
                        printf("%s\t%s\n", cs[i].keys, cs[i].desc);
                }
        }
}
