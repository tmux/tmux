#!/bin/sh

sh autogen.sh || exit 1
if [ "$BUILD" = "static" ]; then
	./configure --enable-static || exit 1
else
	./configure || exit 1
fi
exec make
