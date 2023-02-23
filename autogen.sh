#!/bin/sh
#
# This script is derived from http://svn.abisource.com/abiword-sharp/trunk/autogen.sh.
#

aclocalinclude="-I m4"

#
#clean
#
rm -f Makefile Makefile.in aclocal.m4 config.sub configure intltool-* m4/* \
    libtool missing src/Makefile src/Makefile.in  config.guess config.status depcomp install-sh \
    ltmain.sh po/Makefile.in.in
rm -fr autom4te.cache config.log

mkdir -p m4 include
#
#
#
echo "Create empty aclocal.m4 if it's needed"
test -r ./aclocal.m4 || touch ./aclocal.m4

echo "Making ./aclocal.m4 writable ..."
test -r ./aclocal.m4 && chmod u+w ./aclocal.m4

echo "Running autoupdate ..."
autoupdate

echo "Running aclocal $aclocalinclude ..."
aclocal $aclocalinclude

echo "Running autoheader..."
autoheader

echo "Running automake --foreign $am_opt ..."
automake --add-missing --foreign --copy --force-missing

echo "Running autoconf ..."
autoconf
