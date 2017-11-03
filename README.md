High-density hex viewer focused on visual pattern matching on <1MB binaries.

[Binary Editor BZ](https://github.com/devil-tamachan/binaryeditorbz) has this
covered on Windows. This is a Unix version you can quickly extend and hack
on. Supports live code reload for smooth domain-specific prototyping.

![Quick usage demonstration on /bin/ls](https://i.imgur.com/lYa5KVX.png)

# Installing

On Arch, run `makepkg -si` in [pkg/archlinux/](pkg/archlinux/). On OSX, there's a [Homebrew formula](pkg/homebrew/blindsight.rb) you can add to your personal tap and `brew install`. 

Elsewhere, ensure that you have `ncursesw`, `autotools`, the Tiny C Compiler, and a GCC-based build environment. You may want to install TCC from source (it's small) to enable live code reload. As usual, `./bootstrap && ./configure && make` then `sudo make install`.

[![Travis](https://img.shields.io/travis/amtal/blindsight.svg)](https://travis-ci.org/amtal/blindsight) [![Codacy grade](https://img.shields.io/codacy/grade/e8b2d157ee3448f4ac050e586aa085c4.svg)](https://www.codacy.com/app/amtal/blindsight/dashboard) [![license](https://img.shields.io/github/license/amtal/blindsight.svg)](LICENSE)

# Using

Run some examples (like [examples/bs.c](examples/bs.c#L242)) which are all C
scripts executable via the Tiny C Compiler's `-run` option.  For fast
iteration, saved code will be automatically re-loaded by running hex viewers on
supported platforms. 

![Live code reload demonstration](https://i.imgur.com/XXob133.gif)

You can write your own views that make use of C libraries, such as the
Capstone-based disassembly view in [examples/dasm.c](examples/dasm.c).
Copy-paste liberally from other examples.


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
/*       bytes       y, x    name   func   */
        {16, /*=>*/ {1, 59}, "xxd", rf_xxd},
        {0}, // last
};

int main(const int argc, char** argv) {
        return blindsight(argc, argv, views, "views");
}
```

This should be useful for one-off CTF tools and experiments during reverse engineering.

![Simple page permission viewer on raw TLB dumps](https://i.imgur.com/hoLSY8z.gif)

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
kludge caged in a power-of-2 aligned grid. See [Scapy], [Nom], or [Parsec] and
its [variations] as examples.

[Scapy]: https://www.secdev.org/projects/scapy/
[Nom]: https://crates.io/crates/nom
[Parsec]: https://wiki.haskell.org/Parsec
[variations]: https://hackage.haskell.org/package/trifecta

