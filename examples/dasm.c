#!/usr/bin/tcc -run -L/usr/local/lib -lblindsight -lcapstone
#include "blindsight.h"
#include <ncurses.h>
#include <string.h>
#include <capstone/capstone.h>

/* Color potential ARM Cortex ROM/RAM pointers */
RF(dw_ptr) {
        uint32_t dw = *(uint32_t*)buf;
        bool is_data = (dw & 3) == 0 && dw >= 0x20000000 && dw <= 0x3FFFffff;
        bool is_code = (dw & 3) == 1 && dw > 0 & dw <= 0x1FFFffff;
        mvprintw(y, x, "%08x", dw);
        mvchgat(y, x, 8, A_NORMAL, is_code ? 1 : (is_data ? 3 : 7), 0);
}

RF(dasm) {
        csh hnd;
        cs_insn *isn;

        cs_open(CS_ARCH_ARM, CS_MODE_THUMB, &hnd);
        size_t count = cs_disasm(hnd, (uint8_t*)buf, 2, 0x8000000, 0, &isn); 

        if (count) {
                mvprintw(y, x, isn[0].mnemonic);
                int color = strcmp(isn[0].mnemonic, "stm") ? 0 : 1;
                mvchgat(y, x, 5, A_NORMAL, color, NULL);

                cs_free(isn, count);
        }
        cs_close(&hnd);
}

const view views[] = {
        {4, /*=>*/ {1, 9}, "arm-ptr", rf_dw_ptr},
        {2, /*=>*/ {1, 5}, "dasm-thumb", rf_dasm},
        {0}, // last
};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, views, "views");
}
