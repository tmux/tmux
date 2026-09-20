#!/bin/sh

# the configuration is loaded once if the client that started it is lost, and
# the next client still waits for it to finish

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMPDIR=$(mktemp -d) || exit 1
TMUX="$TEST_TMUX -S$TMPDIR/tmux.sock"

cleanup()
{
	touch $TMPDIR/go
	$TMUX kill-server 2>/dev/null
	rm -rf "$TMPDIR"
}
trap cleanup 0 1 15

# Poll until a command succeeds.
wait_for()
{
	i=0
	while ! "$@"; do
		[ $i -eq 100 ] && exit 1
		i=$((i + 1))
		sleep 0.1
	done
}

# The run-shell holds the configuration until the go file exists.
cat <<EOF >$TMPDIR/conf
set -ga @runs x
new-session -d -s keep
run-shell 'touch $TMPDIR/started; i=0; while [ ! -f $TMPDIR/go ] && [ \$i -lt 100 ]; do sleep 0.1; i=\$((i + 1)); done'
set -g @late 1
EOF

cd $TMPDIR || exit 1
$TMUX -v -f$TMPDIR/conf new-session -d -s first </dev/null &
pid=$!
wait_for test -f $TMPDIR/started
kill $pid
wait $pid
log=$(echo $TMPDIR/tmux-server-*.log)
wait_for grep -q 'lost client' $log

# The second client must identify before the configuration is released.
$TMUX new-session -d -s second \; display -p '#{@runs} #{@late}' \
	</dev/null >$TMPDIR/out &
pid=$!
wait_for grep -q "name is client-$pid\$" $log
touch $TMPDIR/go
wait $pid || exit 1

[ "$(cat $TMPDIR/out)" = "x 1" ] || exit 1

exit 0
