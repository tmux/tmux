# $Id: GNUmakefile,v 1.96 2009-05-13 23:33:54 nicm Exp $

.PHONY: clean

VERSION= 0.9

FDEBUG= 1

CC?= gcc
CFLAGS+= -DBUILD="\"$(VERSION)\""
CPPFLAGS:= -I. -I- $(CPPFLAGS)
LDFLAGS+= -L/usr/local/lib
LIBS+= -lncurses

ifdef FDEBUG
LDFLAGS+= -rdynamic
CFLAGS+= -g -ggdb -DDEBUG
LIBS+= -ldl
ifeq ($(CC),gcc)
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align
endif
endif

PREFIX?= /usr/local
INSTALLDIR= install -d
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

SRCS= $(shell echo *.c|sed 's|osdep-[a-z0-9]*.c||g')
include config.mk
OBJS= $(patsubst %.c,%.o,$(SRCS))

all:		$(OBJS)
		$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

depend: 	$(SRCS)
		$(CC) $(CPPFLAGS) $(CFLAGS) -MM $(SRCS) > .depend

clean:
		rm -f tmux *.o .depend *~ *.core *.log compat/*.o

install:
		$(INSTALLDIR) $(DESTDIR)$(PREFIX)/bin
		$(INSTALLBIN) tmux $(DESTDIR)$(PREFIX)/bin/tmux
		$(INSTALLDIR) $(DESTDIR)$(PREFIX)/man/man1
		$(INSTALLMAN) tmux.1 $(DESTDIR)$(PREFIX)/man/man1/tmux.1
