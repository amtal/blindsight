#ifndef SANDBOX_H
#define SANDBOX_H 1

/* After initialization, the viewer is likely limited to doing
 * terminal-ncurses-stuff, and maybe reading the input stream if I ever get
 * around to implementing something other than direct mmap.
 *
 * Dropping syscall privs via seccomp/pledge before parsing of the input file
 * starts seems like a low-effort no-brainer. The whole point of this tool is
 * writing quick-and-dirty parsing code and running it on whatever. While the
 * fixed-size window constraint limits bug classes that'll actually occur,
 * a ghetto BPF policy will limit future CTF fun.
 */
void sandbox_init();

#endif

