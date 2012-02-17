## Process this file with automake to produce Makefile.in

# checks to perform is a autoconf substitution
check_SCRIPTS = @HA_CHECK@

# Files to be included in distribution
EXTRA_DIST    = haveged.spec ent/test.sh ent/entitle.gif init.d/redhat.in init.d/lsb.in \
                nist/test.sh nist/template9 \
                contrib/diags/data_prep.c contrib/diags/bins.p contrib/diags/inc.p \
                contrib/diags/lognorm.r contrib/diags/lognorm.sh

# Scripts run as check target
EXTRA_SCRIPTS = ent/test.sh nist/test.sh

# Build install script and tests as necessary
SUBDIRS       = src init.d man ent nist

# Perform all check_SCRIPTS
TESTS         = @HA_CHECK@

