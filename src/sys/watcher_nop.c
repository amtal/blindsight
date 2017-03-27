#include <err.h>
#include <stdbool.h>
#include "watcher.h"

bool watcher_init(char* prog_name, bool blocking) {
        return false;
}

bool watcher_check() {
        return false;
}

void* watcher_reload(const char* sym, void* st, 
                void error(void* st, const char* msg)) {
        errx(1, "watcher_reload() unsupported in current build");
}
