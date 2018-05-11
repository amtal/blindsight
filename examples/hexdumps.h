/* Hexdumps with various amounts of flare and formatting. */


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
