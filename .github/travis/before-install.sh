#!/bin/sh

if [ "$TRAVIS_OS_NAME" = "linux" ]; then
	sudo apt-get update -qq
	sudo apt-get -y install bison \
				autotools-dev \
				libncurses5-dev \
				libevent-dev \
				pkg-config \
				libutempter-dev \
				build-essential

	if [ "$BUILD" = "musl" -o "$BUILD" = "musl-static" ]; then
		sudo apt-get -y install musl-dev \
					musl-tools
	fi
fi

if [ "$TRAVIS_OS_NAME" = "freebsd" ]; then
	sudo pkg install -y \
		automake \
		libevent \
		pkgconf
fi
