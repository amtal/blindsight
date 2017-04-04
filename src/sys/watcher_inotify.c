#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <limits.h>
#include <libgen.h> // basename/dirname
#include <stdbool.h>
#include <fcntl.h>
#include <libtcc.h>
#include "watcher.h"

static struct {
        /* public-ish: */
        const char* file_name;
        const char* dir_name;
        /* internals: */
        char real[PATH_MAX];
        int inotify_fd;
        bool blocking_mode;
} w = {0};

/* Just gonna assume PATH_MAX is enough, and to hell with anyone running on
 * filesystems where it's not. */

/* sanity check that we're under tcc, otherwise this is a dumb endeavor */
static bool am_a_script_running_in_tcc() {
        char exe[PATH_MAX];
        ssize_t len = readlink("/proc/self/exe", exe, PATH_MAX);
        if (-1 == len) err(1, "readlink");
        if (len > PATH_MAX - 1) {
                fprintf(stderr, "readlink memory corruption O_o\n");
                abort();
        }

        exe[len] = 0;
        return (0 != strstr(exe, "/bin/tcc"));
}

static void get_script_realpath(const char* argv_0, char* real) {
        /* best guess for abspath of script being run; do not run across trust
         * boundaries unless it would be funny */
        char script[PATH_MAX];
        {
                if (argv_0[0] == '/') {
                        snprintf(script, PATH_MAX, "%s", argv_0);
                } else {
                        char cwd[PATH_MAX];
                        if (!getcwd(cwd, PATH_MAX)) err(1, "getcwd");
                        snprintf(script, PATH_MAX, "%s/%s", cwd, argv_0);
                }
        }

        /* get to what inotify needs */
        if (!realpath(script, real)) err(1, "realpath");
}

bool watcher_init(char* prog_name, bool blocking) {
        if (!am_a_script_running_in_tcc()) return false;

        w.blocking_mode = blocking;

        get_script_realpath(prog_name, w.real);
        w.file_name = basename(w.real);
        w.dir_name = dirname(w.real);

        w.inotify_fd = inotify_init();
        if (w.inotify_fd == -1) err(1, "inotify_init");

        if (!blocking) {
                int flags = fcntl(w.inotify_fd, F_GETFL, 0);
                fcntl(w.inotify_fd, F_SETFL, flags | O_NONBLOCK);
        }

        /* Have to watch entire directory, since editors tend to delete files
         * as they save 'em */
        int in_wd = inotify_add_watch(w.inotify_fd, w.dir_name, IN_MODIFY);
        if (in_wd == -1) err(1, "inotify_add_watch");

        return true;
}

bool watcher_check() {
        char in_buf[sizeof(struct inotify_event) + NAME_MAX + 1];
        bool write_event = false;

        do {
                if (-1 == read(w.inotify_fd, in_buf, sizeof(in_buf))) {
                        if (EAGAIN == errno) { // queue exhausted, return
                                errno = 0; 
                                break;
                        }
                        err(1, "inotify read");
                }

                struct inotify_event* e = (struct inotify_event*)in_buf;

                if (e->mask & IN_MODIFY) {
                        if (e->len && !strcmp(e->name, w.file_name)) {
                                write_event = true;
                        }
                }
        } while (!w.blocking_mode); // exhaust queue in non-block mode

        return write_event;
}

void* watcher_reload(const char* sym, void* st, 
                void error(void* st, const char* msg)) {
        char fname[PATH_MAX];
        snprintf(fname, sizeof(fname), "%s/%s", w.dir_name, w.file_name);

        static TCCState* tcc;

        if (tcc) {
                //tcc_delete(tcc); // uh, let's try memory leaking and see where it gets us!
                tcc = 0;
        }

        tcc = tcc_new();
        if (!tcc) errx(1, "tcc_new() failed!\n");

        tcc_set_error_func(tcc, st, error);
        tcc_set_output_type(tcc, TCC_OUTPUT_MEMORY); // is default, but still...

        if (-1 == tcc_add_library(tcc, "blindsight"))  
                errx(1, "tcc_add_library(..) for self failed!\n");
        if (-1 == tcc_add_file(tcc, fname))  
                errx(1, "tcc_add_file(..) failed!\n");
        // TODO miscompilation probably shouldn't be fatal, just add error handling

        /* Recent versions of tcc_relocate add a 2nd argument.
         *
         * <=0.9.24 no argument, memory managed internally
         * ==0.9.25 pass 0 to return required size, pass buffer to fill it
         * >=0.9.26 pass 0 for required size, 1 for auto-alloc like in <=0.9.24
         *
         * Currently, arch + ubuntu don't have .25 so I'll ignore it.
         * Otherwise, need to add AC_COMPILE_IFELSE test to configure.ac
         * since tcc isn't in pkg-config for either test system.
         */
#ifdef TCC_RELOCATE_AUTO
        if (-1 == tcc_relocate(tcc, TCC_RELOCATE_AUTO))
                errx(1, "tcc_relocate/1 failed!\n");
#else
        if (-1 == tcc_relocate(tcc))
                errx(1, "tcc_relocate/2 failed!\n");
#endif

        return tcc_get_symbol(tcc, sym);
}
