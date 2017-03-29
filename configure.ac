# The file watcher that uses TCC to recompile and reload render code when it
# changes is some fragile, non-portable stuff. Testing dependencies and working
# around API breakage/platform differences is the gnarly problem autotools
# evolved for, so here it is.
#
# The fragile file-watcher-reloader code is the *only* reason autotools is in
# use. All the other bells and whistles it adds may be cool, but aren't worth
# the incidental complexity brought into a tool that's quite manageable as a
# one-file bash/C polyglot.
#
# Without it, this could literally be a single C file you wget and tcc -run.
#
# Serious second thoughts about going down the path at all.
AC_INIT([blindsight],[0.0.4],[amtal@github])

##
## Automake dependencies and config.
##

# Reduce the amount of clutter in project rootdir.
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([build-aux]) # complains about not being named m4??

# The 'foreign' option avoids extra Gnu complaints.
AM_INIT_AUTOMAKE([1.9 -Wall -Werror foreign subdir-objects])


##
## Build system dependencies.
##

#AC_PROG_CC(clang gcc tcc)  # can run on just tcc, but I trust big ones more :|
AC_PROG_CC(gcc)  # hahaha nevermind, calling into clang's xmm ops /w 16byte stack alignment crashes :D
AC_PROG_CC_C99
#AC_PROG_LIBTOOL

# /usr/share/automake-1.15/am/ltlibrary.am: 
# warning: 'libblindsight.la': linking libtool libraries using a non-POSIX
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # magic incantation from some github issue
LT_INIT([shared])


##
## Main library and API dependencies.
##

AC_CHECK_FUNCS([err errx], [], [AC_MSG_ERROR([Missing BSD convenience wrapper.])])

# There's a pile of other linuxy stuff I don't think is portable, but eh, later.
#
# Is there any point to calling these if nothing checks their output? It seems
# nice to have some output on build that indicates a dependency, but means
# there's a pile of extra -DHAVE_WHATEVER passed around and this configure.ac
# is longer than necessary.
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_FUNC_MMAP

# Pin to at-least ncurses versions I've tested, for now.
PKG_CHECK_MODULES([NCURSESW], [ncursesw >= 5.9], [], [
    AC_MSG_WARN([Untested version of ncursesw.])
]) 
AC_SEARCH_LIBS([mvaddwstr], [ncursesw], , [
    AC_MSG_ERROR([Need ncursesw, for wide char and color pair support.])
])
AC_CHECK_HEADERS([ncursesw/ncurses.h]) # Ubuntu layout, needs ifdef

# TODO sort out ncurses.h/ncursesw header location across platforms


##
## Live code update dependencies.
##

# Do an initial basic check for compiler binary, then a bunch of extra checks.
# Once all checks done, create the conditional that will be used to tweak
# generated Makefile.
AC_CHECK_PROG([tcc],[tcc],[yes],[no])
AC_CHECK_HEADERS([libtcc.h], , [tcc=no])
# Debian ships only libtcc.a, I'm want an .so VVVVVVV cuz I'm lazy
AC_SEARCH_LIBS([tcc_new], [tcc], , [tcc=no], [-shared -ldl])

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

AM_CONDITIONAL([HAVE_TCC], [test "x$tcc" = xyes])
AM_COND_IF([HAVE_TCC],,[
    AC_MSG_WARN([Disabled live code reload due to missing TCC.])
])

# Enough checks for a working TCC. 
# Look for platform-specific directory/file watchers.
AC_CHECK_HEADERS([sys/inotify.h], [linux=yes], [linux=no])
AM_CONDITIONAL([SYS_LINUX], [test "x$linux" = xyes])


# Cross fingers and generate makefiles.
AC_CONFIG_FILES([Makefile 
                 src/Makefile])
AC_OUTPUT
