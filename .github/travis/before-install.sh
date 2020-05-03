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
fi
