#ifndef WATCHER_H
#define WATCHER_H 1
#include <stdbool.h>

/* Singleton file-watcher-and-reloader. Assumes main program is a
 * #!/usr/bin/tcc -run shellscript. Returns false for everything otherwise.
 *
 * prog_name:   program's argv[0], used to detect reload path
 * blocking:    watcher_check() blocks until a write
 *
 * returns false if not a script running under /bin/tcc */
bool watcher_init(char* prog_name, bool blocking);

/* true if file was written to since last call */
bool watcher_check();

/* reload file and return a single symbol from it */
void* watcher_reload(const char* sym, void* st, 
                     void error(void* st, const char* msg));

#endif
