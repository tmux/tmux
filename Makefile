# $Id: Makefile,v 1.1.1.1 2007-07-09 19:03:33 nicm Exp $

.SUFFIXES: .c .o .y .l .h
.PHONY: clean

PROG= tmux
VERSION= 0.1

OS!= uname
REL!= uname -r
DATE!= date +%Y%m%d-%H%M

SRCS= tmux.c server.c buffer.c buffer-poll.c xmalloc.c input.c screen.c \
      window.c session.c local.c log.c command.c
HDRS= tmux.h

LEX= lex
YACC= yacc -d

CC= cc
INCDIRS+= -I. -I- -I/usr/local/include
CFLAGS+= -DBUILD="\"$(VERSION) ($(DATE))\""
CFLAGS+= -g -ggdb -DDEBUG
#CFLAGS+= -pedantic -std=c99
#CFLAGS+= -Wredundant-decls  -Wdisabled-optimization -Wendif-labels
CFLAGS+= -Wno-long-long -Wall -W -Wnested-externs -Wformat=2
CFLAGS+= -Wmissing-prototypes -Wstrict-prototypes -Wmissing-declarations
CFLAGS+= -Wwrite-strings -Wshadow -Wpointer-arith -Wcast-qual -Wsign-compare
CFLAGS+= -Wundef -Wshadow -Wbad-function-cast -Winline -Wcast-align

PREFIX?= /usr/local
INSTALLBIN= install -g bin -o root -m 555
INSTALLMAN= install -g bin -o root -m 444

LDFLAGS+= -L/usr/local/lib
LIBS+= -lutil -lncurses

OBJS= ${SRCS:S/.c/.o/:S/.y/.o/:S/.l/.o/}

CLEANFILES= ${PROG} *.o .depend *~ ${PROG}.core *.log

.c.o:
		${CC} ${CFLAGS} ${INCDIRS} -c ${.IMPSRC} -o ${.TARGET}

.l.o:
		${LEX} ${.IMPSRC}
		${CC} ${CFLAGS} ${INCDIRS} -c lex.yy.c -o ${.TARGET}

.y.o:
		${YACC} ${.IMPSRC}
		${CC} ${CFLAGS} ${INCDIRS} -c y.tab.c -o ${.TARGET}

all:		.depend ${PROG}

${PROG}:	${OBJS}
		${CC} ${LDFLAGS} -o ${PROG} ${LIBS} ${OBJS}

.depend:	${HDRS}
		-mkdep ${CFLAGS} ${INCDIRS} ${SRCS:M*.c}

depend:
		mkdep ${CFLAGS} ${INCDIRS} ${SRCS:M*.c}

clean:
		rm -f ${CLEANFILES}
