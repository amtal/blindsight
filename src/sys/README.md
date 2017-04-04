File monitoring on Linux uses inotify. Could be implemented on OS X via
FSEvents or a common wrapper thereof, if needed.

The sandbox attempts to use seccomp, but is pretty ghetto and probably fragile
to whims of glibc/ncursesw.
