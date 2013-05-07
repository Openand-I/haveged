#
# Sample spec file for haveged and haveged-devel
# Copyright  (c)  2013
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
Name:           haveged
Version:        1.7
Release:        1
License:        GPLv3
Group:          System Environment/Daemons
Summary:        Feed entropy into random pool
URL:            http://www.issihosts.com/haveged/
Source0:        http://www.issihosts.com/haveged/haveged-%{version}.tar.gz
BuildRoot:      %{_builddir}/%{name}-root

%description
The haveged daemon feeds the linux entropy pool with random
numbers generated from hidden processor state.

%package devel
Summary:    haveged development files
Group:      Development/Libraries
Provides:   haveged-devel

%description devel
Headers and shared object symbolic links for the haveged library

This package contains the haveged implementation of the HAVEGE
algorithm and supporting features.

%prep
%setup -q

%build
./configure
make

%check
make check

%install
[ ${RPM_BUILD_ROOT} != "/" ] && rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean
[ ${RPM_BUILD_ROOT} != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root, -)
%{_mandir}/man8/haveged.8*
%{_sbindir}/haveged
%{_unitdir}/haveged.service
%doc COPYING README ChangeLog AUTHORS

%files devel
%defattr(-, root, root, -)
%{_mandir}/man3/libhavege.3*
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/havege.h
%doc contrib/build/havege_sample.c
%{_libdir}/*.so*
