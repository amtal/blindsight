#!/usr/bin/tcc -run -L/usr/local/lib -lblindsight
#include <blindsight.h>
#include <assert.h>
#include <math.h>
#define _XOPEN_SOURCE_EXTENDED
// ^ pray to mr skeltal and open source gods
#include <ncurses.h>

#include <blindsight/summaries.h>
#include <blindsight/hexdumps.h>
#include <blindsight/bits.h>

view* default_views[] = {
        &entropy,
        &bytehist,
        &bytepix,
        &braille,
        &HexII_DW,
        &HexII,
        &xxd,
        &bits,
        0
};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, default_views, "default_views");
}
