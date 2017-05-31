#include "config.h"
#ifdef HAVE_NCURSESW_NCURSES_H
# include <ncursesw/ncurses.h>
#else
# include <ncurses.h>
#endif
#include "blindsight.h"
#include "render.h"

static const char log256[256] = {
#define LT(n) n,n,n,n, n,n,n,n, n,n,n,n, n,n,n,n
        -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
        LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
}; // Bit Twiddling Hacks, by Sean Eron Anderson
uint32_t dlog2(uint32_t v) { // working on 4+G files? use a GPU...
        register uint32_t t, tt;
        if ((tt = v >> 16)) {
                return (t = tt >> 8) ? 24 + log256[t] : 16 + log256[tt];
        } else {
                return (t = v >> 8) ? 8 + log256[t] : log256[v];
        }
}

const int default_ui_color = 6; // keep in basic colors, so it *always* works

/* Column width in bytes is a power of 2; rendering offsets into it
 * requires numbers lower than it. In many cases view.x is going to be
 * smaller than this, so we can't render an address for each column. In
 * such cases, we'll render an address every nth column, but the
 * addresses will not be truncated in any way. */
void render_col_addrs(vec2 base, const int dim_x, size_t bytes_per_row, view v) {
        unsigned int addr_sz = dlog2(bytes_per_row) / 8 + 1;
        addr_sz *= 2; // printed in hex
        addr_sz += 1; // add a space?
        unsigned int skip = addr_sz / v.codom.x; // # of columns addr shown
        if (addr_sz % v.codom.x) skip++; // ceil(^)

        char fmt[32];
        snprintf(fmt, sizeof(fmt), "%%0%dx ", addr_sz-1);

        size_t addr = 0;
        for (int x=base.x; x < base.x + dim_x; 
                        x+=v.codom.x * skip, addr+=v.dom * skip) {
                mvprintw(base.y, x, fmt, addr);
                mvchgat(base.y,  x, addr_sz-1, A_NORMAL, default_ui_color, NULL);
        }
}

void render_row_addrs(vec2 base, vec2 dim, const size_t buf_sz, const size_t cursor, view v, size_t bytes_per_row) {
        size_t dy = cursor;
        // Do not print addrs beyond buffer: v
        for (int y=base.y, row=0; y<dim.y && dy<buf_sz; y+=v.codom.y, row++, dy+=bytes_per_row) {
                char fmt[32];
                snprintf(fmt, sizeof(fmt),
                         y == 1 ? "@%%0%dx " : " %%%dx ", 
                         dim.x); 
                // \xe2\x94\x83 <- vertical line
                // don't print redundant 0s, as per HexII
                mvprintw(y, base.x, fmt, dy);
                mvchgat(y, base.x, dim.x, A_NORMAL, default_ui_color, NULL);
        }
        /* Detailed address bar: the cursor is special, but is at the top of
         * the screen. Highlight it a little so it stands out. */
        mvchgat(base.y, base.y, 1, A_NORMAL, 15, NULL); // '@'

        /* cell between address bars is kinda empty-looking, not sure what to
         * put there; I tried the name, but it looked out of place/didn't have
         * enough room for long ones. */
        //mvaddnstr(0, 2, v.name, grid_x - addr_pad); 
}

/* Things being considered:
 *
 * - More views side by side, like hex+ascii or entropy + hexdump. Trick is,
 *   they have to have the same number of bytes/row.
 * - Trying to annotate character classes on address bar.
 * - Generalize column bar as a view while at it.
 *
 *   first address line, capitals are highlighted as a scroll bar for B
 *   V       /- 2nd address line, showing subset of 1st
 *
 *   a eeeee B hhhh jjjjjj <- 3rd view, same bytes/row as h
 *   A eeeee B hhhh jjjjjj
 *   A eeeee B hhhh jjjjjj
 * s A eeeee B hhhh jjjjjj
 * S a eeeee B hhhh jjjjjj
 * s a eeeee B hhhh jjjjjj
 *   a eeeee B hhhh jjjjjj
 *
 * ^ scroll bar
 */
void render_grid(vec2 base, vec2 dim, const size_t buf_sz, const size_t cursor, 
                const unsigned char* const buf, view v, size_t bytes_per_full_row, const pal* pal) {
        /* cell between address bars is kinda empty-looking, not sure what to
         * put there; I tried the name, but it looked out of place/didn't have
         * enough room for long ones. */
        //mvaddnstr(0, 2, v.name, grid_x - addr_pad); 

        for (int y=base.y, row=0; y<dim.y; y+=v.codom.y, row++) {
                const size_t dy = cursor + row * bytes_per_full_row;

                // content
                for (int x=base.x, col=0; x<base.x+dim.x; x+=v.codom.x, col++) {
                        const size_t dx = col * v.dom;
                        if (dy + dx + v.dom <= buf_sz) {
                                v.render(buf + dy + dx, v.dom, y, x, pal);
                        } else {
                                for (int i=0;i<v.codom.x;i++) mvaddch(y, x + i, ' ');
                        }
                }
        }
}
