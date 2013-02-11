## Process this file with automake to produce Makefile.in.

EXTRA_DIST = service.redhat sysv.lsb sysv.redhat

MAINTAINERCLEANFILES = Makefile.in

CLEANFILES = haveged haveged.service

do_subst = sed -e 's,[@]SBIN_DIR[@],$(sbindir),g'
src_tmpl = @HA_DISTRO@

if ENABLE_SYSV
## legacy init script from template

initdir = /etc/init.d
init_SCRIPTS = haveged

haveged:	$(src_tmpl) Makefile
			$(do_subst) < $(srcdir)/$(src_tmpl) > haveged;

endif

install-data-hook:
if ENABLE_SYSTEMD
## new style init - requires pkg-config.
	$(do_subst) < $(srcdir)/$(src_tmpl) > haveged.service; \
	install -p -m644 haveged.service `pkg-config --variable=systemdsystemunitdir systemd`/haveged.service; \
	systemctl enable haveged.service;

endif
if ENABLE_NOINIT
## user will install if necessary.
	@echo "no init script installed";
endif
