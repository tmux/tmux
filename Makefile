# $Id: Makefile,v 1.146 2009-07-21 13:51:30 nicm Exp $

.SUFFIXES: .c .o
.PHONY: clean

VERSION= 1.0

FDEBUG= 1

CC?= cc
CFLAGS+= -DBUILD="\"$(VERSION)\""
LDFLAGS+= -L/usr/local/lib
LIBS+= -lcurses

.ifdef FDEBUG
CFLAGS+= -g -ggdb -DDEBUG
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align
.endif

# This sort of sucks but gets rid of the stupid warning and should work on
# most platforms...
CCV!= (LC_ALL=C ${CC} -v 2>&1|awk '/gcc version 4/') || true
.if empty(CCV)
CPPFLAGS:= -I. -I- -I/usr/local/include ${CPPFLAGS}
.else
CPPFLAGS:= -iquote. -I/usr/local/include ${CPPFLAGS}
.ifdef FDEBUG
CFLAGS+= -Wno-pointer-sign
.endif
.endif

PREFIX?= /usr/local
INSTALLDIR= install -d
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

SRCS!= echo *.c|sed 's|osdep-[a-z0-9]*.c||g'
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
		rm -f tmux *.o *~ *.core *.log compat/*.o

clean-depend:
		rm -f .depend

clean-all:	clean clean-depend
		rm -f config.h config.mk

install:	all
		${INSTALLDIR} ${DESTDIR}${PREFIX}/bin
		${INSTALLBIN} tmux ${DESTDIR}${PREFIX}/bin/
		${INSTALLDIR} ${DESTDIR}${PREFIX}/man/man1
		${INSTALLMAN} tmux.1 ${DESTDIR}${PREFIX}/man/man1/
