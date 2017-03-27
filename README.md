Portable hex viewer focused on visual pattern matching on <1MB binaries.
Trivial to extend with custom markup, supports live editing of markup.

Binary Editor BZ has this covered on Windows, this is a single-file Unix
version you can quickly extend. Because, to quote a neat scifi novel, 
"We're the goddamned Kalashnikovs of thinking meat."

This is a hex viewer, not editor.  I've spent enough of my life staring at
unformatted hexdumps to hold a religious belief; hex viewers are a starting
tool you need to use to build better tools. Don't edit bytes with a hex editor;
implement a partial format parser/pretty-printer, and use that. Struct features
in hex editors make do, but I always prefer something programmatic and nicely
version controlled.

# Build and Install

~~~
./configure
make
sudo make install

examples/bs.c
~~~
