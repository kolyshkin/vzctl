#!/bin/sh

while test -n "$1"; do
	case $1 in
	   -b|--build)
		build=yes
		;;
	   -i|--install)
		build=yes
		install=yes
		;;
	   -v|--verbose)
		verbose=yes
		;;
	   *)
		echo "Invalid argument: $1" 1>&2
		exit 1
	esac
	shift
done

CONFIGURE_AC=configure.ac
RPM_SPEC=vzctl.spec

# Try to figure out version from git
GIT_DESC=$(git describe --tags | sed s'/^[^0-9]*-\([0-9].*\)$/\1/')
GIT_V=$(echo $GIT_DESC | sed 's/-.*$//')
				# 3.0.28-1-gf784152
GIT_R=$(echo $GIT_DESC | sed 's/^[^-]*-\([1-9][0-9]*\)-g/\1.git./')
test "$GIT_V" = "$GIT_R" && GIT_R="1"
GIT_VR="${GIT_V}-${GIT_R}"

CONF_V=$(grep AC_INIT $CONFIGURE_AC | \
		sed 's/^[^,]*,[ ]\([^,]*\),.*$/\1/')

read_spec() {
	SPEC_V=$(awk '($1 == "Version:") {print $2}' $RPM_SPEC)
	SPEC_R=$(awk '($1 " " $2 == "%define rel") {print $3}' $RPM_SPEC)
	SPEC_VR="${SPEC_V}-${SPEC_R}"
}
read_spec

# Set version/release in spec from git
if test "$GIT_VR" != "$SPEC_VR"; then
	test -z "$verbose" || echo "Changing $RPM_SPEC:"
	# Version: 3.0.28
	# Release: 1%{?dist}
	sed -i -e "s/^\(Version:[[:space:]]*\).*\$/\1$GIT_V/" \
	       -e "s/^\(%define rel[[:space:]]*\).*\$/\1$GIT_R/" \
		$RPM_SPEC
	if test "$GIT_R" != "1"; then
		NVR='%{name}-%{version}-%{rel}'
		sed -i -e "s/^\(Source:[[:space:]]*\).*\$/\1${NVR}.tar.bz2/" \
		       -e "s/^%setup[[:space:]]*.*\$/%setup -n ${NVR}/" \
			$RPM_SPEC
	fi
fi
test -z "$verbose" || \
	grep -E -H '^Version:|^%define rel|^Source:|^%setup' $RPM_SPEC

# Set version in configure.ac from spec
read_spec
SPEC_VR=$(echo $SPEC_VR | sed 's/-1$//')
if test "$CONF_V" != "$SPEC_VR"; then
	test -z "$verbose" || echo "Changing $CONFIGURE_AC:"
	# AC_INIT(vzctl, 3.0.28, devel@openvz.org)
	sed -i "s/^\(AC_INIT(vzctl,[ ]\)[^,]*\(,.*\$\)/\1$SPEC_VR\2/" \
		$CONFIGURE_AC
	autoconf
fi
test -z "$verbose" || \
	grep -H '^AC_INIT' $CONFIGURE_AC

test "$build" = "yes" && make rpms
test "$install" = "yes" &&
	sudo rpm -Uhv $(rpm --eval %{_rpmdir}/%{_arch})/vzctl-*${GIT_VR}*.rpm

