#include <assert.h>
#include "config.h"
#ifdef HAVE_NCURSESW_NCURSES_H
# include <ncursesw/ncurses.h>
#else
# include <ncurses.h>
#endif
#include "blindsight.h"
#include "palette.h"
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

palette palette_init() {
        palette pal = {
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
        //color_content(COLOR_BLACK, 0, 0, 0);
        //bkgdset(COLOR_PAIR(255 + 8*26) | '#');
        return pal;
}

void palette_debug_dump(palette* pal) { // for when ^^^ inevitably breaks for mysterious reasons
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

