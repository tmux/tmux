#!/bin/sh

if [ "x$(uname)" = "xOpenBSD" ]; then
	[ -z "$AUTOMAKE_VERSION" ] && export AUTOMAKE_VERSION=1.15
	[ -z "$AUTOCONF_VERSION" ] && export AUTOCONF_VERSION=2.69
fi

die()
{
    echo "$@" >&2
    exit 1
}

mkdir -p etc
autoreconf -i || die "autoreconf failed"
