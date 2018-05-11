#!/usr/bin/tcc -run -L/usr/local/lib -lblindsight
#include <blindsight.h>
#include <assert.h>
#include <math.h>
#define _XOPEN_SOURCE_EXTENDED
// ^ pray to mr skeltal and open source gods
#include <ncurses.h>

/* A histogram view shows symbol (byte) distribution inside an address window.
 * It's not very space-efficient to show, so there's a few ways to compress it:
 *
 * - Shannon entropy is a measure of the information represented by the
 *   particular symbol distribution in a given address window.
 * - The chi-square test is a measure of how far a particular distribution is
 *   from the expected distribution. (For us, the expected distribution is
 *   probably a uniform one produced by encrypted data.)
 *
 * Neithers take symbol ordering into account, just their distribution in the
 * window. Shannon entropy is a little easier to display, so here it is.
 * */
static inline double shannon_entropy(uint16_t count[256], size_t window) {
        /* We're always dealing with 256 symbols, since we work in bytes.
         * Windows are often smaller than that, though; worst-case compression
         * for a 128-byte window would be 7 bits, 16-byte window 4 bits, etc. 
         *
         * For such smaller windows, we'll re-scale the output to '8' bits to
         * keep the shading range looking consistent while switching scales.
         */
        static double visual_scale;

        /* Memoize FP calc for small windows */
        static double lut[512] = {0};
        static size_t lut_for_sz = 0;
        if (lut_for_sz != window && window <= sizeof(lut)/sizeof(lut[0])) {
                lut_for_sz = window;
                for (int i=1; i<window; i++) {
                        double t = (double)i / window;
                        lut[i] = -t * log2(t);
                }
                visual_scale = window >= 256 ? 1.0 : (8.0 / log2(window));
        }

        double entropy = 0.0;
        if (window <= sizeof(lut)/sizeof(lut[0])) { /* small windows */
                for (int i=0;i<256;i++) entropy += lut[count[i]];
                entropy *= visual_scale;
        } else { /* unmemoized */
                for (int i=0;i<256;i++) {
                        if (!count[i]) continue;
                        double prob = (double)count[i] / window;
                        entropy += prob * log2(1 / prob);
                }
        }
        return entropy;
}

VIEW(entropy, 
        32,   /*=>*/ {1, 1, F_256C|F_FG_GRAY}, .zoom={16, 256}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        uint16_t count[256] = {0}; // histogram of symbol occurances
        uint8_t classes = 0; // mask of bits seen, used to detect ascii
        for (int i=0;i<n;i++) {
                count[s[i]]++;
                classes |= s[i];
        }

        double ent = shannon_entropy(count, n);
        int hi = (int)floor(ent);
        int lo = (int)(ent * 10) % 10;

        /* Playing around a bit, showing class via color and most significant
         * digit via DF-water-style numbers works pretty well. The interesting
         * high-entropy region can then be highlighted via background shade,
         * which keeps it obvious that it's a non-linear highlighting.
         *
         * The scaling trick I do for sub-256byte samples looks a little weird
         * when flipping between zoom levels, though. Quantization leads to
         * entropy "changing", very visible with the background shading.
         * Probably still worth the 256-color/ncursesw dependency.
         * */
        int color = 0;
        if (count[0] == n) {
                mvprintw(y, x, " "); // all 0s
                color = pal.fg_gray[3][0];
        } else if (count[255] == n) {
                mvprintw(y, x, "#"); // all Fs
                color = pal.fg_gray[1][0];
        } else if (classes >= ' ' && classes <= 0x7f && !count[0x7f]) {
                mvprintw(y, x, "%01x", hi); // ascii
                color = pal.fg_gray[3][0];
        } else if (ent <= 7.0) {
                mvprintw(y, x, "%01x", hi); // binary, low entropy
                color = pal.fg_gray[4][0];
        } else {
                mvprintw(y, x, "%01x", hi); // binary, very high entropy
                // TODO ought to perceptual-brightness scale this
                int bg = lo ? (int)((float)lo / 10 * 24) : 25;
                color = pal.fg_gray[lo ? 4 : 5][bg];
        }
        mvchgat(y, x, 1, A_NORMAL, color, NULL);
}

/* Grayscale byte occurance histogram, log-scaled to make low counts stand out.
 * (The log scale/palette isn't tuned to adjusting n yet, and this view
 * realllly needs a scaled legend.)
 *
 * Even consuming large byte chunks, this view is still space-inefficient. The
 * lack of a linear "sweep" is also discongruous - it's more detailed than an
 * entropy+class view, but less space-efficient.
 *
 * Using unicode for square "pixels" allows almost 2x the density of an ASCII
 * representation, and better addressing of individual bytes. However, the
 * ASCII representation does leave room for extra class/occurance count info. */
VIEW(bytehist, 
        1024, /*=>*/ {9, 17, F_256C|F_PIXELS}, .zoom={64, 4096}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        // grid
        attr_set(A_NORMAL, pal.gray[6], NULL); // make grid fade into background
        mvprintw(y, x+1, "0123456789abcdef");
        for (int row=1;row<9;row++) {
                mvprintw(y+row, x, "%x", 2 * (row - 1));
                mvaddwstr(y+row, x+1, L"\u2580\u2580\u2580\u2580" // 16x line of
                                      L"\u2580\u2580\u2580\u2580" // square blocks
                                      L"\u2580\u2580\u2580\u2580"
                                      L"\u2580\u2580\u2580\u2580");
        }
        attr_set(A_NORMAL, -1, NULL);

        assert(n <= 0xFFff && "uint16_t limit");
        uint16_t count[256] = {0}; 
        for (int i=0;i<n;i++) count[(s[i])]++;

        // render hist
        double scale = 24.0 / log2(n);
        for (int row=0;row<8;row++) {
                for (int col=0;col<16;col++) {
                        uint16_t top = (uint16_t)(scale * log2((double)count[row * 32 + col]));
                        uint16_t bot = (uint16_t)(scale * log2((double)count[row * 32 + col + 16]));
                        mvchgat(y+1+row, x+1+col, 1, A_NORMAL, pal.gray_gray[0 + bot][0 + top], NULL);
                }
        }
}

/* Like a hexdump, but only shows the top nibble through colour of square
 * Unicode blocks. In this case, grayscale sweep. Could do character classes or
 * something - but this is expensive on the color pair quota, since you need
 * N*N pairs for N colors, fg+bg. 
 * 
 * Since each row is two sequential sweeps of pixels, pixels become
 * non-sequential if there's more than one column. The spacing added to
 * indicate that might not be super-intuitive, hence this comment.
 * */
VIEW(bytepix, 
        128,  /*=>*/ {1, 65, F_256C|F_PIXELS} // errors on narrow screens
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        for (int col=0; col<(n/2); col++) {
                mvaddwstr(y, x+col, L"\u2580");
                const unsigned char top = s[col];
                const unsigned char bot = s[col + n/2];
                mvchgat(y, x+col, 1, A_NORMAL, pal.gray_gray[bot>>4][top>>4], NULL);
        }
}

/* (almost) vim's xxd.c in default mode */
VIEW(xxd, 
        16,/*=>*/ {1, 59}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        for (int i=0, xi=x; i<n; i+=2, xi+=5) {
                mvprintw(y, xi, "%02x%02x ", s[i], s[i+1]);
        }
        mvchgat(y, x, 41, A_NORMAL, 0, NULL);
        for (int i=0, xi=x+41; i<n; i++, xi++) {
                int color = 0;
                unsigned char txt = '.';
                switch(s[i]) {
                case 0: // only difference from xxd; hide nulls as HexII does
                        txt = ' ';
                break;
                case ' ' ... '~':
                        color = 3;
                        txt = s[i];
                break;
                case 0xff:
                        color = 1;
                break;
                }
                mvaddch(y, xi, txt);
                mvchgat(y, xi, 1, A_NORMAL, color, NULL);
        }
}

/* Matching Corkami's spec, although I'm considering removing the : from the
 * address bar which would break compatibility.
 *
 * Should match HexII in 1-byte mode, as long as the address bar retains :.
 *
 * Color scheme sorta-matches Radare 2, with the addition of grayscale hex.
 */
VIEW_(HexII, HexII_B,
        1, /*=>*/ {1, 3, F_256C}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        for (int i=0, xi=x; i<n*2; i+=2, xi+=2) {
                uint8_t c = *(uint8_t*)(s + i/2);

                char* fmt = 0;
                int arg = 0, clr = 0;

                switch(c) {
                case 0:
                        fmt = "  "; // zero still 'blank'
                break;
                case 0xff:
                        fmt = "##"; 
                        clr = 1;
                break;
                case ' ' ... '~':
                        // Could use A_UNDERLINE instead of the . to
                        // distinguish spaces, but it doesn't look a whole lot
                        // better than Corkami's way. The . is serializeable to
                        // plain .txt, but I doubt that's important for this
                        // kind of viewer.
                        fmt = ".%c";
                        arg = c;
                        clr = 3;
                break;
                default:
                        fmt = "%02X";
                        arg = c;
                        // Shade by high nibble, but make sure low values are
                        // still visible.
                        clr = 0 + pal.gray[8 + (c >> 4)];
                }
                mvprintw(y, xi, fmt, arg);
                mvchgat(y, xi, 2, A_NORMAL, clr, NULL);
        }
}

// slightly denser version:
view HexII_DW = {"HexII_DW", HexII_B, 4, /*=>*/ {1, 9, F_256C}};

VIEW(bits, 1, /*=>*/ {1, 8})(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        // assumes 8 bits per char
        // please do not run on obscure DSPs
        assert(n <= 2 && "bits must fit into one hex char");
        for(int i=0; i<n*8; i++) {
                int bit = (n*8 - 1) - i;
                if ((s[bit / 8] >> (bit % 8)) & 1) {
                        mvprintw(y, x+i, "%x", bit);
                        mvchgat(y, x+i, 1, A_NORMAL, pal.gray[25-i*2], NULL);
                }
        }
}

VIEW(braille, 
        1, /*=>*/ {1, 1, F_256C|F_UTF8}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        /* Show bits via 8-dot unicode braille.
         * Probably more pretty than useful, since bits aren't ordered
         * conveniently for patterns to appear. */
        assert(n <= 2 && "bits must fit into one hex char");
        wchar_t bit[2] = {0x2800 + s[0], 0};
        mvaddwstr(y, x, (wchar_t*)&bit);
        mvchgat(y, x, 1, A_NORMAL, pal.gray[8 + (s[0] >> 4)], NULL);
}

view* default_views[] = {
        &entropy,
        &bytehist,
        &bytepix,
        &braille,
        &HexII_DW,
        &HexII,
        &xxd,
        &bits,
        0 //{0},
};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, default_views, "default_views");
}
