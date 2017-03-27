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
#ifdef HAVE_NCURSESW_NCURSES_H
# include <ncursesw/ncurses.h>
#else
# include <ncurses.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "blindsight.h"
#include "watcher.h"
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
        if (COLOR_PAIRS >= 0x7fff) present |= F_WPAIRS;
        if (strstr(locale, "UTF-8")) present |= F_UTF8;

        for (const view* v=views; v->y; v++) {
                if (v->needs & ~present) {
                        if (!needed) {
                                def_prog_mode();
                                endwin(); // drop back to terminal briefly
                                printf("Warning, these views require additional terminal capabilities:\n");
                        }

                        printf("\t%s\n", v->name);
                }
                needed |= v->needs;
        }
        if (needed & F_256C) printf("Need 256+ colors. Set $TERM? Switch terminals?\n");
        if (needed & F_WPAIRS) printf("Need more color pairs. Set $TERM? Link ncursesw, not ncurses?\n");
        if (needed & F_UTF8) printf("Need UTF-8 support. Set $TERM? Check locale?\n");
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

pal palette_init() {
        pal pal = {
                .gray = {
                        0x10, // rgb:000
                        0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 
                        0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3,
                        0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 
                        0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
                        0xe7 // rgb:fff
                },
                .fg_gray = {{0}},
                .gray_gray = {{0}},
        };

        assert(!start_color());
        assert(has_colors());
        /* The default xterm-256color palette, and hopefully others, goes:
         *
         * 0..7         8 dark [black RGYB magenta cyan white] \__prone to remapping!
         * 8..f         8 light [*]                            /
         * 10..e7       6x6x6 rgb cube (black:0x10, white:0xe7)
         * e8..ff       24-value gray sweep (doesn't reach rgb000/fff!)
         *
         * When using ncurses built with extra color pair support, we've got
         * 0x7fff fg-bg pairs to play with. Constructed ncurses palettes
         * probably fall into the following categories:
         *
         *      [Core]
         * - Basic 16 colors. These retain any custom style, and support inversion.
         *   Pro: maintain look and feel. Con: only useable for classification,
         *   likely to run into invisible characters when using background as
         *   an extra dimension.
         * - Worth mapping all 256 basic colors on a static background for
         *   color debug purpose, anyway. That's 1/0x7f palette budget.
         *
         *      [Sweeps]
         * - Square unicode (or DOS CP437 or w/e) pixels are handy for retaining
         *   proportions. Full grayscale sweep of both takes 0x2a4.
         * - Cube helix sweep of hue to extend the grayscale sweep. Should add
         *   an extra couple of bits of information per character, which is
         *   clutch.
         *
         *      [Classes overlaid on sweeps]
         * - Basic 16 colors on grayscale might work, regardless of themes.
         * - Otherwise, import some of the nicer ones (hot/cold delta) and
         *   overlay.
         *
         * Shouldn't run out of budget until heavy use of cube helix on cube
         * helix. Probably don't need palette swapping, except maybe when
         * versioning stabilizes. (Since any external render funs will depend
         * on palettes.)
         */
        use_default_colors();  // roll with defaults for -1...
        assume_default_colors(0x7,0x10); // can use this to remap dynamically, per-renderer palette load?

        
        short base = 0;
        for (short c=0; c<256; c++) init_pair(base+c, c, -1);
        base += 256;

        /* On systems with 256 max color pairs, don't call init_pair beyond
         * that range; it messes up what palette we do have. */
        if (base >= COLOR_PAIRS) return pal;

        for (short fg=0; fg<16; fg++) {
                for (short bg=0; bg<26; bg++) {
                        short pair = base + fg*26 + bg; // TODO running counter this, lol
                        init_pair(pair, fg, pal.gray[bg]);
                        pal.fg_gray[fg][bg] = pair;
                }
        } // experimental: background /w contrast
        base += 26*16;
        for (short fg=0; fg<26; fg++) {
                for (short bg=0; bg<26; bg++) {
                        short pair = base + fg*26 + bg;
                        bool palette_hack = false; // HACK: oookay this can't be portable
                                                  // but on this version of ncurses one of the
                                                  // high bits inverts the fg-bg colors, so while
                                                  // building the palette I'm un-inverting it
                                                  //
                                                  // tracing down bug in source and avoiding the
                                                  // problematic bit (and there's probably more)
                                                  // will probably be more portable :)
                                                  //
                                                  // It gets weirder. That's nor normal; it just 
                                                  // somehow sometimes gets into that mode. Colors 
                                                  // inverted, except this small range. WAT.
                        if (palette_hack && (fg<2 || (fg==2 && bg<19))) {
                                init_pair(pair, pal.gray[bg], pal.gray[fg]);
                        } else {
                                init_pair(pair, pal.gray[bg], pal.gray[fg]);
                        }
                        pal.gray_gray[fg][bg] = pair;
                }
        } // tone-on-tone, for square unicode pixels

        /* technically unused, since V afaik doesn't work?
         * or at least .Xresources takes precedence */
        assert(can_change_color()); 
        color_content(COLOR_BLACK, 0, 0, 0); // I see a background and I wanted it painted black
        //bkgdset(COLOR_PAIR(255 + 8*26) | '#');
        return pal;
}

void palette_debug_dump(pal* pal) { // for when ^^^ inevitably breaks for mysterious reasons
        erase();
        mvprintw(0, 8, "[linear dump, 0x%x colors, 0x%x pairs]", COLORS, COLOR_PAIRS);
        for (int x=0;x<128;x++) {
                mvprintw(1, x, "%x", x >> 4);
                mvprintw(2, x, "%x", x & 7);
                for (int y=0;y<12;y++) {
                        mvprintw(3+y, x, "P");
                        mvchgat(3+y, x, 1, A_NORMAL, y*128+x, NULL);
                }
        }

        mvprintw(18, 8, "[sweeps]");
        for (int y=0;y<16;y++) {
                for (int x=0;x<26;x++) {
                        mvprintw(y+19, x, "#");
                        mvchgat(y+19, x, 1, A_NORMAL, 256+x+y*26, NULL);
                }
        }
        for (int y=0;y<26;y++) {
                for (int x=0;x<26;x++) {
                        mvprintw(y+41, x, "$");
                        mvchgat(y+41, x, 1, A_NORMAL, pal->gray_gray[y][x], NULL);
                }
        }
        refresh();
        getchar();
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


// FIXME should probably make this stdin-friendly
const char* mmap_buf(const char* fname, size_t* fsize) {
        int in_fd = open(fname, O_RDONLY);
        if (in_fd == -1) err(1, "open");

        struct stat in_sb;
        if (fstat(in_fd, &in_sb)) err(1, "fstat");
        *fsize = in_sb.st_size;

        const char* buf = mmap(0, in_sb.st_size, PROT_READ, MAP_PRIVATE, in_fd, 0);
        if (buf == MAP_FAILED) err(1, "mmap");

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

viewport viewport_init(int grid_x, view v) {
        // Trying a thing where instead of declaring inputs/outputs by
        // reference as const, they're passed returned by value. This makes
        // effects 100% clear without relying on warnings or looking up decls.
        viewport vp = {0};

        getmaxyx(stdscr, vp.max_y, vp.max_x); 
        // this is a macro, args are written to :|
        
        vp.max_y--; // make space for 1-tall address bar

        vp.bytes_per_full_row = (vp.max_x - grid_x) / v.x * v.bytes;
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

void render_help();

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

input_req key_actions(int key, viewport const vp, view const v, pal* pal) {
        input_req req = {0};

        switch(key) {
        /* window resize (apparently not as portable as SIGWINCH, but clean) */
        case KEY_RESIZE: endwin(); clear();     break;
        /* WASD-style scrolling */
        case KEY_LEFT:
        case 'a': req.scroll = -v.bytes;
                  break;
        case KEY_RIGHT:
        case 'd': req.scroll = v.bytes;
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
        case '?': render_help();        break;
        /* debug palette dump to deal with weirdness */
        case 'p': palette_debug_dump(pal);         break;
        /* TEST TEST TEST DEBUG/IDEA STUFF */
        case 'j': assume_default_colors(0xf,0x08); break; // okay, can dynamically change this nice :D
        case 'J': assume_default_colors(0x7,0x00); break;
        }

        return req;
}

const struct {
        const char* keys; // optional
        const char* desc; // mandatory until last entry
} KEYS[] = {
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

#define KB_SZ 3
// Try a help screen sort of like the vim cheatsheets, for better spacial
// awareness of key layout.
void render_help() {
        const char* const wasd_kb[KB_SZ] = {
                "qQ wW eE rR tT yY uU iI oO pP [{ ]} \\|",
                " aA sS dD fF gG hH jJ kK lL ;: '\"",
                "  zZ xX cC vV bB nN mM ,< .> /?",
        };

        erase();
        attr_set(A_NORMAL, -1, NULL);
        
        for (int y=0; y<KB_SZ; y++) mvprintw(y, 0, wasd_kb[y]);

        unsigned pal = 2; // ghetto-palette in basic 16 colors
        for (int i=0, y=KB_SZ+1; KEYS[i].desc; i++, y++) {
                if (!KEYS[i].keys) {
                        // group entry, do not advance palette
                        y++; 
                        mvprintw(y, 4, "%s:", KEYS[i].desc);
                } else {
                        for (const char* c=KEYS[i].keys; *c; c++) {
                                for (int y=0; y<KB_SZ; y++) {
                                        for (int x=0; wasd_kb[y][x]; x++) {
                                                if (wasd_kb[y][x] == *c) {
                                                        mvchgat(y, x, 1, A_NORMAL, pal, 0);
                                                }
                                        }
                                }
                        }
                        mvaddstr(y, 0, KEYS[i].keys);
                        mvaddstr(y, 8, KEYS[i].desc);
                        mvchgat(y, 8, strlen(KEYS[i].desc), A_NORMAL, pal, 0);
                        pal++;
                }
        }
        
        refresh();
        getch();
}

const int default_ui_color = 6; // tryout this idea

void render_info(view v, viewport vp, const pal* pal) {
        //attr_set(A_NORMAL, pal->gray_gray[18][1], NULL);
        attr_set(A_REVERSE, default_ui_color, NULL);
        // TODO only have one piece of UI so far; make it pretty with colours, put units into background
        // maybe print palette Legend as well?
        //attr_on(A_REVERSE, NULL); // eh, this looks poor
        mvprintw(vp.max_y - 0, vp.max_x - 62, " \"%s\" b/char:%.3g b/cell:0x%x b/page:0x%X ", 
                // may need to compensate for top address line size, if later added V
                v.name, (float)v.bytes / (v.x * v.y), v.bytes, vp.bytes_per_full_row*(vp.max_y));
        //attr_off(A_REVERSE, NULL);

        // for reference: BZ's recent versions top out the default binary view at 10x12=120bpc
        // reaching that is afaik possible with two things:
        //       sensor fusion (entropy + character classification) 
        //       maximizing information bandwidth (rogue-like use of background color + foreground color + glyphs!)
        //mvprintw(vp.max_y - 1, vp.max_x - 55, " dbg: 0x%x/colors, 0x%x/pairs ", COLORS, COLOR_PAIRS);
}

void render_scroll(size_t buf_sz, size_t cursor, viewport vp) {
        attr_set(A_NORMAL, -1, NULL);

        // overview "scroll" address bar (not sure whether any address info will actually go there, yet)
        size_t bar_step = buf_sz / (vp.max_y + 1);
        for (int y=0; y<vp.max_y+1; y++) {
                size_t cur_end = cursor + vp.bytes_per_full_row * vp.max_y;
                size_t bar = bar_step * y;
                size_t bar_end = bar + bar_step;
                if ((cursor >= bar && cursor <= bar_end) ||
                    (cur_end >= bar && cur_end <= bar_end)) { // segments overlap
                        size_t overlap_lo = MAX(cursor, bar);
                        size_t overlap_hi = MIN(cur_end, bar_end);
                        mvprintw(y, 0, "%01x", (uint8_t)((double)(overlap_hi - overlap_lo) / bar_step * 15));
                } else if (cursor < bar && cur_end > bar_end) { // viewport covers bar step
                        mvprintw(y, 0, "f");
                } else {
                        /* State of the unicodes:
                         *
                         * mvprintw prints UTF-8 encoded as byte sequences; but not wchars, at all, ever
                         * mvaddwstr prints wchars fine, just gotta prefix with L
                         * ACS_foo aren't properly configured in terminfo on my system, :doot:
                         */
                        //mvprintw(y, 0, " %c ", y==0 ? ACS_UARROW : (y==max_y-0) ? ACS_DARROW  : ACS_VLINE);
                        mvaddstr(y, 0, " ");//L"\x2551 ");  // mvaddwstr for unicode char
                        //mvprintw(y, 0, " \xe2\x9c\x93 ");
                        //mvprintw(y, 0, "\xe2\x96\x80\xe2\x96\x84\xe2\x96\x81"); // upper lower upper
                        
                }
                mvchgat(y, 0, 1, A_NORMAL, 0, NULL);
        }
}

void render_grid(const int grid_x, const size_t buf_sz, const size_t cursor, 
                const unsigned char* const buf, view v, viewport vp, const pal* pal) {
        //attr_set(A_NORMAL, pal->gray[16], NULL);

        const int addr_pad = 4; // 2-char scroll bar + ": "

        assert(1 << dlog2(v.bytes) == v.bytes && "viewport calc assumes v.bytes is pow2");
        const int x_width = vp.bytes_per_full_row * v.x / v.bytes; // ^

        char fmt[32]; // variable-width formatting tmpbuf

        // detailed column address bar
        /* Column width in bytes is a power of 2; rendering offsets into it
         * requires numbers lower than it. In many cases view.x is going to be
         * smaller than this, so we can't render an address for each column. In
         * such cases, we'll render an address every nth column, but the
         * addresses will not be truncated in any way. */
        {
                unsigned int addr_sz = dlog2(vp.bytes_per_full_row) / 8 + 1;
                addr_sz *= 2; // printed in hex
                addr_sz += 1; // add a space?
                unsigned int skip = addr_sz / v.x; // # of columns addr shown
                if (addr_sz % v.x) skip++; // ceil(^)
                snprintf(fmt, sizeof(fmt), "%%0%dx ", addr_sz-1);
                size_t addr = 0;
                for (int x=grid_x; x<grid_x+x_width; x+=v.x * skip,
                                addr+=v.bytes * skip) {
                        mvprintw(0, x, fmt, addr);
                        mvchgat(0,  x, addr_sz-1, A_NORMAL, default_ui_color, NULL);
                }
        }

        for (int y=1, row=0; y<vp.max_y+1; y+=v.y, row++) {
                const size_t dy = cursor + row * vp.bytes_per_full_row;

                // detailed row address bar, start ghetto improve later
                if (dy < buf_sz) {
                        snprintf(fmt, sizeof(fmt), 
                                 y == 1 ? "[%%0%dx]" : " %%%dx:", 
                                 grid_x - addr_pad); 
                        // \xe2\x94\x83 <- vertical line
                        // don't print redundant 0s, as per HexII
                        mvprintw(y, 1, fmt, cursor + row * vp.bytes_per_full_row);
                        mvchgat(y,  1, grid_x - addr_pad + 2, A_NORMAL, default_ui_color, NULL);
                }

                // content
                for (int x=grid_x, col=0; x<grid_x+x_width; x+=v.x, col++) {
                        const size_t dx = col * v.bytes;
                        if (dy + dx + v.bytes <= buf_sz) {
                                v.render(y, x, buf + dy + dx, v.bytes, pal);
                        } else {
                                for (int i=0;i<v.x;i++) mvprintw(y, x + i, " ");
                        }
                }
        }
        /* Detailed address bar: the cursor is special, but is at the top of
         * the screen. Highlight it a little so it stands out. */
        mvchgat(1, 1, 1,        A_NORMAL, 14, NULL); // '['
        mvchgat(1, grid_x-2, 1, A_NORMAL, 14, NULL); // ']'

        /* cell between address bars is kinda empty-looking, not sure what to
         * put there; I tried the name, but it looked out of place/didn't have
         * enough room for long ones. */
        //mvaddnstr(0, 2, v.name, grid_x - addr_pad); 
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
                for (int i=0; KEYS[i].desc; i++) {
                        if (!KEYS[i].keys) {
                                printf("\n    %s:\n", KEYS[i].desc);
                        } else { 
                                printf("%s\t%s\n", KEYS[i].keys, KEYS[i].desc);
                        }
                }
                printf("\n" "v%s on %s\n", blindsight_version, curses_version());
                return EXIT_FAILURE;
        }

        /* Going to keep the buffer as unsigned to make it clear it's data, not
         * strings. int8_t would also do, but would encourage UB casts. */
        size_t buf_sz;
        const unsigned char* buf = (unsigned char*)mmap_buf(argv[1], &buf_sz); // TODO move to struct return from arg pass?

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
        for (const view* v=views; v->y; v++) {view_num++;}
        view v = views[view_ind]; // starting view

        /* Window layout: there's three major vertical panes all stuffed together as far left as possible. 
         *
         * - A fixed-width scroll bar, followed by
         * - A variable-width address bar, followed by
         * - A mostly fixed-width fancy 'hexdump'
         *
         * I'm considering adding more views, like the side-by-side hex+ascii
         * view but with more visual options. Making sure the visualizations
         * align in bytes-per-row seems tricky, though. I'm not sure if there's
         * enough compatible ones to make it worthwhile.
         *
         * Two additional possibilities are likely:
         *
         * - Top-side address bar, possibly variable width for accurate addressing of large cells.
         * - Detailed status bar indicating the name and density of a given view.
         *
         */

        /* Layout is currently done the ghetto way. grid_x is the x-offset for
         * the main grid; the address bar/scroll bar are computed from it. 
         * curses windows are considered a complication and unused.
         *
         * It's tempting to generalize address bars as a high-density grid,
         * and start displaying character metadata in them. Multiple scroll
         * bars can then be introduced, for selecting specific regions and
         * viewing their details.
         *
         * However, that's probably nutty for a console viewer. Information
         * density per character isn't good enough to give an overview of the
         * file in the space of an address bar, especially not while also
         * showing the address. Whole-file overview would also require serious
         * caching and ideally be done in parallel. 
         *
         * Hm, yeah. If a single character can (very optimistically) carry ~16
         * bits of information (8 from glyphs, 8 more from clever fg+bg palette
         * use) that's ~50 bits per address line. Compare that to bz's view,
         * which stuffs 9x12 pixels at 8 bits of info each. (For a wide font,
         * but still.) That's ~540 bits of info per address line - I don't
         * think multilevel overview bars are viable here.
         *
         * What may be viable is multiple displays;
         *
         *   first address line, capitals are highlighted as a scroll bar for B
         *   V       /- 2nd address line, showing subset of 1st
         *
         *   a eeeee B hhhh 
         *   A eeeee B hhhh
         *   A eeeee B hhhh
         * s A eeeee B hhhh
         * S a eeeee B hhhh
         * s a eeeee B hhhh
         *   a eeeee B hhhh
         *
         * ^ scroll bar... this is probably too much for command line use in a small terminal
         *   ...and if someone's wide-screening this, might as well do proper bitmap views!
         * */
        int grid_x = (dlog2(buf_sz) + 1) / 8 + 1;  // bytes needed to store address
        grid_x = (grid_x * 2 + 1) + 2; // printing address in hex, with an extra space, and a 2-wide overview bar
        viewport vp = viewport_init(grid_x, v);

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
                } else if (req.zoom && v.min_bytes && v.max_bytes) {
                        v.bytes = req.zoom < 0 ? v.bytes >> 1 : v.bytes << 1;
                        v.bytes = MAX(v.min_bytes, v.bytes);
                        v.bytes = MIN(v.bytes, v.max_bytes);
                }

                // re-calc viewport due to possible changes
                vp = viewport_init(grid_x, v);

                /* clear leftovers, since we'll never be doing partial updates
                 * page scroll caching would be neat, but you're *probably*
                 * bottlenecked by term update (esp. over SSH) than processing
                 * and I guess there might be a static-ish legend/address bar,
                 * but not sure *how* static... Come back to this during
                 * optimization. */
                erase(); 


                /* render different windows/panels */
                render_scroll(buf_sz, cursor, vp);
                render_grid(grid_x, buf_sz, cursor, buf, v, vp, &pal);
                render_info(v, vp, &pal);

                refresh();
        } while ((key_press = key_poll(&trigger_reload)) != 'x');

        endwin();
        return EXIT_SUCCESS;
}
