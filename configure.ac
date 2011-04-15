## Process this file with autoconf to produce a configure script.

## Minimum Autoconf version
AC_PREREQ(2.61)
#
AC_INIT([haveged],[1.01])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([/src/haveged.c])

## Make nist self-test configurable
AC_ARG_ENABLE(nistest, AS_HELP_STRING([--enable-nistest=[no/yes]],[Run NIST test suite [default=no]]),, enable_nistest="no")
if test "x$enable_nistest" = "xyes"; then
   HA_CHECK="ent/test.sh nist/test.sh"
else
   HA_CHECK="ent/test.sh"
fi
AC_SUBST(HA_CHECK,$HA_CHECK)

## Match host to havegedef macros
AC_CANONICAL_HOST
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

# Checks for programs.
AC_PROG_CC([gcc])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_HEADER_TIME

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([floor gettimeofday memset pow select sqrt])

# Sets hardware depedent define for the build
AC_SUBST(HA_CPPFLAGS,$HA_CPPFLAGS)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 man/Makefile
                 ent/Makefile
                 nist/Makefile])
AC_OUTPUT
