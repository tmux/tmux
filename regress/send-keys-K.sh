#!/bin/sh

# A key sent with send-keys -K and not bound must reach the pane as the same
# bytes as a typed key.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

. ./regress-common.inc

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

cleanup()
{
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

OUT=$DIR/out
$INNER new-session -d -s inner -x40 -y10 \
	"stty raw -echo; exec cat >$OUT" || exit 1
$INNER set-option -g status off || exit 1
$OUTER new-session -d -s outer -x40 -y10 "$INNER attach -t inner" || exit 1
wait_for_clients 1 $INNER || exit 1
client=$($INNER list-clients -F '#{client_name}')

for key in C-F3 BTab F3 C-Up a Enter; do
	$INNER send-keys -K -c "$client" $key || exit 1
done
printf '\033[1;5R\033[Z\033OR\033[1;5Aa\r' >$DIR/expected
wait_until cmp -s $OUT $DIR/expected || {
	od -c $OUT >&2
	exit 1
}

exit 0
