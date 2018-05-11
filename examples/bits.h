/* Displays for spotting bit-level patterns. */


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
