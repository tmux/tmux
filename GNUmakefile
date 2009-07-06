# $Id: GNUmakefile,v 1.108 2009-07-06 18:21:17 nicm Exp $

.PHONY: clean

VERSION= 1.0

FDEBUG= 1

CC?= gcc
CFLAGS+= -DBUILD="\"$(VERSION)\""
LDFLAGS+= -L/usr/local/lib
LIBS+= -lncurses

# This sort of sucks but gets rid of the stupid warning and should work on
# most platforms...
ifeq ($(shell (LC_ALL=C $(CC) -v 2>&1|awk '/gcc version 4/') || true), )
CPPFLAGS:= -I. -I- $(CPPFLAGS)
else
CPPFLAGS:= -iquote. $(CPPFLAGS)
endif

ifdef FDEBUG
CFLAGS+= -g -ggdb -DDEBUG
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align
endif

PREFIX?= /usr/local
INSTALLDIR= install -d
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

SRCS= $(shell echo *.c|sed 's|osdep-[a-z0-9]*.c||g')
include config.mk
OBJS= $(patsubst %.c,%.o,$(SRCS))

all:		tmux

tmux:		$(OBJS)
		$(CC) $(LDFLAGS) -o tmux $+ $(LIBS)

depend: 	$(SRCS)
		$(CC) $(CPPFLAGS) $(CFLAGS) -MM $(SRCS) > .depend

clean:
		rm -f tmux *.o .depend *~ *.core *.log compat/*.o

clean-all:	clean
		rm -f config.h config.mk

install:	all
		$(INSTALLDIR) $(DESTDIR)$(PREFIX)/bin
		$(INSTALLBIN) tmux $(DESTDIR)$(PREFIX)/bin/tmux
		$(INSTALLDIR) $(DESTDIR)$(PREFIX)/man/man1
		$(INSTALLMAN) tmux.1 $(DESTDIR)$(PREFIX)/man/man1/tmux.1
