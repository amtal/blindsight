#ifndef BLINDSIGHT_H
#define BLINDSIGHT_H 1
#include <stdint.h>
#include <stddef.h> // size_t

typedef enum {
        F_256C = 1,   // $TERM == xterm-256color or similar
        F_WPAIRS = 2, // 16-bit color pair limit, requires ncursesw (wide)
        F_UTF8 = 4,   // locale should be en_US.UTF-8 or similar
} feature;

/*
 * Palettes set up for ncurses mvchgatt color pairs.
 */
typedef struct {
    short const gray[26];
    short fg_gray[16][26];
    short gray_gray[26][26];
} pal;
// Gonna try moving this to a struct and an arg to deal with reloading better.
// However, this struct is dependency hell...

typedef struct {
        /* Several views are well-adapted to scaling `bytes`, while keeping
         * viewport identical. Should probably add hotkeys to do so! Maybe
         * limits as well?*/
 /*[1]*/uint16_t y, x;
        size_t bytes;   /* can range between b_min..b_max in powers of 2 */
        void (*render)(int y, int x, const unsigned char* buf, size_t buf_sz, const pal* pal);
        const char* name;
        /* optional */
        feature needs;  /* No need to set super-rigorously; as long as at least
                           one view is enabled that has the same requirement, a
                           warning will pop up. This just makes warnings more
                           detailed, showing which views are affected. */
        size_t min_bytes, max_bytes; /* if set, allows adjustment of bytes */
} view;

/*
 * y, x:    ncurses coordinate for the cell
 * buf:     content being drawn
 * buf_sz:  view.bytes passthrough
 *
 * Oh yeah; currently v.render(..) will not be called on any tail <view.bytes
 * at the end of a file. Can't render chunks smaller than chunk size.
 */
#define RF(name) void render_ ## name(int y, int x, const unsigned char* buf, size_t buf_sz, const pal* pal)

int blindsight(const int argc, char** argv, const view* views, const char* reload_sym);

extern const char* blindsight_version; /* semver.org, major.minor.patch */

/* [1] For some reason, gcc propagates a const marking here up to the view v =
 * ... definition elsewhere. Errors saying a read-only variable's being
 * assigned. Which sort of makes sense, except tcc + clang ignore const member
 * assignments. Standard breakage, or undefined? Not filling me with faith in
 * const. */
#endif
