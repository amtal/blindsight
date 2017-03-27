AM_CCFLAGS = -Wall -O3

# TCC doesn't do stack alignment, and I've been returning laaarge structs on
# it. Calling into clang faults on movapd due to non-16byte aligned SP.
# The following is a clang option only. On gcc it's
# -mpreferred-stack-boundary=n (n=number of bits to mask off to 0)
#
# Goddamn. Clang won't align, gcc won't either, O0 doesn't stop generating xmm
# instructions on the stack, and tcc's linker doesn't handle autotools
# arguments. Ffffffffffuck.
# 
# So, we're limited to a GCC library for now.
if HAVE_TCC
#AM_CCFLAGS += -mstackrealign -mstack-alignment=4
AM_CCFLAGS += -mpreferred-stack-boundary=2
endif

include_HEADERS = blindsight.h

lib_LTLIBRARIES = libblindsight.la
libblindsight_la_SOURCES = blindsight.c watcher.h
libblindsight_la_LIBADD = -lncursesw -lm
if HAVE_TCC
if SYS_LINUX
    libblindsight_la_SOURCES += sys/watcher_linux.c
    libblindsight_la_LIBADD += -ltcc
else # unknown platform
    libblindsight_la_SOURCES += sys/watcher_nop.c
endif # SYS_*
else
  libblindsight_la_SOURCES += sys/watcher_nop.c
endif # HAVE_TCC
libblindsight_la_CFLAGS = -Wall
libblindsight_la_LDFLAGS = -export-symbols-regex '(blindsight|blindsight_version)'

## VVV Below is just a test; no point building a binary yet, until hex editor
##     views are *thoroughly* explored.
#bin_PROGRAMS = bs
#bs_SOURCES = bs.c
#bs_LDADD = -lcapstone libblindsight.la -lm -ltcc -ldl
