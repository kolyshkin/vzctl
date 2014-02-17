#!/bin/sh

set -e
aclocal
libtoolize --force --copy --automake
automake -afc
autoconf
./setver.sh
autoconf
