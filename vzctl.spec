%define _initddir %_sysconfdir/init.d
%define _vzdir /vz
%define _lockdir %{_vzdir}/lock
%define _dumpdir %{_vzdir}/dump
%define _privdir %{_vzdir}/private
%define _rootdir %{_vzdir}/root
%define _cachedir %{_vzdir}/template/cache
%define _veipdir /var/lib/vzctl/veip
%define _pkglibdir %_libdir/vzctl
%define _configdir %_sysconfdir/vz
%define _scriptdir /usr/share/vzctl/scripts
%define _vpsconfdir %_sysconfdir/sysconfig/vz-scripts
%define _netdir	%_sysconfdir/sysconfig/network-scripts
%define _logrdir %_sysconfdir/logrotate.d
%define _distconfdir %{_configdir}/dists
%define _namesdir %{_configdir}/names
%define _distscriptdir %{_distconfdir}/scripts
%define _udevrulesdir %_sysconfdir/udev/rules.d
%define _bashcdir %_sysconfdir/bash_completion.d


Summary: OpenVZ containers control utility
Name: vzctl
Version: 3.3
%define rel 1
Release: %{rel}%{?dist}
License: GPL
Group: System Environment/Kernel
Source: http://download.openvz.org/utils/%{name}/%{version}/src/%{name}-%{version}.tar.bz2
ExclusiveOS: Linux
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: vzkernel
Requires: vzeventmod
URL: http://openvz.org/
# these reqs are for vz helper scripts
Requires: bash
Requires: gawk
Requires: sed
Requires: grep
Requires: /sbin/chkconfig
Requires: vzquota >= 2.7.0-4
Requires: fileutils
Requires: vzctl-lib = %{version}-%{release}
Requires: tar
Requires: ploop > 1.4-1
BuildRequires: ploop-devel > 1.4-1

# requires for vzmigrate purposes
Requires: rsync
Requires: gawk
Requires: openssh

# Virtual provides for newer RHEL6 kernel
Provides: virtual-vzkernel-install = 2.0.0

%description
This utility allows system administator to control OpenVZ containers,
i.e. create, start, shutdown, set various options and limits etc.

%prep
%setup

%build
CFLAGS="$RPM_OPT_FLAGS" %configure \
	--enable-bashcomp \
	--enable-logrotate \
	--disable-static
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT vpsconfdir=%{_vpsconfdir} \
	install install-redhat-from-spec
ln -s ../sysconfig/vz-scripts $RPM_BUILD_ROOT/%{_configdir}/conf
ln -s ../vz/vz.conf $RPM_BUILD_ROOT/etc/sysconfig/vz
# Needed for %ghost in %files section below
touch $RPM_BUILD_ROOT/etc/sysconfig/vzeventd
# This could go to vzctl-lib-devel, but since we don't have it...
rm -f  $RPM_BUILD_ROOT/%_libdir/libvzctl.{la,so}

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir %{_pkglibdir}/scripts
%attr(755,root,root) %{_pkglibdir}/scripts/initd-functions
%attr(755,root,root) %{_initddir}/vz
%attr(755,root,root) %{_initddir}/vzeventd
%dir %attr(755,root,root) %{_lockdir}
%dir %attr(755,root,root) %{_dumpdir}
%dir %attr(700,root,root) %{_privdir}
%dir %attr(700,root,root) %{_rootdir}
%dir %attr(755,root,root) %{_cachedir}
%dir %attr(755,root,root) %{_veipdir}
%dir %attr(755,root,root) %{_configdir}
%dir %attr(755,root,root) %{_namesdir}
%dir %attr(755,root,root) %{_vpsconfdir}
%dir %attr(755,root,root) %{_distconfdir}
%dir %attr(755,root,root) %{_distscriptdir}
%dir %attr(755,root,root) %{_vzdir}
%attr(755,root,root) %{_sbindir}/vzctl
%attr(755,root,root) %{_sbindir}/vzeventd
%attr(755,root,root) %{_sbindir}/arpsend
%attr(755,root,root) %{_sbindir}/ndsend
%attr(755,root,root) %{_sbindir}/vzsplit
%attr(755,root,root) %{_sbindir}/vzlist
%attr(755,root,root) %{_sbindir}/vzmemcheck
%attr(755,root,root) %{_sbindir}/vzcpucheck
%attr(755,root,root) %{_sbindir}/vznetcfg
%attr(755,root,root) %{_sbindir}/vznetaddbr
%attr(755,root,root) %{_sbindir}/vzcalc
%attr(755,root,root) %{_sbindir}/vzpid
%attr(755,root,root) %{_sbindir}/vzcfgvalidate
%attr(755,root,root) %{_sbindir}/vzmigrate
%attr(755,root,root) %{_sbindir}/vzifup-post
%attr(755,root,root) %{_sbindir}/vzubc
%attr(644,root,root) %{_logrdir}/vzctl
%attr(644,root,root) %{_distconfdir}/distribution.conf-template
%attr(644,root,root) %{_distconfdir}/default
%attr(755,root,root) %{_distscriptdir}/*.sh
%attr(644,root,root) %{_distscriptdir}/functions
%attr(755,root,root) %{_netdir}/ifup-venet
%attr(755,root,root) %{_netdir}/ifdown-venet
%attr(644,root,root) %{_netdir}/ifcfg-venet0
%attr(644, root, root) %{_mandir}/man8/vzctl.8.*
%attr(644, root, root) %{_mandir}/man8/vzeventd.8.*
%attr(644, root, root) %{_mandir}/man8/vzmigrate.8.*
%attr(644, root, root) %{_mandir}/man8/arpsend.8.*
%attr(644, root, root) %{_mandir}/man8/ndsend.8.*
%attr(644, root, root) %{_mandir}/man8/vzsplit.8.*
%attr(644, root, root) %{_mandir}/man8/vzcfgvalidate.8.*
%attr(644, root, root) %{_mandir}/man8/vzmemcheck.8.*
%attr(644, root, root) %{_mandir}/man8/vzcalc.8.*
%attr(644, root, root) %{_mandir}/man8/vzpid.8.*
%attr(644, root, root) %{_mandir}/man8/vzcpucheck.8.*
%attr(644, root, root) %{_mandir}/man8/vzubc.8.*
%attr(644, root, root) %{_mandir}/man8/vzlist.8.*
%attr(644, root, root) %{_mandir}/man8/vzifup-post.8.*
%attr(644, root, root) %{_mandir}/man5/ctid.conf.5.*
%attr(644, root, root) %{_mandir}/man5/vz.conf.5.*
%attr(644, root, root) %{_udevrulesdir}/*
%attr(644, root, root) %{_bashcdir}/*

%config(noreplace) %{_configdir}/vz.conf
%config(noreplace) %{_configdir}/osrelease.conf
%config(noreplace) %{_configdir}/download.conf
%config(noreplace) %{_distconfdir}/*.conf
%config %{_vpsconfdir}/ve-basic.conf-sample
%config %{_vpsconfdir}/ve-light.conf-sample
%config %{_vpsconfdir}/ve-unlimited.conf-sample
%config %{_vpsconfdir}/ve-vswap-256m.conf-sample
%config %{_vpsconfdir}/ve-vswap-512m.conf-sample
%config %{_vpsconfdir}/ve-vswap-1024m.conf-sample
%config %{_vpsconfdir}/ve-vswap-1g.conf-sample
%config %{_vpsconfdir}/ve-vswap-2g.conf-sample
%config %{_vpsconfdir}/ve-vswap-4g.conf-sample
%config %{_vpsconfdir}/0.conf

%attr(777, root, root) /etc/vz/conf
%config /etc/sysconfig/vz
%ghost %config(missingok) /etc/sysconfig/vzeventd

%post
/bin/rm -rf /dev/vzctl
/bin/mknod -m 600 /dev/vzctl c 126 0
/sbin/chkconfig --add vz > /dev/null 2>&1
/sbin/chkconfig --add vzeventd > /dev/null 2>&1

if [ -f /etc/SuSE-release ]; then
	NET_CFG='ifdown-venet ifup-venet'
	if ! grep -q -E "^alias venet0" /etc/modprobe.conf; then
		echo "alias venet0 vznet" >> /etc/modprobe.conf
	fi
	ln -f /etc/sysconfig/network-scripts/ifcfg-venet0 /etc/sysconfig/network/ifcfg-venet0
	for file in ${NET_CFG}; do
		ln -sf /etc/sysconfig/network-scripts/${file} /etc/sysconfig/network/scripts/${file}
	done
fi
# Install a symlink to vzifup-post
if [ -f /etc/SuSE-release ]; then
	ln -sf %{_sbindir}/vzifup-post /etc/sysconfig/network/if-up.d/
else # RedHat/Fedora/CentOS case
	if [ ! -e /sbin/ifup-local ]; then
		ln -sf %{_sbindir}/vzifup-post /sbin/ifup-local
	elif readlink /sbin/ifup-local |
				fgrep -q %{_sbindir}/vzifup-post; then
		: # Nothing to do, symlink already points to our script
	else
		echo " WARNING: file /sbin/ifup-local is present!"
		echo " You have to manually edit the above file so that"
		echo " it calls %{_sbindir}/vzifup-post"
	fi
fi

# (Upgrading from <= vzctl-3.0.24)
# If vz is running and vzeventd is not, start it
if %{_initddir}/vz status >/dev/null 2>&1; then
	if ! %{_initddir}/vzeventd status >/dev/null 2>&1; then
		%{_initddir}/vzeventd start
	fi
fi
exit 0

%preun
if [ $1 = 0 ]; then
	/sbin/chkconfig --del vz >/dev/null 2>&1
	/sbin/chkconfig --del vzeventd >/dev/null 2>&1
fi

%package lib
Summary: Containers control API library
Group: System Environment/Kernel
Requires: libxml2
BuildRequires: libxml2-devel >= 2.6.16

%description lib
Containers control API library

%files lib
%defattr(-,root,root)
%attr(755,root,root) %{_libdir}/libvzctl-*.so
%dir %{_pkglibdir}
%dir %{_pkglibdir}/scripts
%attr(755,root,root) %{_pkglibdir}/scripts/vps-functions
%attr(755,root,root) %{_pkglibdir}/scripts/vps-net_add
%attr(755,root,root) %{_pkglibdir}/scripts/vps-net_del
%attr(755,root,root) %{_pkglibdir}/scripts/vps-create
%attr(755,root,root) %{_pkglibdir}/scripts/vps-download
%attr(755,root,root) %{_pkglibdir}/scripts/vzevent-stop
%attr(755,root,root) %{_pkglibdir}/scripts/vzevent-reboot
%attr(755,root,root) %{_pkglibdir}/scripts/vps-pci

%changelog
* Thu May 31 2012 Kir Kolyshkin <kir@openvz.org> - 3.3-1
- New features
  - vzmigrate: ploop live migration using ploop-copy (#2252)
  - vzctl stop: add --skip-umount flag
  - vzctl set --ram/--swap: add --force
- Bug fixes
  - fix vzctl and vzlist linking with ld 2.22
- Improvements
  - vzmigrate: improve timings display, add -t option
  - bash_completion: for vzctl restart offer running CT IDs

* Fri May 18 2012 Kir Kolyshkin <kir@openvz.org> - 3.2.1-1
- vzctl set: fix processing --ram/--swap options (#2269)
- vzctl start: improve err msg for vswap config vs non-vswap kernel (#2263)

* Thu May 3 2012 Kir Kolyshkin <kir@openvz.org> - 3.2-1
- New features
  - vzctl console now accepts tty number argument
  - vzctl console: add ESC ! to issue SAK
  - vzlist: show diskspace/diskinodes usage/limit for ploop CTs
  - vzlist: add more new fields
    - layout (simfs/ploop)
    - private/root (to show VE_PRIVATE and VE_ROOT)
    - features
    - smart_ctid (CT name if available, otherwise numeric CTID)
- Fixes
  - vzctl start: ability to start containers with systemd
  - vzctl set --ram, --swap: default value is now in bytes
  - vzctl set --save: do not save parameters if failed to apply (#2032)
  - vzctl restore: fix non-working in-CT quota after restore for ploop case
  - vzctl restore: do not ignore DUMPDIR value
  - Fix giving excessive permissions for ugid quota disk device
  - vzctl console: do not issue SAK on detach (it can kill scripts)
  - vzctl start: umount ploop image on CT start
  - vzctl set/start/convert</code: check for max possible ploop size (#2250)
  - vzlist: do not show UBC from proc for stopped CTs (#2151)
  - init.d/initd-functions: fixes for dash
  - vzubc: fix mixed up qheld/qmaxheld (#2238)
  - vzctl snapshot: resume CT if creating snapshot failed
  - vzmigrate: skip vzquota ops for ploop-based CTs (related to #2252)
  - vzmigrate: do not migrate ploop CT if ploop is not available on dst
  - vzmigrate: do not use --sparse for ploop CTs (related to #2252)
  - Fix error handling in vps_is_run() (#2243)
- Improvements
  - vps-download: accept relative template cache paths (#2222)
  - vzlist: use smart_ctid instead of ctid in default output format
  - vzctl set ram/swap, vzctl start: check if kernel is vswap capable (#2251)
  - bash_completion: only complete simfs CTs for vzctl convert
  - bash_completion: only complete ploop CTs for vzctl snapshot*
  - vzubc: allow -qh/-qm argument to be per cent (if > 1)
  - vzctl snapshot: removed snapshot-create command alias
  - vzctl snapshot: add --skip-suspend option
  - vzctl set --features/--iptables/--capability: ability to specify
    several comma-separated values at once
  - vzmigrate: make -vvv add -vv to rsync
- Code cleanups
  - include/*.h: remove non-existent function prototypes
  - remove NULL checks before free()
  - some functions marked as static, moved to there they belong
  - get rid of setup_resource_management()
  - whitespace nitpicks
- Documentation
  - Add --ram, --swap to vzctl --help output (#2219)
  - vzctl(8): explain host_mac value for bridge (#2210)
  - vzctl(8): better description of --quotaugidlimit wrt ploop
  - vzctl(8): do not use "second-level quota" term
  - vzctl(8): document ttynum vzctl console argument
  - vzctl(8): add/improve escape sequences description for vzctl console
  - vzctl(8): document --reset_ub
  - vzctl(8): describe --name and --description for vzctl snapshot
  - vzctl(8): various formatting fixes and improvements
  - vzmigrate(8): add missing exit codes description
  - man/toc.man.in: fix Copyright years
  - vzctl.spec: add changelog

* Thu Mar 22 2012 Kir Kolyshkin <kir@openvz.org> - 3.1-1
- New features
  - preliminary beta support for ploop (aka container-in-a-file) technology
    - new global config parameter VE_LAYOUT={simfs|ploop}
    - new vzctl create options --layout and --diskspace
    - new vzctl convert command to convert from simfs to ploop (not back!)
    - vzctl mount/umount implemented for ploop case
    - vzctl set --diskspace does ploop image resize
    - second-level (quotaugidlimit) quota on ploop/ext4 support
    - basic snapshot functionality (vzctl snapshot* commands)
  - support for CT console (vzctl console command)
- Fixes
  - gentoo-add_ip.sh: do not set up venet0 if no IPs (#2077)
  - vzctl enter: fix garbage output after enter (#2139, #2146)
  - vzlist: do not exit with 1 if there are no CTs (#2149)
  - vps-download: fix downloaded template GPG check (#2162)
  - vps-download: fix to work under dash
  - vzctl destroy: remove dump file as well (#2163)
  - init.d/vz: fix grep statement
  - vzctl restore: fix "container already running" exit code
- Improvements
  - Make the "Failed to set up upstart" message more verbose (#2140)
  - vzctl create: tell "Creating container" at the right time
  - vzctl create: show tarball extraction progress using pv (if available)
  - init.d/vz: Stricter auto-replacement of CONFIGFILE (#2169)
  - init.d/vz: fix for "we are in container" check
  - postcreate.sh: add ability to skip crontab time randomization (#2174)
  - Improve config parsing and its error reporting
  - vzctl create: improve 'sample config not found' error msg
  - umount_submounts(): process mounts in reverse order
- Documentation
  - ploop and console documented in appropriate man pages
  - man/vzctl.8: fix --diskspace description for ploop case
  - man/vzctl.8: --diskquota, --diskinodes and --quotatime ignored for ploop
  - some macros that are not available on older systems are now embedded
  - vzctl man page: simplified SYNOPSYS section
  - vz.conf(5), vzctl(8): fix/improve description of CONFIGFILE / --config
  - vzctl --help: fix create options
  - vz.conf(5), vzctl(8): describe DEF_OSTEMPLATE / --ostemplate
  - vzctl(8), vzctl --help: add missing --name option to 'create'
  - vzctl(8): add CTID to commands where it was absent

* Wed Jun 13 2007 Andy Shevchenko <andriy@asplinux.com.ua> - 3.0.17-1
- fixed according to Fedora Packaging Guidelines:
  - use dist tag
  - added URL tag
  - use full url for source
  - changed BuildRoot tag
