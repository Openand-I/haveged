Name:           haveged
Version:        0.9
Release:        3%{?dist}
Summary:        A simple entropy daemon
Group:          System Environment/Base

License:        GPLv3
URL:            http://www.issihosts.com/haveged/
Source0:        %{name}-%{version}.tar.gz
Patch0:         fix_initrddir_path.patch
Patch1:         fix_sbin_install_location.patch
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc
BuildRequires: automake
BuildRequires: autoconf >= 2.61

%description


%prep
%setup -q
%patch -P 0
%patch -P 1


%build
autoreconf
automake
%configure
make %{?_smp_mflags}
make check


%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(0644,root,root,0755)
%doc %{_mandir}/man8/%{name}.8.gz
%defattr(0755,root,root,0755)
%{_prefix}/sbin/%{name}
%{_initrddir}/%{name}


%changelog
* Wed Oct 20 2010 Henrik Probell <henrik@henrik.gbg.touchtable.se> - 0.9-2
- Patches added to install SysV init-script into the /etc/rc.d/init.d
  and binaries in /usr/sbin added.

* Mon Oct 18 2010 Henrik Probell <henrik@touchtable.se> - 0.9-1
- Initial attempt at packaging haveged-0.9
