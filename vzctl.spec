%define _initddir %{_sysconfdir}/init.d
%define _vzdir /vz
%define _scriptdir %{_libexecdir}/vzctl/scripts
%define _configdir %{_sysconfdir}/vz
%define _vpsconfdir %{_sysconfdir}/sysconfig/vz-scripts
%define _netdir	%{_sysconfdir}/sysconfig/network-scripts
%define _distconfdir %{_configdir}/dists
%define _distscriptdir %{_distconfdir}/scripts
# Some older systems have sharedstatedir as /usr/com
%define _sharedstatedir /var/lib

Summary: OpenVZ containers control utility
Name: vzctl
Version: 4.9.4
%define rel 1
Release: %{rel}%{?dist}
License: GPLv2+
Group: System Environment/Kernel
Source: http://download.openvz.org/utils/%{name}/%{version}/src/%{name}-%{version}.tar.bz2
ExclusiveOS: Linux
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: vzkernel
Requires: vzeventmod
URL: http://openvz.org/
Requires: /sbin/chkconfig
Requires: vzquota >= 3.1
Requires: fileutils
Requires: vzctl-core = %{version}-%{release}
Requires: tar
Requires: attr
Requires: vzstats
Conflicts: ploop-lib <= 1.12.2-1
BuildRequires: ploop-devel > 1.12.2-1
BuildRequires: libxml2-devel >= 2.6.16
BuildRequires: libcgroup-devel >= 0.37
# requires for vzmigrate purposes
Requires: rsync
Requires: gawk
Requires: openssh
Requires: bridge-utils
# Virtual provides for newer RHEL6 kernel
Provides: virtual-vzkernel-install = 2.0.0

%description
This utility allows system administrators to control Linux containers,
i.e. create, start, shutdown, set various options and limits etc.

%prep
%setup -q

%build
CFLAGS="%{optflags}" %configure \
	vzdir=%{_vzdir} \
	--enable-bashcomp \
	--enable-logrotate \
	--disable-static
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} vpsconfdir=%{_vpsconfdir} \
	install install-redhat-from-spec
ln -s ../sysconfig/vz-scripts %{buildroot}%{_configdir}/conf
ln -s ../vz/vz.conf %{buildroot}/etc/sysconfig/vz
# Needed for %ghost in %files section below
touch %{buildroot}%{_sharedstatedir}/vz
touch %{buildroot}/etc/sysconfig/vzeventd
mkdir %{buildroot}/etc/modprobe.d
touch %{buildroot}/etc/modprobe.d/openvz.conf
# This could go to vzctl-lib-devel, but since we don't have it...
rm -f %{buildroot}%{_libdir}/libvzctl.la
rm -f %{buildroot}%{_libdir}/libvzctl.so
rm -f %{buildroot}%{_libdir}/libvzchown.la
rm -f %{buildroot}%{_libdir}/libvzchown.so.*

%clean
rm -rf %{buildroot}

%files
%dir %{_scriptdir}
%{_scriptdir}/initd-functions
%{_initddir}/vz
%{_initddir}/vzeventd
%{_sbindir}/vzeventd
%{_sbindir}/vzsplit
%{_sbindir}/vzlist
%{_sbindir}/vzmemcheck
%{_sbindir}/vzcpucheck
%{_sbindir}/vznetcfg
%{_sbindir}/vznetaddbr
%{_sbindir}/vzcalc
%{_sbindir}/vzcptcheck
%{_sbindir}/vzfsync
%{_sbindir}/vznnc
%{_sbindir}/vzpid
%{_sbindir}/vzcfgvalidate
%{_sbindir}/vzmigrate
%{_sbindir}/vzifup-post
%{_sbindir}/vzoversell
%{_sbindir}/vzubc
%{_netdir}/ifup-venet
%{_netdir}/ifdown-venet
%{_netdir}/ifcfg-venet0
%{_mandir}/man8/vzeventd.8.*
%{_mandir}/man8/vzmigrate.8.*
%{_mandir}/man8/vzcptcheck.8.*
%{_mandir}/man8/vzfsync.8.*
%{_mandir}/man8/vznnc.8.*
%{_mandir}/man8/vzsplit.8.*
%{_mandir}/man8/vzcfgvalidate.8.*
%{_mandir}/man8/vzmemcheck.8.*
%{_mandir}/man8/vzcalc.8.*
%{_mandir}/man8/vzpid.8.*
%{_mandir}/man8/vzcpucheck.8.*
%{_mandir}/man8/vztmpl-dl.8.*
%{_mandir}/man8/vzubc.8.*
%{_mandir}/man8/vzlist.8.*
%{_mandir}/man8/vzifup-post.8.*
%{_sysconfdir}/udev/rules.d/*
%{_sysconfdir}/bash_completion.d/*
%config /etc/sysconfig/vz
%ghost %config(missingok) /etc/sysconfig/vzeventd
%ghost %config(missingok) /etc/modprobe.d/openvz.conf

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

# Some use /var/lib/vz instead of /vz; create a compatibility symlink
test -a %{_sharedstatedir}/vz || ln -s ../..%{_vzdir} %{_sharedstatedir}/vz

# (Upgrading from <= vzctl-3.0.24)
# If vz is running and vzeventd is not, start it
if %{_initddir}/vz status >/dev/null 2>&1; then
	if ! %{_initddir}/vzeventd status >/dev/null 2>&1; then
		%{_initddir}/vzeventd start
	fi
fi

# Disable VE0 conntracks if they are not used (#2755)
file='/etc/modprobe.d/openvz.conf'
line='options nf_conntrack ip_conntrack_disable_ve0'
if ! grep -wq 'ip_conntrack_disable_ve0' /etc/modprobe.d/* 2>/dev/null; then
	cat << EOF
============================================================================
EOF
	if /sbin/iptables -L -n -t nat | grep -qEw 'SNAT|DNAT|MASQUERADE'; then
		# conntracks are used
		disable=0
	elif /sbin/iptables -L -n | grep -qEw 'state|ctstate'; then
		disable=0
	else
		disable=1
		cat << EOF
Due to conntrack impact on venet performance, conntrack need to be disabled
on the host system (it will still work for containers).

EOF

	fi
	echo "$line=$disable" >> $file
	cat << EOF
Adding the following option to $file:

	$line=$disable

This change will take effect only after the next reboot.

NOTE: if you need to change this setting, edit $file
now. DO NOT REMOVE the line, or it will be re-added!
============================================================================
EOF
fi

# Run post-install script only when installing
test $1 -eq 1 && %{_scriptdir}/vz-postinstall selinux
%{_scriptdir}/vz-postinstall yum

exit 0

%preun
if [ $1 = 0 ]; then
	/sbin/chkconfig --del vz >/dev/null 2>&1
	/sbin/chkconfig --del vzeventd >/dev/null 2>&1
	sed -i -e '/^exclude=kernel$/d' \
		-e '/^# Added by OpenVZ/d' /etc/yum.conf
fi

%package core
Summary: OpenVZ containers control utility core
Group: System Environment/Kernel
Requires: libxml2
Obsoletes: vzctl-lib
# these reqs are for vz helper scripts
Requires: bash
Requires: gawk
Requires: sed
Requires: grep
# requires for bash_completion and vztmpl-dl
Requires: wget

%description core
OpenVZ containers control utility core package

%files core
%{_libdir}/libvz*.so
%dir %{_vzdir}/lock
%dir %{_vzdir}/dump
%dir %{_vzdir}/private
%dir %{_vzdir}/root
%dir %{_vzdir}/template/cache
%dir %{_sharedstatedir}/vzctl/veip
%dir %{_sharedstatedir}/vzctl/vzreboot
%dir %{_sharedstatedir}/vzctl/vepid
%dir %{_configdir}
%dir %{_configdir}/names
%dir %{_vpsconfdir}
%dir %{_distconfdir}
%dir %{_distscriptdir}
%dir %{_vzdir}
%ghost %{_sharedstatedir}/vz
%{_sbindir}/vzctl
%{_sbindir}/arpsend
%{_sbindir}/ndsend
%{_sbindir}/vztmpl-dl
%{_sysconfdir}/logrotate.d/vzctl
%{_distconfdir}/distribution.conf-template
%{_distconfdir}/default
%{_distscriptdir}/*.sh
%{_distscriptdir}/functions
%{_mandir}/man8/vzctl.8.*
%{_mandir}/man8/arpsend.8.*
%{_mandir}/man8/ndsend.8.*
%{_mandir}/man5/ctid.conf.5.*
%{_mandir}/man5/vz.conf.5.*
%dir %{_libexecdir}/vzctl
%dir %{_libexecdir}/vzctl/scripts
%{_scriptdir}/vps-functions
%{_scriptdir}/vps-net_add
%{_scriptdir}/vps-net_del
%{_scriptdir}/vps-netns_dev_add
%{_scriptdir}/vps-netns_dev_del
%{_scriptdir}/vps-create
%{_scriptdir}/vzevent-stop
%{_scriptdir}/vzevent-reboot
%{_scriptdir}/vps-pci
%{_scriptdir}/vps-prestart
%{_scriptdir}/vps-cpt
%{_scriptdir}/vps-rst
%{_scriptdir}/vps-rst-env
%{_scriptdir}/vz-postinstall
%{_configdir}/conf
%config(noreplace) %{_configdir}/vz.conf
%config(noreplace) %{_configdir}/osrelease.conf
%config(noreplace) %{_configdir}/download.conf
%config(noreplace) %{_configdir}/oom-groups.conf
%config(noreplace) %{_distconfdir}/*.conf
%config %{_vpsconfdir}/ve-basic.conf-sample
%config %{_vpsconfdir}/ve-light.conf-sample
%config %{_vpsconfdir}/ve-vswap-256m.conf-sample
%config %{_vpsconfdir}/ve-vswap-512m.conf-sample
%config %{_vpsconfdir}/ve-vswap-1024m.conf-sample
%config %{_vpsconfdir}/ve-vswap-1g.conf-sample
%config %{_vpsconfdir}/ve-vswap-2g.conf-sample
%config %{_vpsconfdir}/ve-vswap-4g.conf-sample
%config %{_vpsconfdir}/0.conf

%post core
/sbin/ldconfig

%postun core
/sbin/ldconfig

%changelog
* Mon Aug 24 2015 Sergey Bronnikov <sergeyb@openvz.org> - 4.9.4-1
- store VE layout to VE config on start
- store VE layout in VE config during create and convert

* Mon Jul 27 2015 Kir Kolyshkin <kir@openvz.org> - 4.9.3-1
- set_console.sh: ubuntu 15.04 systemd console support
- vzoversell: handle unlimited RAM CTs
- vzctl umount: fix exit code if CT is running
- vz.conf, vps-net_add: add FORCE_ROUTE to change existing route to CT

* Mon May 11 2015 Kir Kolyshkin <kir@openvz.org> - 4.9.2-1
- store_devnodes: fix NULL deref (#3228)
- vps-create.sh: use stat -f instead of df
- vzctl.spec: require attr package

* Thu Apr 30 2015 Kir Kolyshkin <kir@openvz.org> - 4.9.1-1
- create_hardlink_dir(): fix wrong owner/perms case (#3222)
- vzctl.spec: drop the "Conflicts: vzkernel" (#3219)

* Wed Apr 22 2015 Kir Kolyshkin <kir@openvz.org> - 4.9-1
- New functionality and important changes:
-- vzmigrate: check CPU caps for suspended CT
-- suse-{add,del}_ip.sh: support for IP mask
-- vz.conf: allow list of interfaces in NEIGHBOUR_DEVS (#1289, #3192)
-- Introduce funtoo-set_hostname (#3097)
-- vz-postinstall: add a way to disable stock distro kernels from repos
-- vzctl set --devnodes|--devices: made cumulative, fix
-- vzctl set --devnodes: remove devices from CT
-- vzctl start/resume: load kernel modules needed for CT
-- vzctl create: disallow VE_PRIVATE be a mount point (#3166)
-- vzevent: try to run a script for all known events
-- vzctl restore|resume: add --skip-fsck
- Fixes:
-- redhat-add_ip.sh: support for Fedora 21 and RHEL/CentOS 7.1 (#3169)
-- vzctl snapshot-delete: ignore ploop 'no guid found'
-- suse-add_ip.sh: fix for venet routing in SUSE 13.2
-- osrelease.conf: add suse 13.2
-- vzctl chkpnt: workaround for ENOSPC
-- ct_enter(): enter mnt namespace last (#3038)
-- vzmigrate: fix for vzfsync if VE_PRIVATE differs (#3170)
-- init.d/vz-gentoo: fix a typo
-- vzctl.spec: fix iptables checking for RHEL5 (#2755)
-- vzmigrate: use DUMPDIR for CT dump (#3054)
-- vzmigrate: don't hardcode /vz/lock, use LOCKDIR (#2976)
-- vzmigrate: use C locale (#3049)
-- vzlist: fix cpuunits rounding (#3120)
-- snapshot-switch --must-restore: fix restoring config
-- fs_create: lock private
-- vps_create: minor fixes to cleanup logic
-- make_dir_mode(): ignore EEXIST from mkdir()
-- vzlist -j: output valid JSON for no CTs
-- init.d/vz-redhat: fix exit codes according to LSB (#3195)
- Improvements:
-- vzmigrate: random ports for ploop copy (#3052)
-- vzctl start: close extra fds later (#3091)
-- vzctl start: mkdir /proc in CT if needed (#3091)
-- vzctl create: fix an error message
-- vzctl.spec: require recent RHEL6 kernel (#3094)
-- init.d/vz*: load pio_kaio
-- suse-add_ip.sh: fix a warning
-- suse-del_ip.sh: remove venet routes
-- init.d/vz-redhat: fix a bashism (#3148)
-- vzctl delete: do rm config/dump even if failed to rm VE_PRIVATE
-- dists/scripts/{funtoo,gentoo}*: remove env var doc
-- debian-add_ip.sh: silent an error
-- vzeventd: ignore non-existent event scripts
- Documentation:
-- vzeventd(8): document new behavior
-- vzcptcheck(8): describe caps check w/o CTID
-- vz.conf(5): describe new NEIGHBOUR_DEV syntax

* Mon Oct  6 2014 Kir Kolyshkin <kir@openvz.org> - 4.8-1
- New functionality and important changes:
-- vzctl set: add NUMA --nodemask (sponsored by FastVPS)
-- vzmigrate: speed up by using se ploop copy with feedback if available
-- vzmigrate: speed up by reusing ssh connection
-- init.d/vz: show CT stop status
-- init.d/vz: implement parallel CT start (#2954, #2084)
-- init.d/vz, vz.conf: use/expose VE_PARALLEL
-- vzctl start,restore: add --skip-remount
-- vzctl snapshot-switch: add --must-restore
-- vzmigrate: ability to run ploop copy with timestamps
- Fixes:
-- vzctl.spec: disable VE0 conntracks only if unused (#2755)
-- vzmigrate: fix for --snapshot (#2907)
-- vzmigrate: don't run vzfsync if there is no need (#3055)
-- vzmigrate: undo_lock if check_cpt_props failed
-- vzmigrate: don't exit 1 on success
-- vzlist: fix showing DISABLED (#3029)
-- vzlist: fix cpulimit rounding (#3063)
-- redhat-set_hostname.sh: fix for F15+/RHEL7 (#3051)
-- vzctl compact: use built-in PATH (#2990)
-- postcreate.sh: fix caps for suexec
- Improvements:
-- debian-add_ip.sh: support for Ubuntu 14.04
-- postcreate.sh: add RHEL7/CentOS7 support
-- vzctl create --diskinodes: check for max ploop size
-- vzctl set --ostemplate: require --save (#2909)
-- vzmigrate: don't specify default cipher
-- vzmigrate: use getopt for option parsing
-- vzmigrate: detect "can't lock CT" error
-- vzmigrate: don't use rsync --delete-excluded
-- cpumask: allow for up to 4096 CPUs
-- vz_setcpu(): don't ignore errors from set_cpu*
-- fixed a few memory leaks and non-closed fds reported by Coverity
-- compare_osrelease(): fix for 3.x kernels
-- parse_{chkpnt,restore}_opt: don't print error twice
-- parse*opt(): add/improve extra args check
-- vzctl create: improve "no ploop" error message
- Documentation:
-- vznnc(8): add
-- vzctl --help: fix iolimit
-- vzctl(8): document set --ostemplate (#2909)
-- vzctl(8): add --nodemask, --must-restore, --skip-remount
-- vzctl(8): improve --netfilter
-- vz.conf(5): document VE_PARALLEL
-- vzmigrate(8), vzmigrate --help: document --ssh-mux
- Build system:
-- setver.sh: check for ./configure to run autogen
-- setver.sh: abort if autogen.sh/configure fails

* Fri May  2 2014 Kir Kolyshkin <kir@openvz.org> - 4.7.2-1
- vzlist: don't complain about missing ploop-lib (#2952)
- setup_console: don't execute on older kernels (#2961)
- clean_hardlink_dir(): note unlink/rmdir errors
- vzctl(8): fix a typo

* Wed Apr  2 2014 Kir Kolyshkin <kir@openvz.org> - 4.7-1
- New functionality and important changes:
-- Disable conntrack for VE0 by default (#2755)
-- vzctl set --diskspace: add --offline-resize (#2281)
-- vzctl create: use ploop by default
-- vzctl create, vzctl convert: honor diskinodes for ploop (#2898)
-- vzctl create: add --diskinodes
-- vzctl set: new option --netfilter to replace --iptables
-- vzmigrate: support for copying CT dump file
-- vzmigrate: introduce/use vzfsync for ploop (to shorten CT freeze time)
-- bash-completion: CTIDs on ploop for compact
-- vzctl create: honor MOUNT_OPTS
-- vzctl console: add set_console dist script (#2865)
-- vzctl snapshot-switch: add --skip_arpdetect option
-- vzctl snapshot-switch: add --skip-resume, --skip-config
-- vzctl set --diskinodes, DISKINODES: allow suffixes (KMG)
-- vzpid: new option "-p" to show in-container PID(s)
-- etc/vz.conf: add SKIP_ARPDETECT example
-- etc/vz.conf: use ploop by default
-- etc/vz.conf: use vswap config by default
-- etc/vz.conf: merge IP6TABLES to IPTABLES_MODULES
- Fixes:
-- vzctl destroy: fix locking (#2814)
-- debian-add_ip.sh: setup loopback device at least (#2859)
-- vzctl start --wait: fix for non-standard Debian 7
-- postcreate.sh: add Fedora 20+
-- postcreate.sh: set file caps for suse 13.1+
-- vzmigrate: fix ploop for diff VE_PRIVATE case (#2875)
-- vzmigrate: hide ploop getdev output
-- vzctl status, snapshot-list: don't mess with stdout even when verbose
-- vzlist: don't spoil output with ploop messages
-- logger.c: fix wrt ploop logging
-- etc/network/if-up.d/vzifup-post: fix for Debian Wheezy (#2914)
-- hooks_ct.c: bind-mount root to itself (fix for kernel v3.11+)
-- hooks_ct: mount /proc and /sys before umounting old root
-- bash_completion: replace exit with return
-- bash_completion.d: add --quiet to vzctl
-- vzmigrate: call vzctl status with --quiet
-- vzcptcheck: fix program name in usage
- Improvements:
-- add_reach_runlevel_mark(): improve error messages
-- set(): don't ignore fail from fill_vswap_ub()
-- vzctl restore: warn in CPT_SET_LOCKFD2 not supported
-- config.c: add SKIP_ARPDETECT to ignored list
-- vzmigrate: lock CT locally
-- vzmigrate --live: check for running CT earlier
-- destroy_dump(): don't log "Removing" if no dump
-- etc/conf/*sample: tune DISKINODES for ploop diskspace/diskinodes ratio
-- etc/conf/ve-unlimited.conf-sample: remove
-- hooks_ct: remove non-working devpts mount
-- hooks_ct: mount devtmpfs in CT
-- ct_chroot(): do not change a set of CT0's mounts
-- parse_netif_str(): improve NETIF= param parsing
-- setup_hardlink_dir(): show error if mkdir() failed
- Documentation:
-- vzctl(8): document MAX_VEID (#2784)
-- vzctl --help: fix convert synopsys
- Build system:
-- setver.sh: rework buildid
-- setver.sh: make it work on fresh git source
-- autogen.sh, setver.sh: fix build from screwed git repo
-- vzctl.spec: require bridge-utils (as we use brctl)

* Wed Nov 27 2013 Kir Kolyshkin <kir@openvz.org> - 4.6.1-1
- Fixes:
-- vzctl set: require swap to be set for VSwap
-- fill_vswap_ub(): fix a potential segfault
-- ndsend: clear reserved2 field (#2804)
-- vzubc: fix to work in old mawk (#2793)
-- vzlist: fix bogus CTIDs in list (#2830)
-- vzctl start: don't fail if VE_ROOT does not exist (#2807)

* Tue Oct 29 2013 Kir Kolyshkin <kir@openvz.org> - 4.6-1
- New functionality:
-- Add iolimit and iopslimit (need kernel >= 042stab084.2)
-- Add optional VM_OVERCOMMIT/--vm_overcommit parameter
-- In VSwap mode, set some secondary UBCs if unset:
--- lockedpages=oomguarpages=ram
--- vmguarpages=ram+swap
--- privvmpages=(ram+swap)*vm_overcommit (if set)
-- vzoversell: add
-- vztmpl-dl: add --list-orphans
-- vztmpl-dl: add --quiet/--no-quiet
-- vzubc: don't show unlimited ubcs by default; add -v to show
-- vzlist: add new fields (vm_overcommit, iolimit, iopslimit)
- Fixes:
-- Fix quota on ploop for RHEL5 CT
-- vzctl console: hack to force redraw on reattach
-- set_ublimit(): don't set unknown UBs to unlim (#2760)
-- init.d/vzeventd: set reboot_event (#2764)
-- arch.conf: add POST_CREATE (#2371)
-- configure: fix libdir for Debian/Ubuntu case
-- ct_env_create_real(): fix build for IA64
-- vzctl create, vzctl exec: do skip fsck
-- init.d/vz-gentoo: fix setting default for NET_MODULES and PLOOP_MODULES
-- init.d/vz-redhat: don't reset cpulimits for all CTs
- Improvements:
-- Add a way to not modify sysctl.conf on installation (#2375)
-- vzctl set --reset_ub: only allow for running CT
-- init.d/vzeventd-redhat: switch to strict bash
-- vz-postinstall: don't add bridge params to sysctl.conf
-- vzlist: skip mounted status check if not needed
-- vzubc: print errors to stderr
-- vzctl start: don't start CT if /proc mount failed
-- vzevent-stop: check for suspend/chkpnt
-- init.d/vz*: unset io limits before stopping CT
-- [build] setver.sh: add build_id, use getopt
-- assorted minor code improvements
- Documentation:
-- vzctl(8), ctid.conf(5): document vm_overcommit
-- vzctl(8): fix per-CT action script prefix
-- vz.conf(5): LOGFILE don't have a default
-- man: don't hardcode configurable paths
-- vzlist(8): fix a subsection reference
-- vzlist(8): fix indentation

* Wed Aug 28 2013 Kir Kolyshkin <kir@openvz.org> - 4.5.1-1
- Fixes:
-- Fix loading older (<1.9) ploop library (#2719)
-- Fix installing rpm for people using /var/lib/vz (#2722)

* Fri Aug 23 2013 Kir Kolyshkin <kir@openvz.org> - 4.5-1
- New functionality:
-- vztmpl-dl: add --upload-all, --ignore-errors
-- vztmpl-dl: add --list-remote, --list-local
-- vztmpl-dl: do not check GPG signatures by default
-- vztmpl-dl: add --gpg-check and --update options
-- vz-postinstall: enable iptables for bridges (#2641)
-- vz-postinstall: be verbose about what we do
-- vzmigrate: support for VE_PRIVATE being a symlink (#2694)
- Fixes:
-- ndsend: fix option field in sending packets (#2709)
-- libvzchown: link to -ldl (#2705)
-- vps_create(): save LOCAL_UID/GID=0 if !userns for upstream CT
-- vzctl.spec: run vz-postinstall on a fresh install only
-- vz-postinstall: do not change rp_filter sysctl
-- vzmigrate: remove a bashism
-- vzctl create: fix running postcreate action wrt --ostemplate path/tmpl
-- vzctl create: use proper version of basename()
-- vzdaemon_stop(): don't return error if stopped already
-- read_resolv_conf(): fix potential buffer overflow
-- vzctl_env_switch_snapshot: fix leak on error path
-- vzctl_env_convert_ploop(): check chmod return code
- Improvements:
-- veth: improve veth random MAC generation (#2695)
-- vzctl start: always mount /dev/pts for upstream CT
-- vzmigrate: add / to paths for rsync (#2686)
-- load_ploop_lib(): load .so.1, try .so too (for ploop-1.9)
-- scripts: use VPSCONFDIR instead of PKGCONFDIR/conf
-- vzctl.spec: add /var/lib/vz as a symlink to /vz
-- vzctl.spec: don't mark symlink as %dir
-- vzctl.spec: remove a bunch of defines
-- vzctl.spec: use %_sharedstatedir not /var/lib
-- vzctl.spec: quote rpm macros
-- vzctl.spec: remove extra slashes
- Documentation:
-- vztmpl-dl: improve usage
-- vztmpl-dl(8): describe new options

* Wed Jul 17 2013 Kir Kolyshkin <kir@openvz.org> - 4.4-1
- New functionality:
-- vztmpl-dl script to aid in template downloading/updating
-- nameserver/searchdomain auto-propagation from the host (#2301)
-- vzctl start: do fsck for ploop, add --skip-fsck (#2615)
-- add --stop-timeout/STOP_TIMEOUT option (#2621)
-- vzmigrate: use remote VZ_PRIVATE and VE_ROOT (#2523)
-- Introduce vz-postinstall script (set sysctl.conf, disable selinux)
-- vzmigrate: add -f, ability to ignore some checks (#2643)
-- distscripts: update for newer Arch Linux (#2617)
-- etc/vz.conf: set default OS template to centos-6-x86
-- etc/vz.conf: comment out NEIGHBOUR_DEVS by default
- Fixes:
-- vzmigrate: fix check for IPs when there are none (#2620)
-- Deny "unlimited" value for DISKSPACE/DISKINODES
-- scripts/vps-netns_dev_add: rework config action (#2637)
-- vzctl convert: fix final renames (#2638)
-- vzctl convert: rename old private back if failed (#2638)
-- vzctl convert: fix new directory mode to be 0700 not 0600
-- scripts/vps-rst: make VE_VETH_DEVS optional (#2659)
-- fix compilation on arches without support for VZ (RH #971821)
-- vzlist -j: fix to work on RHEL5 kernel (#2661)
-- fix exec to really enter into pidns on upstream kernel (#2658)
-- debian-add_ip.sh: ignore comments when looking for venet0 (#2674)
-- destroydir(): don't return -1
-- create.c: fix warnings compiling w/o ploop
-- build fix for automake < 1.10.2
- Improvements:
-- vzmigrate: check ipv6 module on dest (#2555)
-- Remove check for ploop size (let ploop decide)
-- vzmigrate: improve invalid cmdline handling
-- [build] configure: set localstatedir to w/o prefix (#2637#c2)
- Documentation:
-- add vztmpl-dl(8)
-- vzctl(8), vz.conf(5), ctid.conf(5): "inherit" for nameserver/searchdomain
-- vzctl(8): describe new options --skip-fsck, --stop-timeout
-- vzmigrate(8): describe new option -f/--nodeps
-- vzmigrate(8): remove duplicate --live option description
-- vzmigrate --help: simplify synopsys

* Mon Jun  3 2013 Kir Kolyshkin <kir@openvz.org> - 4.3.1-1
- New functionality:
-- vzctl restore with CRIU: restore veth devices
- Fixes:
-- vzmigrate: fix a typo leading to missing `]' warning (harmless)
-- configure.ac: set _GNU_SOURCE for older autoconf
-- vzctl stop: don't kill CT right away if halt exited with 1
-- vzctl restore/start: fix running mount script (#2603)
-- vps_start_custom(): close old_wait_p fds
-- stat_file(): print error if other than ENOENT
-- vzctl snapshot-switch: do apply config saved on snapshot
-- vzctl snapshot-switch: don't remove dump file
-- fix checking stat_file() return code
-- vzctl create: umount ploop device if interrupted
-- src/snapshot.c: log errno after failed rename
-- vzctl start/destroy: fix criu dump removal
-- vzctl restore: synchronize criu with vzctl
-- vzctl --help: fix copyright years
- Improvements:
-- logger(): don't spoil errno
-- Macro GET_DUMP_FILE is internal, move to .c
-- is_vzquota_available(): use access() and check for x bit
-- stat_file(): use access() instead of stat()
-- vzctl_env_[u]mount_snapshot: rm guid check
-- vzctl_env_create_snapshot(): explicitly specify guid on rollback
-- vzctl_env_switch_snapshot(): rework using ploop_switch_snapshot_ex()
-- vzctl restore: more consistent error printing
- Documentation:
-- man: fix pages' dates

* Fri May 24 2013 Kir Kolyshkin <kir@openvz.org> - 4.3-1
- New functionality:
-- vzctl enter/exec now works for upstream kernel 3.8+
-- vzctl snapshot-[u]mount
-- user namespace support for upstream kernel 3.9+
-- vzctl suspend/resume: support upstream 3.x kernel via CRIU (http://criu.org)
-- vzmigrate: add compatibility pre-checks for CPT version and CPU flags
-- Add vzstats dependency to rpm package
- Improvements:
-- vzctl: introduce cleanup handler mechanism, use for ploop, scripts etc.
-- vzctl start: add pre-start dist script
-- vzctl start: remove dumpfile on successful start
-- vzmigrate: add -o BatchMode=yes to SSH_OPTIONS
-- vzctl console: recognize ESC as a first character
-- add vzctl itself to OOM group configuration
-- bash-completion: add vzctl snapshot-list options
-- bash-completion: add vzctl snapshot-* --id/--uuid argument
-- vzctl set --reset_ub: make exclusive
-- vzctl set: on fail don't warn about missing --save
-- etc/init.d/vz*: try to run vzstats
-- vzmigrate: add --check-only (aka --dry-run)
-- Move container private area check after executing premount scripts
- Fixes:
-- vzctl snapshot-list -o desc,device: fix width
-- vzmigrate: fix ploop-based CT migration wrt symlinks
-- vzmigrate: improve a few log messages
-- vzmigrate: fix and optimize IP address checks
-- vzmigrate: fix checking rsync/vzctl exit code
-- vps_destroy_dir(): don't call quota on ploop CT
-- suse-add_ip.sh: remove a bogus warning in no IPs case
-- src/lib/cpt.c:restore_fn(): log errno
-- Many (about 40) fixes here and there, found by Coverity
-- destroydir(): log errno
-- vzctl set 0 ... --force: don't SEGV on non-ovz kernel
-- vzctl set --force: require --save
-- vzctl set --diskspace: require --save for ploop
-- vps-download: fix config file in --config output
-- vzlist -o vswap: fix
-- vzctl start: fix ub limits setting for upstream containers
-- vzctl restore: don't run action scripts
-- Fix checking vps_is_mounted() return value
-- Remove more traces of noatime flag
- Documentation:
-- vzcptcheck(8): added
-- vzctl(8): note vzctl set --name requires --save
-- vzctl(8): improve --setmode description
-- vzctl(8): fix and improve description of set --userpasswd
-- vzctl(8): document snapshot-mount, snapshot-umount
-- vzctl(8): document --local-gid, local-uid
-- distribution.conf-template: document PRE_START
-- other fixes and improvements

* Fri Feb 15 2013 Kir Kolyshkin <kir@openvz.org> - 4.2-1
- New functionality:
-- Support for Fedora 18 in container (devices, disk quota, venet IPs, caps)
-- vzctl snapshot-list: add options a la vzlist (see --help or man for details)
- Improvements:
-- vzctl create: allow existing empty VE_PRIVATE (#2450)
-- vzctl stop/reboot: disable fsync in CT
-- vzctl: fix check for VEID_MAX
-- vzctl --ipadd: IPv6 support for etcnet (ALT Linux) (#2482)
-- vzlist: more strict check for cmdline-supplied CTIDs
-- vzlist: warn/skip invalid CTIDs in ve.conf files (#2514)
-- vzevent: do umount CT in case of reboot (#2507)
-- init.d/vz-redhat: stop vz earlier (#2478)
-- init.d/vz-gentoo: don't call tools by absolute path (#2477)
-- vzubc: add -wt option (add -t to invoked watch) (#2474)
-- vzubc: remove check for watch presence
-- vzctl.spec: cleanups, fixes, improvements
-- vzctl set --devnodes: add /usr/lib/udev/devices
-- minor code cleanups
- Fixes:
-- vzlist: fix segfault for ploop-based CT with no DISKINODES set (#2488)
-- vzlist --json: fix showing disk usage for non-running CTs
-- vzlist -o cpus: do not overwrite runtime value
-- vzlist --json: skip collecting numcpu info on old kernel
-- vzubc: fix -w/-c check
- Documentation:
-- man/*: correct path to scripts
-- vzctl(8): add missing CTID to SYNOPSYS
-- vzctl(8): document new snapshot-list options

* Tue Jan  1 2013 Kir Kolyshkin <kir@openvz.org> - 4.1.2-1
- Regressions:
-- etc/init.d/vz-gentoo: fix missing VZREBOOTDIR (#2467)
-- fix extra arguments parsing by add-on modules (#2428)
-- do not whine about unknown VE_STOP_MODE parameter
- Bug fixes:
-- load_ploop_lib(): prevent buffer overflow with newer ploop-lib

* Fri Dec  7 2012 Kir Kolyshkin <kir@openvz.org> - 4.1.1-1
- Regressions:
-- etc/init.d/vz*: fix accidental start of all CTs (#2424)
-- etc/init.d/vz*: do not auto-start CTs marked with ONBOOT=no (#2456)
-- init.d/vz*: only apply oom score if appropriate /proc file exist (#2423)
- Fixes:
-- vzctl set --devnodes: add /usr/lib/udev/devices
-- vzlist --json: skip collecting numcpu info on old kernel
- Improvements:
-- vz.conf, init.d/vz*: support for VE_STOP_MODE global parameter (#2432)
-- enable build for architectures not supported by OpenVZ kernel
-- vzlist: show if onboot field is unset
- Documentation:
-- vz.conf(5): describe VE_STOP_MODE
-- vzctl(8), ctid.conf(5): fix ONBOOT/--onboot description

* Thu Nov  1 2012 Kir Kolyshkin <kir@openvz.org> - 4.1-1
- New features
- * etc/init.d/vz: restore running containers after reboot (#781)
- * etc/init.d/vz: faster restart by doing CT suspend instead of stop (#2325)
- * vzctl start: try to restore CT first if default dump file exists
- * Add OOM adjustments configuration (see /etc/vz/oom-groups.conf)
- * If a CT is locked, show pid and cmdline of a locker
- * vzctl snapshot: add --skip-config option
- * vzctl: add 'suspend' and 'resume' aliases (for 'chkpnt' and 'restore')
- Fixes
- * vzctl snapshot: fix storing CT config file
- * vzctl snapshot-switch: fix restoring CT config file
- * vps-create: fix checking needed disk space (#2413)
- * vzctl set --mount_opts: fix a segfault (#2385)
- * suse-add_ip.sh: only set default route if there is no other (#2376)
- * set_userpass.sh: fix a bashism (#2403)
- * etc/init.d/vz*: eliminate "Container(s) not found" msg
- * etc/init.d/vz*: fix vzlist invocation in stop_ve(s)
- * etc/init.d/vz-redhat: mark more local vars as such
- * vzctl_resize_image(): initialize ploop_resize_param
- * getlockpid(): fix potential buffer overflow
- * Do not call xmlCleanupParser() from vzctl
- * Fixed compilation with libcgroup-0.37-r2 (#2370)
- * Properly return errors in cgroup_init() (#2372)
- * Print failures in ct_do_open directly to stderr
- * vzeventd: do process -h option
- Improvements
- * etc/init.d/vz* stop: set cpuunits for all CTs at once
- * vzctl snapshot*: improve --id parameter parsing
- * vzctl umount: handle the case when CT have deleted mount points
- * vzevent-stop: add workaround for Fedora 17 reboot problem (#2336)
- * vzctl restore: do not print "Starting container"
- * vzctl restore: print 'restore failed' not 'start failed'
- * scripts/vps-download: fix bogus warning from checkbashisms
- * vzctl_merge_snapshot(): simplify return code handling
- * Simplify ct_chroot() (no need to umount each mount point)
- Documentation
- * vzctl(8): improved vzctl create --layout/--diskspace description
- * vzctl(8): improve --diskspace description
- * vzctl(8): disambiguate 'it' in snapshot-switch description
- Build system
- * configure: add ability to alter /vz path (#421)
- * src/Makefile.am: fix building with builddir != srcdir (#2375)
- * Makefile.am: use AM_CPPFLAGS (not AM_CFLAGS)
- * properly propagate /var/lib/vzctl/veip dir
- * setver.sh: restore original configure.ac and vzctl.spec if building
- * setver.sh: clean up dist tarball (if building) and rpms (if installing)
- * setver.sh: add -o|--oldpackage option
- * other minor improvements

* Tue Sep 25 2012 Kir Kolyshkin <kir@openvz.org> - 4.0-1
- New features
- * Ability to work with non-openvz kernel (experimental,
    see http://wiki.openvz.org/Vzctl_for_upstream_kernel)
- * vzlist: add JSON output format (--json flag)
- * vzctl compact: implement (to compact ploop image)
- * vzctl snapshot: store/restore CT config on snapshot create/switch
- * vzctl set: add --mount_opts to set mount options for ploop
- * Implement dynamic loading of ploop library
- * Implement ability to build w/o ploop headers (./configure --without-ploop)
- * Split into vzctl-core and vzctl packages, removed vzctl-lib
- * Scripts moved from /usr/lib[64]/vzctl/scripts to /usr/libexec/vzctl
- * Added dists/scripts support for Alpine Linux
- Fixes
- * postcreate.sh: create /etc/resolv.conf with correct owner and perms (#2290)
- * vzctl --help: add snapshot* and compact commands
- * vzctl set --capability: improve cap setting code, eliminate kernel warning
- * vzctl set --quotaugidlimit: fix working for ploop after restart
- * vzctl start|enter|exec: eliminate race when checking CT's /sbin/init
- * vzlist, vzctl set --save: avoid extra delimiter in features list
- * vzlist: return default to always print CTID (use -n for names) (#2308)
- * vzmigrate: fix for offline migration of ploop CT (#2316, #2356)
- * vzctl.spec: add wget requirement (for vps-download)
- * osrelease.conf: add ubuntu-12.04 (#2343)
- * init.d/vz-redhat: fix errorneous lockfile removal (#2342)
- * suse-add_ip.sh: do not set default route on venet0 when no IPs (#1941)
- * arch-del_ip.sh: fixed for /etc/rc.conf case (#2367)
- * arch-{add,del}_ip.sh: updated to deal with new Arch netcfg (#2280)
- * configure.ac: on an x86_64, install libraries to lib64
- * Build system: fix massively parallel build (e.g. make -j88)
- Improvements
- * init.d/vz*: stop CTs in the in the reverse order of start (#2330)
- * init.d/vz-redhat: add /vz to PRUNEPATHS in /etc/updatedb.conf
- * bash-completion: add remote completion for --ostemplate
- * bash_completion: complete ploop commands only if supported by the kernel
- * vzctl: call set_personality32() for 32-bit CTs on all architectures
- * vzctl console: speed up by using bigger buffer
- * vzctl chkpnt: fsync dump file
- * vzctl mount,destroy,snapshot-list: error out for too many arguments
- * vzctl set --diskinodes: warn it's ignored on ploop
- * vzctl set --hostname: put ::1 below 127.0.0.1 in CT's /etc/hosts (#2290)
- * vzctl set: remove --noatime (obsolete now when relatime is used)
- * vzctl snapshot: added check for snapshot guid dup
- * vzctl snapshot-delete: fix error code
- * vzctl start/stop: print error for non-applicable options
- * vzctl status: do not show 'mounted' if stat() on root/private fails
- * vzctl status: do not show 'suspended' for running container
- * vzctl stop: various minor improvements
- * vzlist: add the following new fields:
    nameserver, searchdomain, vswap, disabled, origin_sample, mount_opts
- * vzlist, vzctl status: speed up querying mounted status
- * vzlist: faster ploop diskspace info for unmounted case
- * vzmigrate: rename --online to --live
- * vzmigrate: do not use pv unless -v is specified
- * vzmigrate: do not lose ACLs and XATTRS (#2056)
- * vzmigrate: dump/restore first-level quota
- * switch to new ploop_read_disk_descr()
- * is_ploop_supported(): reimplement using /proc/vz/ploop_minor
- * Code refactoring, moving vz- and upstream-specific stuff to hooks_{vz,ct}.c
- * Various code cleanups

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
