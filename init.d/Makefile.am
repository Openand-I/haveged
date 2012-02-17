## Process this file with automake to produce Makefile.in.

initdir       = /etc/init.d
init_SCRIPTS = haveged

EXTRA_DIST = lsb.in redhat.in

CLEANFILES = haveged

MAINTAINERCLEANFILES = Makefile.in

do_subst = sed -e 's,[@]SBIN_DIR[@],$(sbindir),g'
src_tmpl = @HA_DISTRO@

haveged:	$(src_tmpl) Makefile
			$(do_subst) < $(srcdir)/$(src_tmpl) > haveged

