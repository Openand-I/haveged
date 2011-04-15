Name:           haveged
Version:        1.0
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

%prep
%setup -q

%build
./configure
make
make check

%install

rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%clean

rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/local/sbin/haveged
/usr/local/share/man/man8/haveged.8
/etc/init.d/haveged
