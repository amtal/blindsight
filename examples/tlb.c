#!/usr/bin/tcc -run -L/usr/local/lib -lblindsight
#include "blindsight.h"
#include <ncurses.h>

/* QEMU Cortex-A15 first-level translation table rwx-markup.
 *
 * Port of equivalent Python script from Boston Key Party's 2017 barewithme
 * challenge. Usage from gdb:
 *
 *      x/258 0x2a00000
 *      dump binary tlb.bin 0x2a00000 0x2a00408
 *      shell ./tlb.c tlb.bin
 */
VIEW(a15_TLB,
        4, /*=>*/ {1, 23}
)(uint8_t* s, size_t n, /*=>*/ int y, int x) {
        uint32_t w = *(uint32_t*)s;
        if (!w) return;  // leave unused entries blank

        // Parse problem-relevant bits only:
        uint32_t addr = w & 0xFFFFf000;
        uint32_t mask = w & 0x00000fff;
        char* mode = "?? ??";
        switch (mask & 0xc00) {
                case 0x000: mode = "-- --"; break;
                case 0x400: mode = "rw --"; break;
                case 0x800: mode = "rw r-"; break;
                case 0xc00: mode = "rw rw"; break;
        }
        char* exe = mask & 0x010 ? "-" : "x";

        /* Print supervisor and user rw access, with a static-state hack to
         * only print info that changed. (Rendering is always sequential.) */
        static uint32_t last = 0;
        char* fmt = mask == last ? "0x%08x *   *  *  *" : "0x%08x %03x %s %s";
        last = mask;

        // Highlight userland-reachable pages:
        int color = 0;
        switch (mask & 0xc00) {
                // rwx:red, r-x:green
                case 0xc00: color = mask & 0x010 ? 2 : 1; break;
                // r--:yellow
                case 0x800: color = 3; break;
        }

        mvprintw(y, x, fmt, addr, mask, mode, exe);
        mvchgat(y, x, 23, A_NORMAL, color, 0);
        /* 0x02c00000 802 rw r- x 0x02d00000 *   *  *  *
         * 0x06000000 c03 rw rw x 0x06100000 *   *  *  *
         * 0x06400000 c13 rw rw - 0x06500000 *   *  *  */
}

view* views[] = {&a15_TLB, 0};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, views, "views");
}
