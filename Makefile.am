## Process this file with automake to produce Makefile.in

# checks to perform is a autoconf substitution
check_SCRIPTS = @HA_CHECK@

# startup script
initdir       = /etc/init.d
init_SCRIPTS  = init.d/haveged

# Files to be included in distribution
EXTRA_DIST    = haveged.spec ent/test.sh ent/entitle.gif init.d/haveged nist/test.sh nist/template9

# Scripts run as check target
EXTRA_SCRIPTS = ent/test.sh nist/test.sh

# Distribution contains source for daemon and 2 tests
SUBDIRS       = src man ent nist

# Perform all check_SCRIPTS
TESTS         = @HA_CHECK@

