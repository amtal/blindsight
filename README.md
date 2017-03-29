High-density hex viewer focused on visual pattern matching on <1MB binaries.

[Binary Editor BZ](https://github.com/devil-tamachan/binaryeditorbz) has this
covered on Windows. This is a Unix version you can quickly extend and prototype
on. Supports live code reload for smooth domain-specific prototyping.

# Design Rationale

> "Cause still unknown after several thousand engineering-hours of review. Now
> parsing data with a hex editor to recover final milliseconds."

Hexdumps are the lowest-level debug tool in software. They'll never go away,
but as shown by [Corkami](https://github.com/corkami/pics/tree/master/binary)'s
dissections they might get fancier.

> "We're the goddamned Kalashnikovs of thinking meat."

This is a hex *viewer*. It's here to encourage and assist the part of the brain
that sees animals in clouds and patterns in bespoke ciphertexts.  Use it to do
initial reconnaissance, seed whimsical hypotheses, and test them swiftly. 

Stateful operations like edits on binary formats should be formalized in
version controlled tools, not shot from the hip and left undocumented. Your end
goal should be elegant domain-specific parsers and pretty-printers, not some
kludge caged in a power-of-2 aligned grid. See
[Scapy](https://www.secdev.org/projects/scapy/),
[Nom](https://crates.io/crates/nom), or
[Parsec](https://wiki.haskell.org/Parsec) and its
[variations](https://hackage.haskell.org/package/trifecta) as examples.

# Installing

Mandatory: `ncursesw`, `autotools`, C build environment, Tiny C Compiler.
Optional: `libtcc.so` on Linux.

```bash
./bootstrap
./configure
make
sudo make install
```

# Using

Run some examples (like `./examples/bs.c`) which are all C files executable via
the Tiny C Compiler. 

You can write your own, copy-pasting liberally from examples. Saved code will
be re-loaded by running instances on supported platforms.


```c
#!/usr/bin/tcc -run -L/usr/local/lib -lblindsight
#include "blindsight.h"
#include <ncurses.h>

/* vim's xxd.c in default mode */
RF(xxd) {
        for (int i=0, xi=x; i<buf_sz; i+=2, xi+=5) {
                mvprintw(y, xi, "%02x%02x ", buf[i], buf[i+1]);
        }
        for (int i=0, xi=x+41; i<buf_sz; i++, xi++) {
                unsigned char c = buf[i];
                mvaddch(y, xi, ' ' <= c && c <= '~' ? buf[i] : '.');
        }
        mvchgat(y, x, 57, A_NORMAL, 0, NULL);
}

const view views[] = {
/*      {y, x,  bs, fn,         name},   */
        {1, 59, 16, render_xxd, "xxd"},
        {0}, // last
};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, views, "views");
}
```

