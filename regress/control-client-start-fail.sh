#!/bin/sh

# The server cannot create its socket, so it sets the control client to exit
# before the client completes identification. The exit check must not use the
# control state before MSG_IDENTIFY_DONE creates it. The client must get the
# reason for the failure in %exit.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d)
SOCK=$DIR/missing/sock

OUT=$($TEST_TMUX -f/dev/null -S$SOCK -C new-session </dev/null 2>&1)
rm -rf $DIR
[ "$OUT" = "%exit error creating $SOCK (No such file or directory)" ] || exit 1

exit 0
