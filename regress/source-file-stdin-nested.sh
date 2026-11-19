#!/bin/sh

. ./regress-common.inc

# a nested source-file - fails because stdin can only be read once, rather
# than hanging the client

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMPDIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$TMPDIR/tmux.sock"

cleanup()
{
	[ -n "$pid" ] && kill -9 $pid 2>/dev/null
	stop_server $TMUX || exit 1
	rm -rf "$TMPDIR"
}
trap cleanup 0 1 15

$TMUX -f/dev/null new-session -d || exit 1

printf 'display -p x\nsource -\n' >$TMPDIR/in
$TMUX source - <$TMPDIR/in >$TMPDIR/out 2>&1 &
pid=$!
wait_for_exit $pid || exit 1
pid=

[ "$(sed -n 1p $TMPDIR/out)" = "x" ] || exit 1
grep -q 'Bad file descriptor: -' $TMPDIR/out || exit 1
[ $(wc -l <$TMPDIR/out) -eq 2 ] || exit 1

exit 0
