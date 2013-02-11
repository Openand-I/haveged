## Process this file with automake to produce Makefile.in

# checks to perform is a autoconf substitution
check_SCRIPTS = @HA_CHECK@

# Files to be included in distribution
EXTRA_DIST    = ent/test.sh ent/entitle.gif init.d/service.fedora init.d/service.redhat \
                init.d/sysv.lsb init.d/sysv.redhat nist/test.sh nist/template9 \
                contrib/diags/data_prep.c contrib/diags/bins.p contrib/diags/inc.p \
                contrib/diags/lognorm.r contrib/diags/lognorm.sh \
                haveged.spec contrib/build/build.sh contrib/build/lib.spec contrib/build/nolib.spec \
                contrib/build/havege_sample.c contrib/build/fedora.spec

# Scripts run as check target
EXTRA_SCRIPTS = ent/test.sh nist/test.sh

# Build install script and tests as necessary
SUBDIRS       = src init.d man ent nist

# Perform all check_SCRIPTS
TESTS         = @HA_CHECK@

