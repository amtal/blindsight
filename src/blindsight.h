#ifndef BLINDSIGHT_H
#define BLINDSIGHT_H 1
#include <stdint.h>
#include <stddef.h> // size_t

/* Versioned according to Semantic Versioning 2.0.0, http://semver.org */
extern const char* blindsight_version; // "v<MAJ>.<MIN>.<PATCH>"

/* Palettes set up for ncurses mvchgatt color pairs. */
typedef struct {
    short const gray[26];
    short fg_gray[16][26];
    short gray_gray[26][26];
} pal; // this struct is dependency hell :|

typedef enum {
        F_256C = 1,     // $TERM == xterm-256color or similar
        F_FG_GRAY = 2,  // 16-bit color pair limit, requires ncursesw (wide)
        F_GRAY_GRAY = 4,// *
        F_UTF8 = 8,     // locale should be en_US.UTF-8 or similar
        F_PIXELS = F_GRAY_GRAY|F_UTF8, // Unicode blocks + FGxBG monotone
} feature;

/* This library generalizes hexdumps as tables of identical cells.
 *
 * The table shows a sequential, uninterrupted window of buffer contents. The
 * address of each cell within it is the sum of the column address and row
 * address, which are shown in address bars if there's room. 
 *
 * To keep addresses simple to read, the byte width for a single row is
 * constrained to powers of 2.
 *
 * A 'view' struct contains the information needed to render a single
 * standalone cell. This operation is mapped over the table for a full dump.
 * */
typedef struct {
        /* The 'render' function is a mapping from a byte array domain to an
         * ncurses screen codomain. Specifically:
         *
         * - The domain is a fixed-width window into the overall binary being
         *   displayed.
         * - The codomain is a y*x character rectangle, possibly with additional
         *   degrees of freedom such as colors, blink attributes, or emoji.
         *
         * The dimensions and cardinalities of the domain and codomain are
         * useful for categorizing functions in terms of information density,
         * bijective versus lossy display, and terminal capabilities needed.
         * They're declared statically alongside the render function. */
        size_t dom; /* Byte window being shown, pow2 only. Passed as buf_sz. */
        struct {
            uint16_t y, x;   /* Screen-coordinate bounds of codomain. */
            feature bounds;  /* Color pair/character set bounds.
                                Determines terminal features needed. */
        } codom;
        const char* name;
        /* The function maps the domain into an "image" in the codomain. :) */
        void (*render)(const unsigned char* buf, size_t buf_sz, 
                      /* -> */ int y, int x, const pal* pal);

        /* Some parameters are optional. For instance, views that summarize
         * lots of data (histograms, entropy, region classification) can have
         * their domain changed at runtime without affecting the codomain.
         */
        struct {size_t min, max;} zoom; /* Must still be powers of 2. */
        /* A description of what the view shows can help understand it. */
        const char* help;
} view;

/* Helper for defining rendering functions, while the function signature is
 * still in flux.
 *
 * buf:     content being drawn
 * buf_sz:  view.dom passthrough
 * y, x:    ncurses coordinate for the cell
 * pal:     color pairs grouped by palette
 *
 * Oh yeah; currently v.render(..) will not be called on any tail <view.bytes
 * at the end of a file. Can't render chunks smaller than chunk size, so views
 * with large domain will truncate the file being examined without any visual
 * indication that it's happening.
 */
#define RF(name) void rf_ ## name(const unsigned char* buf, size_t buf_sz, \
                                  int y, int x, const pal* pal)

/* Main executable 
 * argc, argv:  passthrough from main
 * views:       selection of view structs that will be rendered
 * reload_sym:  name of the symbol defining the previous argument
 *              (will be reloaded on save if live code reload is supported)
 */
int blindsight(const int argc, char** argv, const view* views, const char* reload_sym);

#endif
