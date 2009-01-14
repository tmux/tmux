# $Id: GNUmakefile,v 1.55 2009-01-14 23:08:23 nicm Exp $

.PHONY: clean

PROG= tmux
VERSION= 0.6

DATE= $(shell date +%Y%m%d-%H%M)

DEBUG= 1

META?= \002

SRCS= tmux.c server.c server-msg.c server-fn.c buffer.c buffer-poll.c status.c \
      xmalloc.c xmalloc-debug.c input.c input-keys.c \
      screen.c screen-write.c screen-redraw.c \
      grid.c grid-view.c \
      window.c session.c log.c client.c client-msg.c client-fn.c cfg.c \
      key-string.c key-bindings.c resize.c arg.c mode-key.c \
      cmd.c cmd-generic.c cmd-string.c \
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
      cmd-respawn-window.c cmd-source-file.c cmd-server-info.c \
      cmd-clock-mode.c cmd-lock-server.c cmd-set-password.c \
      cmd-save-buffer.c cmd-select-pane.c cmd-split-window.c \
      cmd-resize-pane-up.c cmd-resize-pane-down.c cmd-kill-pane.c \
      cmd-up-pane.c cmd-down-pane.c \
      window-clock.c window-scroll.c window-more.c window-copy.c \
      options.c options-cmd.c paste.c colour.c utf8.c clock.c \
      tty.c tty-term.c tty-keys.c tty-write.c

CC?= gcc
INCDIRS+= -I. -I-
CFLAGS+= -DBUILD="\"$(VERSION) ($(DATE))\"" -DMETA="'${META}'"
ifdef DEBUG
CFLAGS+= -g -ggdb -DDEBUG
LDFLAGS+= -rdynamic
LIBS+= -ldl
endif
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align

LDFLAGS+=
LIBS+= -lncurses

PREFIX?= /usr/local
MANDIR?= $(PREFIX)/man/man1
INSTALLDIR= install -d
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

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
INCDIRS+= -Icompat -I/usr/local/include/ncurses
SRCS+= compat/strtonum.c compat/daemon.c compat/forkpty-sunos.c \
	compat/asprintf.c compat/fgetln.c compat/vis.c
CFLAGS+= -DNO_STRTONUM -DNO_TREE_H -DNO_PATHS_H -DNO_SETPROCTITLE \
	-DNO_DAEMON -DNO_FORKPTY -DNO_PROGNAME -DNO_ASPRINTF -DNO_FGETLN
LDFLAGS+= -L/usr/local/lib
LIBS+= -lsocket -lnsl
endif

ifeq ($(shell uname),Darwin)
INCDIRS+= -Icompat
SRCS+= compat/strtonum.c compat/bsd-poll.c compat/vis.c
CFLAGS+= -DNO_STRTONUM -DNO_SETRESUID -DNO_SETRESGID -DNO_SETPROCTITLE \
         -DNO_TREE_H -DBROKEN_POLL
endif

ifeq ($(shell uname),Linux)
INCDIRS+= -Icompat
SRCS+= compat/strlcpy.c compat/strlcat.c compat/strtonum.c \
       compat/fgetln.c compat/getopt_long.c compat/vis.c
CFLAGS+= $(shell getconf LFS_CFLAGS) -D_GNU_SOURCE \
         -DNO_STRLCPY -DNO_STRLCAT -DNO_STRTONUM -DNO_SETPROCTITLE \
         -DNO_QUEUE_H -DNO_TREE_H -DUSE_PTY_H -DNO_FGETLN \
	 -DBROKEN_GETOPT -std=c99
LIBS+= -lcrypt -lutil
endif

OBJS= $(patsubst %.c,%.o,$(SRCS))

CLEANFILES= ${PROG} *.o .depend *~ ${PROG}.core *.log index.html

CPPFLAGS+= $(INCDIRS)

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $+ $(LIBS)

depend: $(SRCS)
	$(CC) $(CFLAGS) $(INCDIRS) -MM $(SRCS) > .depend

install:
	$(INSTALLDIR) $(DESTDIR)$(PREFIX)/bin
	$(INSTALLBIN) $(PROG) $(DESTDIR)$(PREFIX)/bin/$(PROG)
	$(INSTALLDIR) $(DESTDIR)$(MANDIR)
	$(INSTALLMAN) $(PROG).1 $(DESTDIR)$(MANDIR)

clean:
	rm -f $(CLEANFILES)
