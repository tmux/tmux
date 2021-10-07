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
	musl)
		CC=musl-gcc sh $(dirname $0)/build-all.sh
		exec make
		;;
	musl-static)
		CC=musl-gcc sh $(dirname $0)/build-all.sh --enable-static
		exec make
		;;
	*)
		./configure || exit 1
		exec make
		;;
esac
