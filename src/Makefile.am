sbin_PROGRAMS = haveged

AM_CFLAGS=-Wall -I..

haveged_SOURCES = haveged.c havege.c havegetune.c havegecollect.c \
                  cpuid-43.h haveged.h havege.h havegecollect.h oneiteration.h

haveged_LDADD = @HA_LDFLAGS@

MAINTAINERCLEANFILES = Makefile.in
