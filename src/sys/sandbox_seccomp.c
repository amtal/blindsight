/* This sandbox is kind of sketch due to being bolted on. Not from a security
 * point of view, but a stability one. The only system it's been tested on
 * identified some edge cases that can't possibly be the full extent of what
 * ncursesw/libc will eventually call.
 *
 * Whatever script launches the program can mess with things by rebinding file
 * descriptors/mapping in shared memory to a vulnerable process or whatever.
 * Sandbox assumes a pretty minimal/sane situation, and is meant to protect
 * against lazy bad code, not cartoon villain bad code.
 */
#include <err.h>
#include <seccomp.h>
#include <unistd.h>
#include "sandbox.h"

#define ALLOW(ctx, sc) seccomp_rule_add(ctx, SCMP_ACT_ALLOW, sc, 0)

/* Final tally and security argument:
 *
 * - Code can open + read whatever, and mmap + mprotect whatever.
 * - Code can only write to file descriptors 1 and 2.
 *
 * Therefore:
 *
 * - Confidentiality: attacks limited to covert channels and dumb stuff
 *   like /dev/tcp, which thankfully is a bash-ism not a Linux-ism.
 * - Integrity: as long as there's no way to remap 1/2 to something other
 *   than STDFOO, should be fine. I don't know of one on Linux. Some
 *   privesc attack surface, but not that much - although everything
 *   needed for rowhammer is there. :)
 * - Availability: memory/CPU DOS trivial, but lame.
 *
 */
void sandbox_init() {
        scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
        if (!ctx) errx(1, "seccomp_init failed!");

        int sc = 0; /* error accumulator */

        /* normal I/O */
        sc |= ALLOW(ctx, SCMP_SYS(read)); // STDIN || mapped file, allowing all
        sc |= seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
                        SCMP_A0(SCMP_CMP_EQ, STDOUT_FILENO));
        sc |= seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
                        SCMP_A0(SCMP_CMP_EQ, STDERR_FILENO));
        /* file polling */
        sc |= ALLOW(ctx, SCMP_SYS(poll)); // fds is an array, can't inspect
        sc |= ALLOW(ctx, SCMP_SYS(gettimeofday));
        sc |= ALLOW(ctx, SCMP_SYS(rt_sigaction)); //SIGTSTP

        /* iconv funtimes */
        sc |= ALLOW(ctx, SCMP_SYS(open)); // O_RDONLY|O_CLOEXEC, but needed for TCC anyway
        sc |= ALLOW(ctx, SCMP_SYS(fstat64)); // on the above?
        sc |= ALLOW(ctx, SCMP_SYS(close));

        /* memory management; my code is malloc-clean - dependencies aren't */
        sc |= ALLOW(ctx, SCMP_SYS(brk)); 

        sc |= ALLOW(ctx, SCMP_SYS(sigreturn)); // from SIGWINCH

        /* clean exit */
        sc |= ALLOW(ctx, SCMP_SYS(ioctl)); // SNDCTL_TMR_STOP or TCSETSW??
        sc |= ALLOW(ctx, SCMP_SYS(munmap));
        sc |= ALLOW(ctx, SCMP_SYS(exit_group));
        sc |= ALLOW(ctx, SCMP_SYS(rt_sigprocmask)); // abort
        sc |= ALLOW(ctx, SCMP_SYS(exit)); // not used anymore on glibc/musl...

        /* live code reloads hit a bunch of stuff, of course */
        sc |= ALLOW(ctx, SCMP_SYS(mmap2)); // only hit on TCC reloads
        sc |= ALLOW(ctx, SCMP_SYS(mmap)); // adding for portability?
        sc |= ALLOW(ctx, SCMP_SYS(_llseek));
        sc |= ALLOW(ctx, SCMP_SYS(mprotect)); // yeahhhh...
        /* it's tempting to disable this w/o TCC, but then I see a state
         * explosion on testing on the horizon... */

        if (sc) errx(128+sc, "seccomp rule addition failed");
        if (seccomp_load(ctx)) errx(1, "seccomp_load");
        seccomp_release(ctx);
}
