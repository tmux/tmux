# $Id: Makefile,v 1.28 2007-10-19 11:10:34 nicm Exp $

.SUFFIXES: .c .o .y .h
.PHONY: clean

PROG= tmux
VERSION= 0.1

OS!= uname
REL!= uname -r
DATE!= date +%Y%m%d-%H%M

# This must be empty as OpenBSD includes it in default CFLAGS.
DEBUG=

META?= \002 # C-b

SRCS= tmux.c server.c server-msg.c server-fn.c buffer.c buffer-poll.c status.c \
      xmalloc.c xmalloc-debug.c input.c input-keys.c screen.c window.c \
      session.c local.c log.c client.c client-msg.c client-fn.c key-string.c \
      key-bindings.c resize.c cmd.c cmd-new-session.c cmd-detach-session.c \
      cmd-list-sessions.c cmd-new-window.c cmd-next-window.c cmd-bind-key.c \
      cmd-unbind-key.c cmd-previous-window.c cmd-last-window.c cmd-list-keys.c \
      cmd-set-option.c cmd-rename-window.c cmd-select-window.c \
      cmd-list-windows.c cmd-attach-session.c cmd-send-prefix.c \
      cmd-refresh-session.c cmd-kill-window.c

YACC= yacc -d

CC?= cc
INCDIRS+= -I. -I- -I/usr/local/include
CFLAGS+= -DBUILD="\"$(VERSION) ($(DATE))\"" -DMETA="'${META}'"
.ifdef PROFILE
# Don't use ccache
CC= /usr/bin/gcc
CFLAGS+= -pg -DPROFILE -O0
.endif
.ifdef DEBUG
CFLAGS+= -g -ggdb -DDEBUG
LDFLAGS+= -Wl,-E
.endif
#CFLAGS+= -pedantic -std=c99
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wbad-function-cast -Winline -Wcast-align

PREFIX?= /usr/local
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

LDFLAGS+= -L/usr/local/lib
.ifdef PROFILE
LDFLAGS+= -pg
.endif
LIBS+= -lutil -lncurses

OBJS= ${SRCS:S/.c/.o/:S/.y/.o/}

CLEANFILES= ${PROG} *.o .depend *~ ${PROG}.core *.log

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

clean:
		rm -f ${CLEANFILES}

install:	all
		${INSTALLBIN} ${PROG} ${DESTDIR}${PREFIX}/bin/${PROG}
