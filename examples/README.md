[bs.c](bs.c) is a a general-purpose binary viewer. It's a prototype for what a
standalone tool might look like, if it's ever stabilized and packaged with the
library. The views it implements are arranged roughly by level-of-detail.

Other examples are simple and single-purpose:

- [dasm.c](dasm.c) - uses an external library (Capstone) for more info
- [tlb.c](tlb.c) - human-readable ARM TLB dumps for a CTF challenge
