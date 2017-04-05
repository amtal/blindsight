AM_CCFLAGS = -Wall -O3

## TCC doesn't do stack alignment, and I've been returning laaarge structs on
## it. Calling into clang faults on movapd due to non-16byte aligned SP.
##
## Stack alignment for clang (-mstackrealign -mstack-alignment=4) don't seem to
## change behavior. Switching to GCC, where preferred-stack-boundary (argument
## in of bits to mask off to 0, unlike clang's stride) works.
##
## Can't use TCC as compiler for everything because its linker doesn't handle
## autotools default arguments.
## 
## So, we're limited to building library with GCC for now.
if HAVE_TCC
AM_CCFLAGS += -mpreferred-stack-boundary=2
##AM_CCFLAGS += -mstackrealign -mstack-alignment=4
endif

include_HEADERS = blindsight.h

lib_LTLIBRARIES = libblindsight.la
libblindsight_la_SOURCES = blindsight.c palette.c cmd.c \
						   palette.h cmd.h watcher.h sandbox.h
libblindsight_la_LIBADD = 

if HAVE_TCC
if SYS_LINUX
    libblindsight_la_SOURCES += sys/watcher_inotify.c
    libblindsight_la_LIBADD += -ltcc
else # unknown platform
    libblindsight_la_SOURCES += sys/watcher_nop.c
endif # SYS_*
else
    libblindsight_la_SOURCES += sys/watcher_nop.c
endif # HAVE_TCC

if HAVE_SANDBOX
    libblindsight_la_SOURCES += sys/sandbox_seccomp.c
    libblindsight_la_LIBADD += -lseccomp
else
    libblindsight_la_SOURCES += sys/sandbox_nop.c
endif # SANDBOX

libblindsight_la_CFLAGS = -Wall
libblindsight_la_LDFLAGS = -export-symbols-regex '(blindsight|blindsight_version)'
