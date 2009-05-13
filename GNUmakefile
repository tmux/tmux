# $Id: GNUmakefile,v 1.93 2009-05-13 22:20:47 nicm Exp $

.PHONY: clean

PROG= tmux
VERSION= 0.9

DATE= $(shell date +%Y%m%d-%H%M)

FDEBUG= 1

SRCS= tmux.c server.c server-msg.c server-fn.c buffer.c buffer-poll.c status.c \
      xmalloc.c xmalloc-debug.c input.c input-keys.c \
      screen.c screen-write.c screen-redraw.c \
      grid.c grid-view.c \
      window.c session.c log.c client.c client-msg.c client-fn.c cfg.c \
      layout.c key-string.c key-bindings.c resize.c arg.c mode-key.c \
      cmd.c cmd-generic.c cmd-string.c cmd-list.c \
      cmd-detach-client.c cmd-list-sessions.c cmd-new-window.c cmd-bind-key.c \
      cmd-unbind-key.c cmd-previous-window.c cmd-last-window.c cmd-list-keys.c \
      cmd-set-option.c cmd-rename-window.c cmd-select-window.c \
      cmd-list-windows.c cmd-attach-session.c cmd-send-prefix.c \
      cmd-refresh-client.c cmd-kill-window.c cmd-list-clients.c \
      cmd-link-window.c cmd-unlink-window.c cmd-next-window.c cmd-send-keys.c \
      cmd-swap-window.c cmd-rename-session.c cmd-kill-session.c \
      cmd-switch-client.c cmd-has-session.c cmd-scroll-mode.c cmd-copy-mode.c \
      cmd-paste-buffer.c cmd-new-session.c cmd-start-server.c \
      cmd-kill-server.c cmd-set-window-option.c cmd-show-options.c \
      cmd-show-window-options.c cmd-command-prompt.c cmd-set-buffer.c \
      cmd-show-buffer.c cmd-list-buffers.c cmd-delete-buffer.c \
      cmd-list-commands.c cmd-move-window.c cmd-select-prompt.c \
      cmd-respawn-window.c cmd-source-file.c cmd-server-info.c cmd-down-pane.c \
      cmd-clock-mode.c cmd-lock-server.c cmd-set-password.c cmd-up-pane.c \
      cmd-save-buffer.c cmd-select-pane.c cmd-split-window.c cmd-kill-pane.c \
      cmd-resize-pane.c cmd-choose-window.c cmd-choose-session.c \
      cmd-suspend-client.c cmd-find-window.c cmd-load-buffer.c \
      cmd-copy-buffer.c cmd-break-pane.c cmd-swap-pane.c cmd-rotate-window.c \
      cmd-confirm-before.c cmd-next-layout.c cmd-previous-layout.c \
      window-clock.c window-scroll.c window-more.c window-copy.c \
      window-choose.c \
      options.c options-cmd.c paste.c colour.c utf8.c clock.c \
      tty.c tty-term.c tty-keys.c tty-write.c util.c names.c attributes.c

CC?= gcc
INCDIRS+= -I. -I-
CFLAGS+= -DBUILD="\"$(VERSION) ($(DATE))\""
ifdef FDEBUG
CFLAGS+= -g -ggdb -DDEBUG
LDFLAGS+= -rdynamic
LIBS+= -ldl
endif
ifeq (${CC},gcc)
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align
endif

LDFLAGS+=
LIBS+= -lncurses

PREFIX?= /usr/local
INSTALLDIR= install -d
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

ifeq ($(shell uname),AIX)
INCDIRS+= -I/usr/local/include/ncurses -Icompat
SRCS+= compat/strlcpy.c compat/strlcat.c compat/strtonum.c \
       compat/fgetln.c compat/asprintf.c compat/daemon.c compat/forkpty-aix.c \
       compat/getopt.c compat/bsd-poll.c
CFLAGS+= -DNO_TREE_H -DNO_ASPRINTF -DNO_QUEUE_H -DNO_VSYSLOG \
	 -DNO_PROGNAME -DNO_STRLCPY -DNO_STRLCAT -DNO_STRTONUM \
	 -DNO_SETPROCTITLE -DNO_QUEUE_H -DNO_TREE_H -DNO_FORKPTY -DNO_FGETLN \
	 -DBROKEN_GETOPT -DBROKEN_POLL -DNO_PATHS_H
LDFLAGS+= -L/usr/local/lib
endif

ifeq ($(shell uname),IRIX64)
INCDIRS+= -Icompat -I/usr/local/include/ncurses
SRCS+= compat/strlcpy.c compat/strtonum.c compat/daemon.c \
	compat/asprintf.c compat/fgetln.c compat/forkpty-irix.c
CFLAGS+= -DNO_STRLCPY -DNO_STRTONUM -DNO_TREE_H -DNO_SETPROCTITLE \
	-DNO_DAEMON -DNO_FORKPTY -DNO_PROGNAME -DNO_ASPRINTF -DNO_FGETLN \
	-DBROKEN_VSNPRINTF -D_SGI_SOURCE -std=c99
LDFLAGS+= -L/usr/local/lib
LIBS+= -lgen
endif

ifeq ($(shell uname),SunOS)
INCDIRS+= -Icompat -I/usr/include/ncurses
SRCS+= compat/strtonum.c compat/daemon.c compat/forkpty-sunos.c \
	compat/asprintf.c compat/fgetln.c compat/getopt.c
CFLAGS+= -DNO_STRTONUM -DNO_TREE_H -DNO_PATHS_H -DNO_SETPROCTITLE \
	-DNO_DAEMON -DNO_FORKPTY -DNO_PROGNAME -DNO_ASPRINTF -DNO_FGETLN \
	-DBROKEN_GETOPT -DNO_QUEUE_H
LDFLAGS+= -L/usr/gnu/lib
LIBS+= -lsocket -lnsl
endif

ifeq ($(shell uname),Darwin)
INCDIRS+= -Icompat
SRCS+= compat/strtonum.c compat/bsd-poll.c
CFLAGS+= -DNO_STRTONUM -DNO_SETPROCTITLE -DNO_QUEUE_H -DNO_TREE_H -DBROKEN_POLL
endif

ifeq ($(shell uname),Linux)
INCDIRS+= -Icompat
SRCS+= compat/strlcpy.c compat/strlcat.c compat/strtonum.c \
       compat/fgetln.c compat/getopt.c
CFLAGS+= $(shell getconf LFS_CFLAGS) -D_GNU_SOURCE \
         -DNO_STRLCPY -DNO_STRLCAT -DNO_STRTONUM -DNO_SETPROCTITLE \
         -DNO_QUEUE_H -DNO_TREE_H -DUSE_PTY_H -DNO_FGETLN \
	 -DBROKEN_GETOPT -std=c99
LIBS+= -lcrypt -lutil
endif

LCOS= $(shell uname|tr '[:upper:]' '[:lower:]')
OSDEP= $(shell [ -f osdep-$(LCOS).c ] && echo $(LCOS) || echo unknown)
SRCS+= osdep-$(OSDEP).c

OBJS= $(patsubst %.c,%.o,$(SRCS))

CLEANFILES= ${PROG} *.o .depend *~ ${PROG}.core *.log compat/*.o index.html

CPPFLAGS:= ${INCDIRS} ${CPPFLAGS}

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

depend: $(SRCS)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MM $(SRCS) > .depend

install:
	$(INSTALLDIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALLBIN) $(PROG) $(DESTDIR)$(PREFIX)/bin/$(PROG)
	$(INSTALLDIR) $(DESTDIR)$(PREFIX)/man/man1
	$(INSTALLMAN) $(PROG).1 $(DESTDIR)$(PREFIX)/man/man1/$(PROG).1

clean:
	rm -f $(CLEANFILES)
