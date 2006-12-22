#!/bin/sh

set -e

aclocal
libtoolize --force --copy --automake
automake -afc
autoconf
rm -rf autom4te.cache/
