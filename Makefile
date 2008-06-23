# $Id: Makefile,v 1.66 2008-06-23 22:24:16 nicm Exp $

.SUFFIXES: .c .o .y .h
.PHONY: clean update-index.html upload-index.html

PROG= tmux
VERSION= 0.4

OS!= uname
REL!= uname -r
DATE!= date +%Y%m%d-%H%M

# This must be empty as OpenBSD includes it in default CFLAGS.
DEBUG=

META?= \002 # C-b

SRCS= tmux.c server.c server-msg.c server-fn.c buffer.c buffer-poll.c status.c \
      xmalloc.c xmalloc-debug.c input.c input-keys.c screen.c screen-display.c \
      window.c session.c log.c client.c client-msg.c client-fn.c cfg.c \
      key-string.c key-bindings.c resize.c arg.c \
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
      cmd-list-commands.c \
      window-scroll.c window-more.c window-copy.c options.c paste.c \
      tty.c tty-keys.c tty-write.c screen-write.c screen-redraw.c

CC?= cc
INCDIRS+= -I. -I- -I/usr/local/include
CFLAGS+= -DMETA="'${META}'"
.ifdef PROFILE
# Don't use ccache
CC= /usr/bin/gcc
CFLAGS+= -pg -DPROFILE -O0
.endif
.ifdef DEBUG
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

# FreeBSD
.if ${OS} == "FreeBSD"
CFLAGS+= -DUSE_LIBUTIL_H
.endif

OBJS= ${SRCS:S/.c/.o/:S/.y/.o/}

DISTDIR= ${PROG}-${VERSION}
DISTFILES= *.[chyl] Makefile GNUmakefile *.[1-9] NOTES TODO \
	   `find examples compat -type f -and ! -path '*CVS*'`

CLEANFILES= ${PROG} *.o .depend *~ ${PROG}.core *.log index.html

.c.o:
		${CC} ${CFLAGS} ${INCDIRS} -c ${.IMPSRC} -o ${.TARGET}

.y.o:
		${YACC} ${.IMPSRC}
		${CC} ${CFLAGS} ${INCDIRS} -c y.tab.c -o ${.TARGET}

all:		${PROG}

${PROG}:	${OBJS}
		${CC} ${LDFLAGS} -o ${PROG} ${LIBS} ${OBJS}

depend:
		mkdep ${CFLAGS} ${INCDIRS} ${SRCS:M*.c}

dist:		clean
		grep '^#DEBUG=' Makefile
		grep '^#DEBUG=' GNUmakefile
		[ "`(grep '^VERSION' Makefile; grep '^VERSION' GNUmakefile)| \
			uniq -u`" = "" ]
		tar -zc \
			-s '/.*/${DISTDIR}\/\0/' \
			-f ${DISTDIR}.tar.gz ${DISTFILES}

lint:
		lint -cehvx ${CFLAGS:M-D*} ${SRCS:M*.c}

clean:
		rm -f ${CLEANFILES}

upload-index.html:
		scp index.html nicm@shell.sf.net:index.html
		ssh nicm@shell.sf.net sh update-index-tmux.sh

update-index.html:
		sed "s/%%VERSION%%/${VERSION}/g" index.html.in >index.html

install:	all
		${INSTALLDIR} ${DESTDIR}${PREFIX}/bin
		${INSTALLBIN} ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}
		${INSTALLDIR} ${DESTDIR}${PREFIX}/man/man1
		${INSTALLMAN} ${PROG}.1 ${DESTDIR}${PREFIX}/man/man1/
