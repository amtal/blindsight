[blindsight.c](blindsight.c) is a a general-purpose binary viewer. It's a prototype for what a standalone tool might look like, if it's ever stabilized and packaged with the library. The views it implements are arranged roughly by level-of-detail. Currently, half of them require a terminal and `ncurses` that supports >256 color pairs.

Other example viewers are simple and single-purpose:

- [dasm.c](dasm.c) - uses an external library (Capstone) for more info
- [tlb.c](tlb.c) - human-readable ARM TLB dumps for a CTF challenge

Individual views are provided as examples in [hexdumps.h](hexdumps.h), 
[summaries.h](summaries.h), and [bits.h](bits.h). You can copy-paste from them
as starting points, or just `#include <blindsight/bits.h>` directly into your
viewer.
