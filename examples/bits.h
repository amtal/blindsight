/* Displays for spotting bit-level patterns. */


/* Print positions at which bits are set, with a nice gray sweep. */
VIEW(bits, 1, /*=>*/ {1, 8})(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        // assumes 8 bits per char, please do not run on obscure DSPs
        assert(n <= 2 && "bits must fit into one hex char");
        for(int i=0; i<n*8; i++) {
                int bit = (n*8 - 1) - i;
                if ((s[bit / 8] >> (bit % 8)) & 1) {
                        mvprintw(y, x+i, "%x", bit);
                        mvchgat(y, x+i, 1, A_NORMAL, pal.gray[25-i*2], NULL);
                }
        }
}

/* Show bits via 8-dot unicode Braille. */
VIEW(braille, 
        1, /*=>*/ {1, 1, F_256C|F_UTF8}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        assert(n <= 2 && "bits must fit into one hex char");
        /* The Unicode braille symbols are ordered a little weird, since
         * there's 8 dots but Braille has 6. The following remaps:
         *
         * 01 08    01 02
         * 02 10 -\ 04 08
         * 04 20 -/ 10 20
         * 40 80    40 80
         */
        uint8_t u = s[0];
        u = u&0xe1 | (u&2)<<2 | (u&4)>>1 | (u&8)<<1 | (u&16)>>2;

        wchar_t byte[2] = {0x2800 + u, 0};
        mvaddwstr(y, x, (wchar_t*)&byte);

        mvchgat(y, x, 1, A_NORMAL, pal.gray[8 + (s[0] >> 4)], NULL);
}
