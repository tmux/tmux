#!/bin/sh

BUILD=$PWD/build

LIBEVENT=https://github.com/libevent/libevent/releases/download/release-2.1.11-stable/libevent-2.1.11-stab\
le.tar.gz
NCURSES=https://ftp.gnu.org/gnu/ncurses/ncurses-6.2.tar.gz

wget -4q $LIBEVENT || exit 1
tar -zxf libevent-*.tar.gz || exit 1
(cd libevent-*/ &&
	 ./configure --prefix=$BUILD \
		     --enable-shared \
		     --disable-libevent-regress \
		     --disable-samples &&
	 make && make install) || exit 1

wget -4q $NCURSES || exit 1
tar -zxf ncurses-*.tar.gz || exit 1
(cd ncurses-*/ &&
	CPPFLAGS=-P ./configure --prefix=$BUILD \
				--with-shared \
				--with-termlib \
				--without-ada \
				--without-cxx \
				--without-manpages \
				--without-progs \
				--without-tests \
				--without-tack \
				--disable-database \
				--enable-termcap \
				--enable-pc-files \
				--with-pkg-config-libdir=$BUILD/lib/pkgconfig &&
	 make && make install) || exit 1

sh autogen.sh || exit 1
PKG_CONFIG_PATH=$BUILD/lib/pkgconfig ./configure --prefix=$BUILD "$@"
make && make install || (cat config.log; exit 1)
