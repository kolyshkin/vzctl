#!/bin/sh

set -e
./setver.sh
aclocal
libtoolize --force --copy --automake
automake -afc
autoconf
rm -rf autom4te.cache/
