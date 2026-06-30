# Standalone build for the cmd-parse.y AST parser.
#
# Compiles only ../cmd-parse.y + ../tmux-parser.h against the local stub tmux.h
# and tiny helper stubs. The rest of tmux is not needed.

YACC ?= bison -y
CC ?= cc

CFLAGS += -D_GNU_SOURCE -I. -I.. -g -Wall

OBJS = y.tab.o main.o xstubs.o

parsetest: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

y.tab.c: ../cmd-parse.y
	$(YACC) -d ../cmd-parse.y

y.tab.o: y.tab.c
	$(CC) $(CFLAGS) -c -o $@ y.tab.c

main.o: main.c tmux.h
	$(CC) $(CFLAGS) -c -o $@ main.c

xstubs.o: xstubs.c tmux.h
	$(CC) $(CFLAGS) -c -o $@ xstubs.c

clean:
	rm -f parsetest y.tab.c y.tab.h $(OBJS)

.PHONY: clean
