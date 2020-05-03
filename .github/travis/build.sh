#!/bin/sh

sh autogen.sh || exit 1
case "$BUILD" in
	static)
		./configure --enable-static || exit 1
		exec make
		;;
	all)
		sh $(dirname $0)/build-all.sh
		exec make
		;;
	*)
		./configure || exit 1
		exec make
		;;
esac
