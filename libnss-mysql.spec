Summary: NSS library for MySQL.
Name: libnss-mysql
Version: 0.9
Release: 1
Source0: http://prdownloads.sourceforge.net/libnss-mysql/libnss-mysql-%{version}.tar.gz
URL: http://libnss-mysql.sourceforge.net/
License: GPL
Group: System Environment/Base
BuildRoot: %{_tmppath}/%{name}-root

%description
Store your UNIX user accounts in MySQL

%prep
%setup -q -a 0

%build
./configure
make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/{etc,lib}
make DESTDIR=$RPM_BUILD_ROOT install

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%attr(0755,root,root) /lib/*.so*
%attr(0644,root,root) %config(noreplace) /etc/libnss-mysql.cfg
%attr(0600,root,root) %config(noreplace) /etc/libnss-mysql-root.cfg
%doc README ChangeLog AUTHORS THANKS NEWS COPYING FAQ DEBUGGING UPGRADING
%doc sample

%changelog
* Wed Jun 18 2003 Ben Goodwin <cinergi@users.sourceforge.net> 0.9-1
- Update to 0.9

* Sun Dec 29 2002 Ben Goodwin <cinergi@users.sourceforge.net> 0.8-1
- Initial version
