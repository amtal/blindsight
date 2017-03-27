#!/usr/bin/tcc -run -lblindsight -lcapstone
#include "blindsight.h"
#include <ncurses.h>
#include <string.h>
#include <capstone/capstone.h>

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
        {1, 5, 2, render_dasm, "dasm-thumb"},
        {0}, // last
};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, views, "views");
}
