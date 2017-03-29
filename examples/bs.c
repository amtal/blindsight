#!/usr/bin/tcc -run -L/usr/local/lib -lblindsight
#include "blindsight.h"
#include <assert.h>
#include <math.h>
#define _XOPEN_SOURCE_EXTENDED
// ^ pray to mr skeltal and open source gods
#include <ncurses.h>


/* Shannon entropy, takes a string of symbols and counts occurances of
 * particular symbols. Then, predicts ideal compression for that string in
 * bits. Disregards ordering. Works best with large samples? Okay at spotting
 * code, occasionally crypto material. 
 *
 * End use is similar to Chi Squared test, which determines distance from what
 * a uniform byte distribution would generate.
 * */
RF(ent) {
        /* Cache repetitive FLOPS in a lookup table calculated on first render.
         * Max buf_sz limited by LUT size, chosen based on performance trial & 
         * error on an ancient netbook. */
        static double lut[256];
        static size_t lut_for_sz = 0;

        assert(buf_sz <= sizeof(lut) && "entropy cache size limit");
        if (lut_for_sz != buf_sz) {
                for (int i=0; i<buf_sz; i++) {
                        double t = (double)i / buf_sz;
                        lut[i] = -t * log2(t);
                }
                lut_for_sz = buf_sz;
        }

        // histogram symbol occurances, then accumulate entropy based on counts
        uint8_t count[256] = {0}; // 256 possible symbols in a byte
        double entropy = 0.0;
        uint8_t classes = 0;

        assert(buf_sz <= sizeof(count) && "uint8_t limit on histogram");
        for (int i=0;i<buf_sz;i++) {
                uint8_t c = (uint8_t)(buf[i]);
                count[c]++;
                classes |= c;
        }
        for (int i=0;i<256;i++) if (count[i]) entropy += lut[count[i]];

        // render
        
        // TODO fix 0..8thness / highlight particularly high-entropy (key/crypto) regions!
        // TODO display class with (ideally) colour, ugh color pairs
        // not sure what to display; just character classes, or low entropy bit?
        int hi = (int)floor(entropy);
        //int lo = (int)(entropy * 10) % 10;
        if (count[0] == buf_sz) {
                mvprintw(y, x, " "); // TODO benchmark using right funcs instead of printw on everything
                mvchgat(y, x, 1, A_NORMAL, pal->fg_gray[3][0], NULL);
        } else if (count[255] == buf_sz) {
                mvprintw(y, x, "#");
                mvchgat(y, x, 1, A_NORMAL, pal->fg_gray[3][0], NULL);
        } else if (classes >= ' ' && classes <= 0x7f) {
                // ASCII + DEL, close 'nuff
                mvprintw(y, x, "a");
                mvchgat(y, x, 1, A_NORMAL, pal->fg_gray[4][hi*3], NULL);
        } else {
                mvprintw(y, x, "$");
                mvchgat(y, x, 1, A_NORMAL, pal->fg_gray[3][hi*3], NULL);
        }
}

/* Grayscale byte occurance histogram, log-scaled to make low counts stand out.
 * (The log scale/palette isn't tuned to adjusting buf_sz yet, and this view
 * realllly needs a scaled legend.)
 *
 * Even consuming large byte chunks, this view is still space-inefficient. The
 * lack of a linear "sweep" is also discongruous - it's more detailed than an
 * entropy+class view, but less space-efficient.
 *
 * Using unicode for square "pixels" allows almost 2x the density of an ASCII
 * representation, and better addressing of individual bytes. However, the
 * ASCII representation does leave room for extra class/occurance count info. */
RF(hist_square) {
        // grid
        attr_set(A_NORMAL, pal->gray[6], NULL); // make grid fade into background
        mvprintw(y, x+1, "0123456789abcdef");
        for (int row=1;row<9;row++) {
                mvprintw(y+row, x, "%x", 2 * (row - 1));
                mvaddwstr(y+row, x+1, L"\u2580\u2580\u2580\u2580" // 16x line of
                                      L"\u2580\u2580\u2580\u2580" // square blocks
                                      L"\u2580\u2580\u2580\u2580"
                                      L"\u2580\u2580\u2580\u2580");
        }
        attr_set(A_NORMAL, -1, NULL);

        assert(buf_sz <= 0xFFff && "uint16_t limit");
        uint16_t count[256] = {0}; 
        for (int i=0;i<buf_sz;i++) count[(uint8_t)(buf[i])]++;

        // render hist
        for (int row=0;row<8;row++) {
                for (int col=0;col<16;col++) {
                        uint16_t top = (uint16_t)log2((double)count[row * 32 + col]);
                        uint16_t bot = (uint16_t)log2((double)count[row * 32 + col + 16]); // something weird here
                        mvchgat(y+1+row, x+1+col, 1, A_NORMAL, pal->gray_gray[0 + bot*2][0 + top*2], NULL);
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
RF(byte_pixels) {
        for (int col=0; col<(buf_sz/2); col++) {
                mvaddwstr(y, x+col, L"\u2580");
                const unsigned char top = buf[col];
                const unsigned char bot = buf[col + buf_sz/2];
                mvchgat(y, x+col, 1, A_NORMAL, pal->gray_gray[bot>>4][top>>4], NULL);
        }
}

/* (almost) vim's xxd.c in default mode */
RF(xxd) {
        for (int i=0, xi=x; i<buf_sz; i+=2, xi+=5) {
                mvprintw(y, xi, "%02x%02x ", buf[i], buf[i+1]);
        }
        mvchgat(y, x, 41, A_NORMAL, 0, NULL);
        for (int i=0, xi=x+41; i<buf_sz; i++, xi++) {
                int color = 0;
                unsigned char txt = '.';
                switch(buf[i]) {
                case 0: // only difference from xxd; hide nulls as HexII does
                        txt = ' ';
                break;
                case ' ' ... '~':
                        color = 3;
                        txt = buf[i];
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
RF(hexii) { 
        for (int i=0, xi=x; i<buf_sz*2; i+=2, xi+=2) {
                uint8_t c = *(uint8_t*)(buf + i/2);

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
                        clr = 0 + pal->gray[8 + (c >> 4)];
                }
                mvprintw(y, xi, fmt, arg);
                mvchgat(y, xi, 2, A_NORMAL, clr, NULL);
        }
}

/* Color potential ARM Cortex ROM/RAM pointers */
RF(dw_ptr) {
        uint32_t dw = *(uint32_t*)buf;
        bool is_data = (dw & 3) == 0 && dw >= 0x20000000 && dw <= 0x3FFFffff;
        bool is_code = (dw & 3) == 1 && dw > 0 & dw <= 0x1FFFffff;
        mvprintw(y, x, "%08x", dw);
        mvchgat(y, x, 8, A_NORMAL, is_code ? 1 : (is_data ? 3 : 7), 0);
}

RF(bits) {
        // assumes 8 bits per char
        // please do not run on obscure DSPs
        assert(buf_sz <= 2 && "bits must fit into one hex char");
        for(int i=0; i<buf_sz*8; i++) {
                int bit = (buf_sz*8 - 1) - i;
                if ((buf[bit / 8] >> (bit % 8)) & 1) {
                        mvprintw(y, x+i, "%x", bit);
                }
        }
}


const view default_views[] = {
/*      {y, x, bs, fn,         name},   */
        {1, 1, 64, render_ent, "entropy", .needs = F_256C | F_WPAIRS, .min_bytes=16, .max_bytes=128},
//        {1, 1, 32, render_ent, "ent32"},
//        {1, 1, 16, render_ent, "ent/16", .needs = F_256C | F_WPAIRS},
        {9, 17, 1024, render_hist_square, "bytehist", .needs = F_256C | F_WPAIRS | F_UTF8},
        {1, 65, 128, render_byte_pixels, "bytepix", .needs = F_256C | F_WPAIRS | F_UTF8},
//        {1, 129, 256, render_byte_pixels, "pixbytes", .needs = F_256C | F_WPAIRS | F_UTF8},
//        /* currently, wide views cause an error if the screen's too narrow */
        {1, 9, 4, render_hexii, "HexII", .needs = F_256C},
        {1, 3, 1, render_hexii, "HexII", .needs = F_256C},
        {1, 59, 16, render_xxd, "xxd"},
        {1, 9, 4, render_dw_ptr, "arm-ptr"},
        {1, 8, 1, render_bits, "bits"},
        {0}, // last
};



int main(const int argc, char** argv) {
        return blindsight(argc, argv, default_views, "default_views");
}
