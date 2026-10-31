#!/bin/sh

. ./regress-common.inc

# Print the selected pane to the invoking client, with no default command.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	stop_server $TMUX2 || exit 1
	stop_server $TMUX || exit 1
	rm -rf "$DIR"
}
trap cleanup EXIT

fail()
{
	echo "$*" >&2
	[ ! -f "$DIR/error" ] || cat "$DIR/error" >&2
	exit 1
}

wait_format()
{
	wait_for_output "$3" $TMUX display-message -p -t "$1" "$2" ||
	    fail "expected '$3' for $2"
}

start_print()
{
	target=$1
	shift
	$TMUX set -g @after-print no || exit 1
	$TMUX set -g @picked none || exit 1
	$TMUX display-panes -Pd 0 -t "$target" "$@" \; \
	    set -gF @after-picked '#{@picked}' \; set -g @after-print yes \
	    >"$DIR/print" 2>"$DIR/error" &
	print_pid=$!
	wait_format "$target" '#{pane_mode}' panes-mode
	[ "$($TMUX show -gqv @after-print)" = no ] ||
	    fail "-P returned before the mode exited"
}

finish_print()
{
	wait_format "$p0" '#{@after-print}' yes
	wait_for_exit "$print_pid" || fail "display-panes -P did not exit"
	wait "$print_pid" || fail "display-panes -P failed"
	: >"$DIR/expected"
	[ -z "$1" ] || printf '%s\n' "$1" >"$DIR/expected"
	cmp -s "$DIR/print" "$DIR/expected" || fail "wrong printed pane"
}

$TMUX new-session -d -s m -x 80 -y 24 'cat' || exit 1
p0=$($TMUX display-message -p -t m:0 '#{pane_id}')
p1=$($TMUX split-window -dP -F '#{pane_id}' -t "$p0" 'cat') || exit 1
$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t m" || exit 1
wait_format "$p0" '#{session_attached}' 1

# The default and an explicitly empty template both leave the active pane alone.
start_print "$p0"
$TMUX send-keys -t "$p0" 1 || exit 1
finish_print "$p1"
wait_format "$p0" '#{pane_active}' 1

start_print "$p0" ''
$TMUX send-keys -t "$p0" 1 || exit 1
finish_print "$p1"
wait_format "$p0" '#{pane_active}' 1

# Explicit templates still run, before subsequent commands in the calling queue.
start_print "$p0" 'set -g @picked %%'
$TMUX send-keys -t "$p0" 1 || exit 1
finish_print "$p1"
wait_format "$p0" '#{@after-picked}' "$p1"

$TMUX set -g @after-print no || exit 1
$TMUX set -g @picked none || exit 1
$TMUX source-file - >"$DIR/print" 2>"$DIR/error" <<EOF &
display-panes -Pd 0 -t $p0 { set -g @picked %% }
set -gF @after-picked '#{@picked}'
set -g @after-print yes
EOF
print_pid=$!
wait_format "$p0" '#{pane_mode}' panes-mode
$TMUX send-keys -t "$p0" 1 || exit 1
finish_print "$p1"
wait_format "$p0" '#{@after-picked}' "$p1"

# Cancellation, timeout, and replacing the mode produce no selection.
for key in q Escape; do
	start_print "$p0"
	$TMUX send-keys -t "$p0" "$key" || exit 1
	finish_print ''
done
$TMUX display-panes -PNd 20 -t "$p0" >"$DIR/print" ||
	    fail "timeout failed"
[ ! -s "$DIR/print" ] || fail "timeout printed a pane"

start_print "$p0"
$TMUX copy-mode -t "$p0" || exit 1
finish_print ''
$TMUX send-keys -t "$p0" -X cancel || exit 1

# A second invocation must not wait if panes-mode is already active.
$TMUX display-panes -d 0 -t "$p0" || exit 1
$TMUX display-panes -Pd 0 -t "$p0" >"$DIR/print" ||
	    fail "existing mode failed"
[ ! -s "$DIR/print" ] || fail "existing mode printed a pane"
$TMUX send-keys -t "$p0" q || exit 1

# -s prints the selected pane from the source window.
source=$($TMUX new-window -dP -F '#{pane_id}' -t m: 'cat') || exit 1
start_print "$p0" -s "$source"
$TMUX send-keys -t "$p0" 0 || exit 1
finish_print "$source"
$TMUX kill-window -t "$source" || exit 1

# Killing the hosting pane, window, or session must release the caller.
for action in pane window session; do
	case "$action" in
	pane)
		victim=$($TMUX split-window -dP -F '#{pane_id}' -t "$p0" 'cat') ||
		    exit 1
		;;
	window)
		victim=$($TMUX new-window -dP -F '#{pane_id}' -t m: 'cat') ||
		    exit 1
		;;
	session)
		victim=$($TMUX new-session -dP -F '#{pane_id}' -s victim 'cat') ||
		    exit 1
		;;
	esac
	start_print "$victim"
	case "$action" in
	pane) $TMUX kill-pane -t "$victim" || exit 1 ;;
	window) $TMUX kill-window -t "$victim" || exit 1 ;;
	session) $TMUX kill-session -t victim || exit 1 ;;
	esac
	finish_print ''
done

# -k prints the selection even when exiting the mode kills its hosting pane.
victim=$($TMUX split-window -dP -F '#{pane_id}' -t "$p0" 'cat') || exit 1
start_print "$victim" -k
$TMUX send-keys -t "$victim" 0 || exit 1
finish_print "$p0"

# Printing for an interactive client opens view mode after panes-mode is freed.
$TMUX bind P display-panes -Pd 0 -t "$p0" || exit 1
$TMUX2 send-keys -t out:0 C-b P || exit 1
wait_format "$p0" '#{pane_mode}' panes-mode
$TMUX2 send-keys -t out:0 1 || exit 1
wait_format "$p0" '#{pane_mode}' view-mode
$TMUX send-keys -t "$p0" q || exit 1
wait_format "$p0" '#{pane_in_mode}' 0

exit 0
