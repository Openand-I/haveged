## Process this file with autoconf to produce a configure script.

## Minimum Autoconf version
AC_PREREQ(1.9)

AC_INIT([haveged],[1.4])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([/src/haveged.c])

HA_LDFLAGS=""

## Make init script configurable
AC_ARG_VAR(HA_DISTRO,[Init script template [default=redhat.in]])
if test "x$HA_DISTRO" = "x"; then
   HA_DISTRO="redhat.in"
fi

## Make capture diagnostic configurable
AC_ARG_ENABLE(capture, AS_HELP_STRING([--enable-capture=[no/yes]],[Enable capture diagnostic [default=no]]),, enable_capture="no")
if test "x$enable_capture" = "xyes"; then
   AC_DEFINE(RAW_OUT_ENABLE, 1, [Define to 1 for capture diagnostic])
fi

## Make build w/o daemon interface configurable
AC_ARG_ENABLE(daemon, AS_HELP_STRING([--enable-daemon=[yes/no]],[Enable daemon [default=yes if linux]]),, enable_daemon="x" )

## Make inject diagnostic configurable
AC_ARG_ENABLE(inject, AS_HELP_STRING([--enable-inject=[no/yes]],[Enable inject diagnostic [default=no]]),, enable_inject="no")
if test "x$enable_inject" = "xyes"; then
   AC_DEFINE(RAW_IN_ENABLE, 1, [Define to 1 for injection diagnostic])
fi

## Make inject diagnostic configurable
AC_ARG_ENABLE(threads, AS_HELP_STRING([--enable-threads=[no/yes]],[Enable threads [default=no]]),, enable_threads="no")
if test "x$enable_threads" = "xyes"; then
   AC_DEFINE(NUMBER_CORES, 4, [Define maxium number of collection threads])
   HA_LDFLAGS="-pthread"
else
   AC_DEFINE(NUMBER_CORES, 1, [Define to single collection thread])
fi


## Make nist self-test configurable
AC_ARG_ENABLE(nistest, AS_HELP_STRING([--enable-nistest=[no/yes]],[Run NIST test suite [default=no]]),, enable_nistest="no")
if test "x$enable_nistest" = "xyes"; then
   HA_CHECK="ent/test.sh nist/test.sh"
else
   HA_CHECK="ent/test.sh"
fi
AC_SUBST(HA_CHECK,$HA_CHECK)

## Checks for programs.
AC_PROG_CC([gcc])

## Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

## Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([__rdtsc clock_gettime floor gettimeofday memset pow select sched_yield sqrt])

## Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(sched.h)
AC_CHECK_HEADERS(semaphore.h)
AC_CHECK_HEADERS(stdlib.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/mman.h)
AC_CHECK_HEADERS(sys/wait.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(cpuid.h)
AC_CHECK_HEADERS(x86intrin.h)
AC_CHECK_HEADERS(syslog.h)
AC_CHECK_HEADERS(linux/random.h)

AC_CANONICAL_HOST

## Determine if daemon interface to be included
if test "x$enable_capture" == "xyes"; then
   HA_DAEMON="no"
elif test "x$enable_inject" == "xyes"; then
   HA_DAEMON="no"
elif test "x$enable_daemon" = "xyes"; then
   HA_DAEMON="yes"
elif test "x$enable_daemon" = "xno"; then
   HA_DAEMON="no"
else
   case "$host_os" in
    linux*)
      HA_DAEMON="yes"
   ;;
    *)
      HA_DAEMON="no"
   ;;
  esac
fi 

## Match host to havegedef macros
case "$host" in
 x86_64-*)
    AC_DEFINE(HAVE_ISA_X86, 1, [Define to 1 for x86_64])
 ;;

 i*86*-*)
    AC_DEFINE(HAVE_ISA_X86, 1, [Define to 1 for x86])
 ;;

 ia64-*)
    AC_DEFINE(HAVE_ISA_IA64, 1, [Define to 1 for ia64])
 ;;

 powerpc-*|pcc-*|powerpc64|ppc64)
    AC_DEFINE(HAVE_ISA_PPC, 1, [Define to 1 for ppc])
 ;;

 s390*-*)
    AC_DEFINE(HAVE_ISA_S390, 1, [Define to 1 for s390])
 ;;

 sparclite*-*)
    AC_DEFINE(HAVE_ISA_SPARCLITE, 1, [Define to 1 for sparclite])
 ;;

 sparc*-*)
    AC_DEFINE(HAVE_ISA_SPARC, 1, [Define to 1 for sparc])
 ;;

 *)
    HA_LDFLAGS="$HA_LDFLAGS -lrt"
    AC_DEFINE_UNQUOTED(HAVE_ISA_GENERIC, "$host", [Define generic host])
 ;;

esac

## Suppress daemon
if test "$HA_DAEMON" = "no"; then
    AC_DEFINE(NO_DAEMON, 1, [Define to 1 to supress daemon interface])
fi
## Fixup install

AC_SUBST(HA_DISTRO)

## Set hardware depedent define for the build

AC_SUBST(HA_LDFLAGS)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 man/Makefile
                 init.d/Makefile
                 ent/Makefile
                 nist/Makefile])
AC_OUTPUT
