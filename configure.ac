dnl The file watcher that uses TCC to recompile and reload render code when it
dnl changes is some fragile, non-portable stuff. Testing dependencies and working
dnl around API breakage/platform differences is the gnarly problem autotools
dnl evolved for, so here it is.
dnl
dnl The fragile file-watcher-reloader code is the *only* reason autotools is in
dnl use. All the other bells and whistles it adds may be cool, but aren't worth
dnl the incidental complexity brought into a tool that's quite manageable as a
dnl one-file bash/C polyglot.
dnl
dnl Without it, this could literally be a single C file you wget and tcc -run.
dnl
dnl Serious second thoughts about going down the path at all.
AC_INIT([blindsight],[0.0.5],[amtal@github])

dnl ################################
dnl Automake dependencies and config
dnl ################################

dnl Reduce the amount of clutter in project rootdir.
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([build-aux]) # complains about not being named m4??

dnl The 'foreign' option avoids extra Gnu complaints.
AM_INIT_AUTOMAKE([1.9 -Wall -Werror foreign subdir-objects])

dnl #########################
dnl Build System Dependencies
dnl #########################

dnl There's a weird dep on GCC as compiler right now due to stack alignment
dnl issues when called by TCC. One day...
dnl AC_PROG_CC(clang gcc tcc)
AC_PROG_CC(gcc)
AC_PROG_CC_C99
dnl /usr/share/automake-1.15/am/ltlibrary.am: 
dnl warning: 'libblindsight.la': linking libtool libraries using a non-POSIX
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # magic incantation from some github issue
LT_INIT([shared])
AC_PROG_LIBTOOL

dnl #################################
dnl Main Library and API Dependencies
dnl #################################

AC_CONFIG_HEADERS([src/config.h]) # avoid -D clutter in build log

AC_CHECK_FUNCS([err errx], [], [AC_MSG_ERROR([Missing BSD convenience wrapper.])])

dnl There's a pile of other linuxy stuff I don't think is portable, but eh, later.
dnl
dnl Is there any point to calling these if nothing checks their output? It seems
dnl nice to have some output on build that indicates a dependency, but means
dnl there's a pile of extra -DHAVE_WHATEVER passed around and this configure.ac
dnl is longer than necessary.
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_FUNC_MMAP

dnl Pin to at-least ncurses versions I've tested, for now.
PKG_CHECK_MODULES([NCURSESW], [ncursesw >= 5.9], [], [
    AC_MSG_WARN([Untested version of ncursesw.])
]) 
AC_SEARCH_LIBS([mvaddwstr], [ncursesw], , [
    AC_MSG_ERROR([Need ncursesw, for wide char and color pair support.])
])
AC_CHECK_HEADERS([ncursesw/ncurses.h]) # Ubuntu layout, needs ifdef


dnl #####################
dnl Optional Dependencies
dnl #####################

dnl Live code update:

dnl Do an initial basic check for compiler binary, then a bunch of extra checks.
dnl Once all checks done, create the conditional that will be used to tweak
dnl generated Makefile.
AC_CHECK_PROG([tcc],[tcc],[yes],[no])
AC_CHECK_HEADERS([libtcc.h], , [tcc=no])

dnl libtcc depends on libdl, SEARCH_LIBS check will fail if it's not added. Could
dnl add -ldl to arguments, or can search for [dl] first since autotools seems to
dnl append a running list of -lfoo for every library found. Which breaks future
dnl SEARCH_LIBS calls if you add -ldl to this SEARCH_LIBS arguments, because
dnl future ones will have -ltcc but no -ldl.
dnl Why am I doing this again?
AC_SEARCH_LIBS([dlopen], [dl], , [tcc=no])
dnl Debian ships only libtcc.a, I want to build a .so, and am too lazy to figure
dnl out the right way to do this. So, let's just make sure the system has a .so
dnl by adding -shared to the build-test.
AC_SEARCH_LIBS([tcc_new], [tcc], , [tcc=no], [-shared])

AC_MSG_CHECKING([if tcc_relocate is the okay one])
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM(
        [[#include <libtcc.h>]],
        [[TCCState* tcc;
          tcc_relocate(tcc, 0);
          #ifdef TCC_RELOCATE_AUTO
          #error auto-allocation re-added, everything is fine
          #endif
        ]]
    )],
    [AC_MSG_RESULT([no])
     AC_MSG_WARN([Unsupported 0.9.25 tcc_relocate detected])
     tcc=no
    ],
    [AC_MSG_RESULT([yes])]
)

dnl Enough checks for a working TCC. 
AM_CONDITIONAL([HAVE_TCC], [test "x$tcc" = xyes])
AM_COND_IF([HAVE_TCC],,[
    AC_MSG_WARN([Disabled live code reload due to missing TCC.])
])

dnl Look for platform-specific directory/file watchers:

AC_CHECK_HEADERS([sys/inotify.h], [linux=yes], [linux=no])
AM_CONDITIONAL([SYS_LINUX], [test "x$linux" = xyes])

dnl Sandbox, Linux-only currently:

AC_ARG_ENABLE([sandbox], 
[AS_HELP_STRING([--enable-sandbox], [enforce sandbox where available])],
[
  case "${enableval}" in
   yes)   sandbox=yes ;;
   no)    sandbox=no ;; 
   *) AC_MSG_ERROR([bad value ${enableval} for --enable-sandbox]) ;;
  esac

], [sandbox=yes])

dnl For some reason SEARCH_LIBS links -ltcc -lncursesw, which fails due to tcc
dnl depending on dl. Guess I'll check for dl.
AC_SEARCH_LIBS([seccomp_init], [seccomp], , [sandbox=no])
AC_CHECK_HEADERS([seccomp.h], , [sandbox=no])

dnl Until this feature proves stable across platforms, fail-silent for when it's
dnl missing due to broken dependency. Warn when it's enabled to give a heads up
dnl on imminent SIGSYS.
AM_CONDITIONAL([HAVE_SANDBOX], [test "x$sandbox" = xyes])
AM_COND_IF([HAVE_SANDBOX],[
    AC_MSG_WARN([Ghetto sandbox is enabled, get ready to strace crashes!])
])


dnl Cross fingers and generate makefiles.
AC_CONFIG_FILES([Makefile 
                 src/Makefile])
AC_OUTPUT
