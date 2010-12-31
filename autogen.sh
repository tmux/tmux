#!/bin/sh
# $Id: autogen.sh,v 1.2 2010-12-31 22:13:48 nicm Exp $

[ -z "$AUTOMAKE_VERSION" ] && export AUTOMAKE_VERSION=1.10
[ -z "$AUTOCONF_VERSION" ] && export AUTOCONF_VERSION=2.65

die()
{
    echo "$@" >&2
    exit 1
}

mkdir -p etc
aclocal || die "aclocal failed"
automake --add-missing --force-missing --copy --foreign || die "automake failed"
autoreconf || die "autoreconf failed"
