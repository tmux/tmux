# $Id: GNUmakefile,v 1.8 2007-11-25 22:08:13 nicm Exp $

.PHONY: clean

PROG= tmux
VERSION= 0.2

DATE= $(shell date +%Y%m%d-%H%M)

DEBUG= 1

META?= \002

SRCS= tmux.c server.c server-msg.c server-fn.c buffer.c buffer-poll.c status.c \
      xmalloc.c xmalloc-debug.c input.c input-keys.c screen.c screen-display.c \
      window.c session.c local.c log.c client.c client-msg.c client-fn.c \
      key-string.c key-bindings.c resize.c cmd.c cmd-new-session.c \
      cmd-detach-client.c cmd-list-sessions.c cmd-new-window.c cmd-bind-key.c \
      cmd-unbind-key.c cmd-previous-window.c cmd-last-window.c cmd-list-keys.c \
      cmd-set-option.c cmd-rename-window.c cmd-select-window.c \
      cmd-list-windows.c cmd-attach-session.c cmd-send-prefix.c \
      cmd-refresh-client.c cmd-kill-window.c cmd-list-clients.c \
      cmd-link-window.c cmd-unlink-window.c cmd-next-window.c \
      cmd-swap-window.c cmd-rename-session.c cmd-kill-session.c \
      cmd-switch-client.c cmd-has-session.c cmd-scroll-mode.c cmd-copy-mode.c \
      cmd-paste-buffer.c window-scroll.c window-more.c window-copy.c

CC?= gcc
INCDIRS+= -I. -I-
CFLAGS+= -DBUILD="\"$(VERSION) ($(DATE))\"" -DMETA="'${META}'"
ifdef DEBUG
CFLAGS+= -g -ggdb -DDEBUG
LDFLAGS+= -rdynamic
endif
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align

LDFLAGS+= 
LIBS+= -lncurses

PREFIX?= /usr/local
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

ifeq ($(shell uname),Darwin)
INCDIRS+= -Icompat
SRCS+= compat/strtonum.c
CFLAGS+= -DNO_STRTONUM -DNO_SETRESUID -DNO_SETRESGID -DNO_SETPROCTITLE \
         -DNO_TREE_H
endif

ifeq ($(shell uname),Linux)
INCDIRS+= -Icompat
SRCS+= compat/strlcpy.c compat/strlcat.c compat/strtonum.c
CFLAGS+= $(shell getconf LFS_CFLAGS) -D_GNU_SOURCE \
         -DNO_STRLCPY -DNO_STRLCAT -DNO_STRTONUM -DNO_SETPROCTITLE \
         -DNO_QUEUE_H -DNO_TREE_H -DUSE_PTY_H
LDFLAGS+= -lresolv -lutil
# Required for LLONG_MAX and friends
CFLAGS+= -std=c99
endif

OBJS= $(patsubst %.c,%.o,$(SRCS))

CLEANFILES= $(PROG) y.tab.c y.tab.h $(OBJS) .depend

CPPFLAGS+= $(INCDIRS)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) $(LIBS) -o $@ $+

depend: $(SRCS)
	$(CC) $(CFLAGS) $(INCDIRS) -MM $(SRCS) > .depend

install:
	$(INSTALLBIN) $(PROG) $(DESTDIR)$(PREFIX)/bin/$(PROG)
	$(INSTALLMAN) $(PROG).1 $(DESTDIR)$(PREFIX)/man/man1/

clean:
	rm -f $(CLEANFILES)
