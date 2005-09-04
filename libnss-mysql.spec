Summary: NSS library for MySQL.
Name: libnss-mysql
Version: 1.5
Release: 1
Source0: http://prdownloads.sourceforge.net/libnss-mysql/libnss-mysql-%{version}.tar.gz
URL: http://libnss-mysql.sourceforge.net
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
%doc README ChangeLog AUTHORS THANKS NEWS COPYING FAQ DEBUGGING UPGRADING TODO
%doc sample

%changelog
* Sat Sep 3 2005 Ben Goodwin <cinergi@users.sourceforge.net> 1.5-1
- Update to 1.4

* Sat Apr 10 2004 Ben Goodwin <cinergi@users.sourceforge.net> 1.4-1
- Update to 1.4

* Sat Apr 10 2004 Ben Goodwin <cinergi@users.sourceforge.net> 1.3-1
- Update to 1.3
- Remove manual static re-link (1.3 relieves the need for this)
- doc += TODO

* Sun Mar 28 2004 Ben Goodwin <cinergi@users.sourceforge.net> 1.2-1
- Update to 1.2

* Tue Mar 02 2004 Ben Goodwin <cinergi@users.sourceforge.net> 1.1-1
- s#exports.linux#.libs/libnss_mysql.ver#
- Oops, libs/*.o not *.lo

* Sat Jul 12 2003 Ben Goodwin <cinergi@users.sourceforge.net> 1.0-2
- Link with version script

* Sat Jul 12 2003 Ben Goodwin <cinergi@users.sourceforge.net> 1.0-1
- Update to 1.0
- Use *.lo instead of individual .lo names in re-link
- Removed -Bgroup and --allow-shlib-undefined linker options

* Thu Jun 19 2003 Ben Goodwin <cinergi@users.sourceforge.net> 0.9-2
- Added ugly hack to relink some libraries static.  It will probably
  break rpm builds on some hosts ...

* Wed Jun 18 2003 Ben Goodwin <cinergi@users.sourceforge.net> 0.9-1
- Update to 0.9

* Sun Dec 29 2002 Ben Goodwin <cinergi@users.sourceforge.net> 0.8-1
- Initial version
