/* 
 * "We're the goddamn Kalashnikovs of thinking meat." 
 *      - Echophraxia, by Peter Watts
 **/
#define _XOPEN_SOURCE_EXTENDED
// ^ pray to mr skeltal and open source gods
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "config.h"
#ifdef HAVE_NCURSESW_NCURSES_H
# include <ncursesw/ncurses.h>
#else
# include <ncurses.h>
#endif
#include "blindsight.h"
#include "palette.h"
#include "render.h"
#include "cmd.h"
#include "watcher.h"
#include "sandbox.h"
//#include <stdint.h> 
/* TODO: purge all non-unsigned with righteous flame, except those mandated by ncurses I guess >_>
      uh, maybe ignore ncurses and just use typedefs to mark separate coord spaces?  */

/* semver.org, major.minor.patch */
const char* blindsight_version = PACKAGE_VERSION;

/* Architecture:
  
   Entry point is an executable that bootstraps the main library.
   Main library sets up viewport, 
    mmaps file, 
    loads shaders and 
    starts rendering.
    It also watches the entry point for changes.
   Renderer library is a .so in entry point, re-loaded by renderer?
  

Contemplations: 
        Range-skips ala hd, worth implementing or no? Would be neat for heapviz.
        Nethack-style keyboard controls? :D
 */


/* Warn on missing stuff - has to run after curses is initialized */
void check_features_and_warn(const view* views, const char* locale) {
        feature present = 0, needed =0;

        if (COLORS >= 256) present |= F_256C;
        if (COLOR_PAIRS >= 0x7fff) present |= F_FG_GRAY|F_GRAY_GRAY;
        if (strstr(locale, "UTF-8")) present |= F_UTF8;

        for (const view* v=views; v->codom.y; v++) {
                if (v->codom.bounds & ~present) {
                        if (!needed) {
                                def_prog_mode();
                                endwin(); // drop back to terminal briefly
                                printf("Warning, these views require additional terminal capabilities:\n");
                        }

                        printf("\t%s\n", v->name);
                }
                needed |= v->codom.bounds;
        }
        if (needed & F_256C) 
                printf("Need 256+ colors. Set $TERM? Switch terminals?\n");
        if (needed & (F_FG_GRAY|F_GRAY_GRAY)) 
                printf("Need more color pairs. Set $TERM? Link ncursesw, not ncurses?\n");
        if (needed & F_UTF8) 
                printf("Need UTF-8 support. Set $TERM? Check locale?\n");
        if (needed) {
                printf("Running 'export TERM=xterm-256color' will probably fix everything!\n");
                // Take that, neckbeards! ^^^ This is for all the time I wasted
                // on Unix archeology research, trying to figure out how to
                // control terminal colors the "right" way. 
                //
                // Turns out, there's more cruft and dirty hacks than you can
                // shake a stick at and nobody really cares all that much. Time
                // was not well-spent.
                reset_prog_mode();
        }
}

pal screen_init() {
        initscr();
        pal p = palette_init();

        raw();                  // do not buffer until line break
        keypad(stdscr, TRUE);   // capture KEY_* function keys
        noecho();               // we control input
        nonl();                 // not sure what this does 
        curs_set(0);            // do not show cursor, if supported

        return p;
}

/* unsafe type-generic macros, safety is for Rust */
#define MIN(a, b) ((a)>(b) ? (b) : (a))
#define MAX(a, b) ((a)>(b) ? (a) : (b))

/* At the end of this call is the first point where untrusted data is
 * introduced into the program. The sandbox is thus initialized here; call
 * should be pushed as far forward in the initialization process as possible.
 *
 * FIXME should probably make this stdin-friendly
 */
const char* mmap_buf(const char* fname, size_t* fsize) {
        int in_fd = open(fname, O_RDONLY);
        if (in_fd == -1) err(1, "open");

        struct stat in_sb;
        if (fstat(in_fd, &in_sb)) err(1, "fstat");
        *fsize = in_sb.st_size;

        const char* buf = mmap(0, in_sb.st_size, PROT_READ, MAP_PRIVATE, in_fd, 0);
        if (buf == MAP_FAILED) err(1, "mmap");

        sandbox_init(); /* hope we're all done initializing */

        return buf;
}


/* 'hex view' grid
 * cached parameters updated on screen re-size or view change
 * not the cursor though, this stays constant through cursor changes */
typedef struct {
        /* screen coordinates */
        int max_y, max_x; // *screen* max, not just grid window's :|
        /* file coordinates */
        size_t bytes_per_full_row; // grid window rows
} viewport;

viewport viewport_init(int non_grid_x, view v) {
        // Trying a thing where instead of declaring inputs/outputs by
        // reference as const, they're passed returned by value. This makes
        // effects 100% clear without relying on warnings or looking up decls.
        viewport vp = {0};

        getmaxyx(stdscr, vp.max_y, vp.max_x); 
        // this is a macro, args are written to :|
        
        vp.max_y--; // compensate for cursor dead space at the bottom of window
                    // am I not initializing things correctly or something?

        vp.bytes_per_full_row = (vp.max_x - non_grid_x) / v.codom.x * v.dom;
        vp.bytes_per_full_row = 1 << dlog2(vp.bytes_per_full_row);

        return vp;
}

void reload_error(void* st, const char* msg) {
        // for now, dump syntax errors to stdout while dropped out of terminal
        printf("tcc: %s\n", msg);
}

/* Hot code reloads are watched on the input to the main render loop. However,
 * there are occasional other modes (help screen, debug palette dump, address
 * entry, etc) where a code reload isn't useful since it won't result in an
 * immediate re-render.
 * So, the main loop is polled with a timeout, but otherwise getch() is set to
 * block indefinitely. */
int key_poll(bool* trigger_reload) {
        timeout(10); // 200ms peak latency on live reloads should be fine
                      // want to spend time blocked on user input, not polling
        // XXX overpolling 20x until I figure out what's up with watcher...
        // seems like it's dropping messages from a full queue or something
        int key;
        do {
                if (watcher_check()) {
                        *trigger_reload = true;
                        key = 0; // exit loop w/o user input
                        break;
                }
                key = getch();
        } while (key == ERR);

        timeout(-1); // return to blocking mode
        return key;
}

/* Major actions on the view that key presses can request. Bounds checked
 * later, closer to the state the bounds checking is actually performed on.
 *
 * Alternatively, just poke ncurses/globals directly. Nobody's watching! 
 *
 * It's awfully tempting to generate a help screen with a horrific wrapper
 * macro. I shouldn't. */
typedef struct {
        intptr_t scroll;// relative scrolling
        size_t jump;    // absolute, SIZE_MAX goes to 0
        signed flip;    // view shift to adjacent ones
        signed zoom;    // view byte zoom, if supported
        // window sizes?
} input_req; 

cmd KEYS[] = {
        {"x",   "Exit program."},
        {"h?",  "Hotkey cheatsheet and help screen."},
        {.desc = "Cursor movement"},
        {"wasd","Vertical scroll by row, horizontal by cell."},
        {"WS",  "Vertical scroll by page."},
        {"g",   "Go to address 0x0."},
        {.desc = "View control"},
        {"qe",  "Flip through the view list. Acts like a \"zoom\"."},
        {"QE",  "Zoom bytes-per-cell scale on current view, if supported."},
        {.desc = "Debug"},
        {"p",   "Palette dump screen, for triaging color issues."},
        {"jJ",  "Debug background color test."},
        {0}
};
/* Printed Nethack-guidebook style in --help, 
 * vim-cheatsheet style in help screen. */

input_req key_actions(int key, viewport const vp, view const v, pal* pal) {
        input_req req = {0};

        switch(key) {
        /* window resize (apparently not as portable as SIGWINCH, but clean) */
        case KEY_RESIZE: endwin(); clear();     break;
        /* WASD-style scrolling */
        case KEY_LEFT:
        case 'a': req.scroll = -v.dom;
                  break;
        case KEY_RIGHT:
        case 'd': req.scroll = v.dom;
                  break;
        case KEY_UP:
        case 'w': req.scroll = -vp.bytes_per_full_row;
                  break;
        case KEY_DOWN:
        case 's': req.scroll = vp.bytes_per_full_row;
                  break;
        case KEY_PPAGE:
        case 'W': req.scroll = -vp.bytes_per_full_row * vp.max_y;
                  break;
        case KEY_NPAGE:
        case 'S': req.scroll = vp.bytes_per_full_row * vp.max_y;
                  break;
        /* view/zoom control */
        case 'e': req.flip = 1;         break;
        case 'q': req.flip = -1;        break;
        case 'E': req.zoom = 1;         break;
        case 'Q': req.zoom = -1;        break;
        /* jump back to zero */
        case 'g': req.jump = SIZE_MAX;  break;
        /* help screen */
        case 'h':
        case '?': cmd_render_help(KEYS);        break;
        /* debug palette dump to deal with weirdness */
        case 'p': palette_debug_dump(pal);         break;
        /* TEST TEST TEST DEBUG/IDEA STUFF */
        case 'j': assume_default_colors(0xf,0x08); break; // okay, can dynamically change this nice :D
        case 'J': assume_default_colors(0x7,0x00); break;
        }

        return req;
}

// TODO: generalize in terms of dims, drop viewport dependency, move into render.c
void render_scrollbar(const vec2 base, size_t buf_sz, size_t cursor, view v, viewport vp, const pal* pal) {
        // overview "scroll" address bar (not sure whether any address info will actually go there, yet)
        size_t bar_step = buf_sz / (vp.max_y + 1 - base.y);
        int bot_arrow = 0;
        for (int y=base.y; y<vp.max_y+1; y++) {
                size_t cur_end = cursor + vp.bytes_per_full_row * vp.max_y / v.codom.y; // close 'nuff
                size_t bar = bar_step * y;
                size_t bar_end = bar + bar_step;
                if ((cursor >= bar && cursor <= bar_end) ||
                    (cur_end >= bar && cur_end <= bar_end)) { // segments overlap
                        size_t overlap_lo = MAX(cursor, bar);
                        size_t overlap_hi = MIN(cur_end, bar_end);
                        uint8_t overlap = (uint8_t)((double)(overlap_hi - overlap_lo) / bar_step * 20);
                        /* Currently requires 256-color palette. Just inverting
                         * common UI color doesn't look quite as good as fading
                         * in % of bar accessed. Hmm. */
                        mvaddch(y, base.x, bot_arrow++ ? 'V' : 'A'); // arrow selection isn't perfect FIXME
                        mvchgat(y, base.x, 1, A_NORMAL, pal->gray[1 + overlap], NULL);
                } else if (cursor < bar && cur_end > bar_end) { // viewport covers bar step
                        mvaddch(y, base.x, '#');
                        mvchgat(y, base.x, 1, A_NORMAL, pal->gray[21], NULL);
                } else {
                        /* State of the unicodes:
                         *
                         * mvprintw prints UTF-8 encoded as byte sequences; but not wchars, at all, ever
                         * mvaddwstr prints wchars fine, just gotta prefix with L
                         * ACS_foo aren't properly configured in terminfo on my system, :doot:
                         */
                        //mvprintw(y, 0, " %c ", y==0 ? ACS_UARROW : (y==max_y-0) ? ACS_DARROW  : ACS_VLINE);
                        //mvaddstr(y, 0, " ");//L"\x2551 ");  // mvaddwstr for unicode char
                        //mvprintw(y, 0, " \xe2\x9c\x93 ");
                        //mvprintw(y, 0, "\xe2\x96\x80\xe2\x96\x84\xe2\x96\x81"); // upper lower upper
                        mvchgat(y, base.x, 1, A_NORMAL, pal->gray[0], NULL);
                }
        }
}

void render_info(const vec2 base, const view v, const viewport vp) {
        attr_set(A_REVERSE, default_ui_color, NULL);
        // TODO only have one piece of UI so far; make it pretty with colours, put units into background
        mvprintw(base.y, base.x, " \"%s\" b/char:%.3g b/cell:0x%x b/page:0x%X ", 
                // may need to compensate for top address line size, if later added V
                v.name, 
                (float)v.dom / (v.codom.x * v.codom.y), 
                v.dom, 
                vp.bytes_per_full_row*vp.max_y/v.codom.y);
        /* For reference: BZ's recent versions top out the default binary view
         * at 10*12 pixels = 120 bytes/hex-char, or 960 bits/hex-char. A single
         * terminal character can carry up to ~8 bits of information in glyphs,
         * and ~8 more in clever fg+bg palette use... 
         * However, BZ spends most screen estate on a classic hex+ascii dump,
         * so with an order of magnitude or so entropy+class display we can
         * still summarize files effectively. */
}

int blindsight(const int argc, char** argv, const view* views, const char* reload_sym) {
        if (argc != 2) {
                printf("         _   _ _       _     _     _   _   \n"
                       "        | |_| |_|___ _| |___|_|___| |_| |_ \n"
                       "        | . | | |   | . |_ -| | . |   |  _|\n"
                       "        |___|_|_|_|_|___|___|_|_  |_|_|_|  \n"
                       "                              |___|        \n"
                       "High-density binary viewer for visual pattern matching.\n"
                       "\n"
                       "Usage: %s <file>\n"
                       "\n",
                       argv[0]
                       );
                cmd_print_help(KEYS);
                printf("\n" "v%s on %s\n", blindsight_version, curses_version());
                return EXIT_FAILURE;
        }

        pal pal = screen_init();
        check_features_and_warn(views, setlocale(LC_CTYPE, ""));


        /* Three coordinates are kept separate by name and type convention:
         * 
         * file offsets:
         *      size_t cursor, dy, dx, dtmp, ...  
         * screen coordinates, curses-style:
         *      int y, x; 
         * addressed grid of cells:
         *      int row, col;
         */

        // TODO move definition/initialization somewhere appropriate (maybe group /w other state into struct?)
        bool trigger_reload = false;
        if (!watcher_init(argv[0], false)) { // non-blocking
                reload_sym = 0; // not running under tcc, don't poll inotify
        }

        size_t cursor = 0;
        signed view_ind = 0;  // index into view array, kept as int for easy rolling
        signed view_num = 0;
        for (const view* v=views; v->codom.y; v++) {view_num++;}
        view v = views[view_ind]; // starting view

        /* Going to keep the buffer as unsigned to make it clear it's data, not
         * strings. int8_t would also do, but would encourage UB casts. */
        size_t buf_sz;
        const unsigned char* buf = (unsigned char*)mmap_buf(argv[1], &buf_sz); // TODO move to struct return from arg pass?

        /* Window layout: there's three major vertical panes all stuffed
         * together as far left as possible. 
         *
         * - A fixed-width scroll bar, followed by
         * - A variable-width address bar, followed by
         * - A mostly fixed-width fancy 'hexdump'.
         *
         * addr_width plus some spacing is the screen x-offset for the
         * 'hexdump' grid.
         */
        int addr_width = (dlog2(buf_sz) + 1) / 4 + 1;  // nibbles in max address
        viewport vp = viewport_init(addr_width + 2, v);

        int key_press = 0;
        do { // main loop
                /* handle inputs and update state */
                if (trigger_reload) {
                        // Print any possible errors to console, relying on
                        // upcoming refresh() to restore terminal.
                        def_prog_mode();
                        endwin(); 

                        printf("Re-loading %s... ", argv[0]);
                        views = watcher_reload(reload_sym, 0, reload_error);
                        v = views[view_ind]; // that's what I get for doing stuff by-value...
                        assert(views && "live code reload didn't return a symbol!");
                        trigger_reload = false;
                        printf("ok\n");

                } 
                input_req req = key_actions(key_press, vp, v, &pal);

                /* Current theory is that scrolling should always be done in
                 * predictable increments.
                 * Jumping a portion of a cell down to 0 or up to the filesize
                 * is a little disconcerting and might lose an interesting
                 * location when zoomed out temporarily. So, never do less than
                 * a requested scroll.
                 */
                assert(buf_sz < INTPTR_MAX && "cleaner code, but <2GB inputs (lol)");
                if (req.scroll && buf_sz > (size_t)(cursor + req.scroll)) {
                        cursor += req.scroll;
                } else if (req.jump) {
                        cursor = req.jump == SIZE_MAX ? 0 : MIN(req.jump, buf_sz - 1); 
                } else if (req.flip) {
                        view_ind += req.flip;
                        view_ind = MAX(0, view_ind);
                        view_ind = MIN(view_ind, view_num - 1);
                        v = views[view_ind];
                } else if (req.zoom && v.zoom.min && v.zoom.max) {
                        v.dom = req.zoom < 0 ? v.dom >> 1 : v.dom << 1;
                        v.dom = MAX(v.zoom.min, v.dom);
                        v.dom = MIN(v.dom, v.zoom.max);
                }

                vp = viewport_init(addr_width + 2, v); // possible view changes,re-calc

                assert(1 << dlog2(v.dom) == v.dom && "viewport calc assumes v.dom is pow2");
                const int x_width = vp.bytes_per_full_row * v.codom.x / v.dom; // ^

                /* Writing view render functions is easier if you don't have to
                 * fill 100% of the declared codomain area. Clear on redraw. */
                attr_set(A_NORMAL, -1, NULL);
                erase(); 
                // Minimal address bar, separated from main grid by a scrol bar.
                render_scrollbar(yx(1, 0), buf_sz, cursor, v, vp, &pal);
                render_row_addrs(yx(1, 0), yx(vp.max_y + 1, addr_width + 2), 
                                buf_sz, cursor, v, vp.bytes_per_full_row);
                // Column addresses aligned with main grid.
                render_col_addrs(yx(0, addr_width + 2), x_width, 
                                vp.bytes_per_full_row, v);
                render_grid(yx(1, addr_width + 2), yx(vp.max_y+1, x_width), 
                                buf_sz, cursor, buf, v, 
                                vp.bytes_per_full_row, &pal);
                // View-info window somewhere out of the way.
                render_info(yx(vp.max_y - 0, vp.max_x - 62), v, vp);
                refresh();
        } while ((key_press = key_poll(&trigger_reload)) != 'x');

        endwin();
        return EXIT_SUCCESS;
}
