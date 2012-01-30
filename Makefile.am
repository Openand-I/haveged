## Process this file with automake to produce Makefile.in

# checks to perform is a autoconf substitution
check_SCRIPTS = @HA_CHECK@

if DAEMON
# startup script
initdir       = /etc/init.d
init_SCRIPTS  = init.d/haveged init.d/haveged_fedora
endif

# Files to be included in distribution
EXTRA_DIST    = haveged.spec ent/test.sh ent/entitle.gif init.d/haveged init.d/haveged_fedora \
                nist/test.sh nist/template9 contrib/fedora12/fix_initrddir_path.patch \
                contrib/fedora12/fix_sbin_install_location.patch \
                contrib/fedora12/haveged.spec contrib/fedora12/README \
		contrib/haveged_fedora

# Scripts run as check target
EXTRA_SCRIPTS = ent/test.sh nist/test.sh

# Distribution contains source for daemon and 2 tests
SUBDIRS       = src man ent nist

# Perform all check_SCRIPTS
TESTS         = @HA_CHECK@

