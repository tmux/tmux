#!/bin/sh

. ./regress-common.inc

# a client lost while the configuration is loading is freed

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMPDIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$TMPDIR/tmux.sock"

cleanup()
{
	touch $TMPDIR/go
	stop_server $TMUX || exit 1
	rm -rf "$TMPDIR"
}
trap cleanup 0 1 15

# The run-shell holds the configuration until the go file exists.
cat <<EOF >$TMPDIR/conf
new-session -d -s keep
run-shell 'touch $TMPDIR/started; i=0; while [ ! -f $TMPDIR/go ] && [ \$i -lt 100 ]; do sleep 0.1; i=\$((i + 1)); done'
EOF

cd $TMPDIR || exit 1
$TMUX -v -f$TMPDIR/conf new-session -d -s first </dev/null &
pid=$!
wait_until test -f $TMPDIR/started || exit 1
kill $pid
wait $pid
log=$(echo $TMPDIR/tmux-server-*.log)
wait_until grep -q 'lost client' $log || exit 1
c=$(sed -n 's/.* lost client //p' $log)

# The client is freed straight away, well before the configuration ends.
wait_until grep -q "free client $c " $log || exit 1

exit 0
