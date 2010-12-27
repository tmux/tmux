# $Id: Makefile,v 1.163 2010-12-27 22:13:35 tcunha Exp $
#
# Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
# IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
# OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

.SUFFIXES: .c .o
.PHONY: clean

VERSION= 1.5

FDEBUG= 1

CC?= cc
CFLAGS+= -DBUILD="\"$(VERSION)\""
LDFLAGS+= -L/usr/local/lib
LIBS+=

.ifdef FDEBUG
CFLAGS+= -g -ggdb -DDEBUG
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align
.endif

# This sort of sucks but gets rid of the stupid warning and should work on
# most platforms...
CCV!= (LC_ALL=C ${CC} -v 2>&1|awk '/gcc version 4|clang/') || true
.if empty(CCV)
CPPFLAGS:= -I. -I- -I/usr/local/include ${CPPFLAGS}
.else
CPPFLAGS:= -iquote. -I/usr/local/include ${CPPFLAGS}
.ifdef FDEBUG
CFLAGS+= -Wno-pointer-sign
.endif
.endif

PREFIX?= /usr/local
INSTALL?= install
INSTALLDIR= ${INSTALL} -d
INSTALLBIN= ${INSTALL} -m 555
INSTALLMAN= ${INSTALL} -m 444

SRCS!= echo *.c|LC_ALL=C sed 's|osdep-[a-z0-9]*.c||g'
.include "config.mk"
OBJS= ${SRCS:S/.c/.o/}

.c.o:
		${CC} ${CPPFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

all:		tmux

tmux:		${OBJS}
		${CC} ${LDFLAGS} -o tmux ${OBJS} ${LIBS}

depend:
		mkdep ${CPPFLAGS} ${CFLAGS} ${SRCS:M*.c}

clean:
		rm -f tmux *.o *~ *.core *.log compat/*.o compat/*~

clean-depend:
		rm -f .depend

clean-all:	clean clean-depend
		rm -f config.h config.mk

install:	all
		${INSTALLDIR} ${DESTDIR}${PREFIX}/bin
		${INSTALLBIN} tmux ${DESTDIR}${PREFIX}/bin/
		${INSTALLDIR} ${DESTDIR}${PREFIX}/man/man1
		${INSTALLMAN} tmux.1 ${DESTDIR}${PREFIX}/man/man1/
