## Process this file with autoconf to produce a configure script.

## Minimum Autoconf version
AC_PREREQ(1.9)

AC_INIT([haveged],[1.3])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([/src/haveged.c])

## Make build w/o daemon interface configurable
AC_ARG_ENABLE(daemon, AS_HELP_STRING([--enable-daemon=[yes/no]],[Enable daemon [default=yes if linux]]),, enable_daemon="x" )

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
AC_CHECK_FUNCS([floor gettimeofday memset pow select sqrt])

## Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(stdlib.h)
AC_CHECK_HEADERS(string.h)
AC_CHECK_HEADERS(sys/ioctl.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(unistd.h)
AC_CHECK_HEADERS(cpuid.h)
AC_CHECK_HEADERS(x86intrin.h)
AC_CHECK_HEADERS(syslog.h)
AC_CHECK_HEADERS(linux/random.h)

AC_CANONICAL_HOST

## Determine if daemon interface to be included
if test "x$enable_daemon" = "xyes"; then
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
    HA_CPPFLAGS="-DHAVE_ISA_X86 -DHAVE_64"
 ;;

 i*86*-*)
    HA_CPPFLAGS="-DHAVE_ISA_X86"
 ;;

 ia64-*)
    HA_CPPFLAGS="-DHAVE_ISA_IA64"
 ;;

 powerpc-*|pcc-*|powerpc64|ppc64)
    HA_CPPFLAGS="-DHAVE_ISA_PPC"
 ;;

 sparclite*-*)
    HA_CPPFLAGS="-DHAVE_ISA_SPACLITE"
 ;;

 sparc*-*)
    HA_CPPFLAGS="-DHAVE_ISA_SPARC"
 ;;

 *)
    echo "Unsupported host: $host";
    exit 1
 ;;

esac

## Suppress daemon
if test "$HA_DAEMON" = "no"; then
     HA_CPPFLAGS="$HA_CPPFLAGS -DNO_DAEMON"
fi
## Fixup install too
AM_CONDITIONAL([DAEMON], [test HA_DAEMON = yes])

## Set hardware depedent define for the build
AC_SUBST(HA_CPPFLAGS,$HA_CPPFLAGS)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 man/Makefile
                 ent/Makefile
                 nist/Makefile])
AC_OUTPUT
