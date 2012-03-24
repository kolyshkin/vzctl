#!/bin/sh

set -e
aclocal
libtoolize --force --copy --automake
automake -afc
./setver.sh
autoconf
rm -rf autom4te.cache/
