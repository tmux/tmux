#!/bin/sh

# A control client whose server fails to create its socket is marked for exit
# as soon as the server creates it, before it has identified. The server loop
# can then see CLIENT_CONTROL, from the identify flags, before
# MSG_IDENTIFY_DONE has set up the control state, and the exit check must not
# touch that state. The client should be told why the server could not start.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d)
SOCK=$DIR/missing/sock

OUT=$($TEST_TMUX -f/dev/null -S$SOCK -C new-session </dev/null 2>&1)
rm -rf $DIR
[ "$OUT" = "%exit error creating $SOCK (No such file or directory)" ] || exit 1

exit 0
