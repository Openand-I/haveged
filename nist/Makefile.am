## Process this file with automake to produce Makefile.in.

check_PROGRAMS = nist

AM_CFLAGS=-Wall

nist_LDADD = -lm

nist_SOURCES = cephes.c dfft.c matrix.c nist.c packtest.c special-functions.c cephes-protos.h matrix.h mconf.h special-functions.h nist.h

CLEANFILES = sample nist.out

MAINTAINERCLEANFILES = Makefile.in
