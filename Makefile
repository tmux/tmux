# $Id: Makefile,v 1.126 2009-04-29 22:25:20 nicm Exp $

.SUFFIXES: .c .o .y .h
.PHONY: clean update-index.html upload-index.html

PROG= tmux
VERSION= 0.9

OS!= uname
REL!= uname -r
DATE!= date +%Y%m%d-%H%M

FDEBUG= 1

META?= \002 # C-b

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
      cmd-respawn-window.c cmd-source-file.c cmd-server-info.c \
      cmd-clock-mode.c cmd-lock-server.c cmd-set-password.c \
      cmd-save-buffer.c cmd-select-pane.c cmd-split-window.c \
      cmd-resize-pane-up.c cmd-resize-pane-down.c cmd-kill-pane.c \
      cmd-up-pane.c cmd-down-pane.c cmd-choose-window.c cmd-choose-session.c \
      cmd-suspend-client.c cmd-find-window.c cmd-load-buffer.c \
      cmd-copy-buffer.c cmd-break-pane.c cmd-swap-pane.c cmd-next-layout.c \
      cmd-rotate-window.c cmd-confirm-before.c \
      window-clock.c window-scroll.c window-more.c window-copy.c \
      window-choose.c \
      options.c options-cmd.c paste.c colour.c utf8.c clock.c \
      tty.c tty-term.c tty-keys.c tty-write.c util.c names.c attributes.c \
      osdep-unknown.c osdep-openbsd.c osdep-freebsd.c osdep-linux.c \
      osdep-darwin.c osdep-netbsd.c

CC?= c
INCDIRS+= -I. -I- -I/usr/local/include
CFLAGS+= -DMETA="'${META}'"
.ifdef PROFILE
# Don't use ccache
CC= /usr/bin/gcc
CFLAGS+= -pg -DPROFILE -O0
.endif
.ifdef FDEBUG
CFLAGS+= -g -ggdb -DDEBUG
LDFLAGS+= -Wl,-E
CFLAGS+= -DBUILD="\"$(VERSION) ($(DATE))\""
.else
CFLAGS+= -DBUILD="\"$(VERSION)\""
.endif
#CFLAGS+= -pedantic -std=c99
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align

PREFIX?= /usr/local
INSTALLDIR= install -d
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

LDFLAGS+= -L/usr/local/lib
.ifdef PROFILE
LDFLAGS+= -pg
.endif
LIBS+= -lutil -lncurses

# FreeBSD and DragonFly
.if ${OS} == "FreeBSD" || ${OS} == "DragonFly"
INCDIRS+= -Icompat
CFLAGS+= -DUSE_LIBUTIL_H -DNO_QUEUE_H -DNO_TREE_H
LIBS+= -lcrypt
.endif

# NetBSD
.if ${OS} == "NetBSD"
INCDIRS+= -Icompat
SRCS+= compat/strtonum.c
LIBS+= -lcrypt
CFLAGS+=-DNO_STRTONUM -DNO_QUEUE_H
.endif

OBJS= ${SRCS:S/.c/.o/:S/.y/.o/}

DISTDIR= ${PROG}-${VERSION}
DISTFILES= *.[chyl] Makefile GNUmakefile *.[1-9] NOTES TODO CHANGES FAQ \
	   `find examples compat -type f -and ! -path '*CVS*'`

CLEANFILES= ${PROG} *.o .depend *~ ${PROG}.core *.log compat/*.o index.html

CPPFLAGS:= ${INCDIRS} ${CPPFLAGS} 

.c.o:
		${CC} ${CPPFLAGS} ${CFLAGS} -c ${.IMPSRC} -o ${.TARGET}

.y.o:
		${YACC} ${.IMPSRC}
		${CC} ${CPPFLAGS} ${CFLAGS}  -c y.tab.c -o ${.TARGET}

all:		${PROG}

${PROG}:	${OBJS}
		${CC} ${LDFLAGS} -o ${PROG} ${OBJS} ${LIBS}

depend:
		mkdep ${CPPFLAGS} ${CFLAGS} ${SRCS:M*.c}

dist:		clean
		grep '^#FDEBUG=' Makefile
		grep '^#FDEBUG=' GNUmakefile
		[ "`(grep '^VERSION' Makefile; grep '^VERSION' GNUmakefile)| \
			uniq -u`" = "" ]
		tar -zc \
			-s '/.*/${DISTDIR}\/\0/' \
			-f ${DISTDIR}.tar.gz ${DISTFILES}

lint:
		lint -chvx ${CFLAGS:M-D*} ${SRCS:M*.c}

clean:
		rm -f ${CLEANFILES}

upload-index.html: update-index.html
		scp index.html images/*.png \
			nicm,tmux@web.sf.net:/home/groups/t/tm/tmux/htdocs
		rm -f images/small-*

update-index.html:
		(cd images && \
			rm -f small-* && \
			for i in *.png; do \
			convert "$$i" -resize 200x150 "small-$$i"; \
			done \
		)
		sed "s/%%VERSION%%/${VERSION}/g" index.html.in >index.html

install:	all
		${INSTALLDIR} ${DESTDIR}${PREFIX}/bin
		${INSTALLBIN} ${PROG} ${DESTDIR}${PREFIX}/bin/
		${INSTALLDIR} ${DESTDIR}${PREFIX}/man/man1
		${INSTALLMAN} ${PROG}.1 ${DESTDIR}${PREFIX}/man/man1/
