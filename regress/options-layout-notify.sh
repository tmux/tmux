#!/bin/sh

. ./regress-common.inc

# Option changes which resize or move panes notify hooks and control clients
# once per affected window. No-op and colour-only changes do not notify.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
DIR=$(test_tmpdir) || exit 1
OUT=$DIR/output
PID=

cleanup()
{
	exec 3>&-
	[ -n "$PID" ] && kill "$PID" 2>/dev/null
	stop_server $TMUX || exit 1
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

has_output()
{
	grep -Fxq -- "$1" "$OUT"
}

notification_count()
{
	awk '/^%layout-change / { n++ } END { print n + 0 }' "$OUT"
}

serial=0
check_change()
{
	expected_window=$1
	expected_count=$2
	shift 2
	serial=$((serial + 1))
	$TMUX set -g @layout_count 0 \; set -g @layout_window none || exit 1
	before=$(notification_count)
	$TMUX "$@" || fail "command failed: $*"
	printf 'display-message -p done-%s\n' "$serial" >&3
	wait_until has_output "done-$serial" || fail "control client did not sync"
	after=$(notification_count)
	if [ "$expected_window" = none ]; then
		[ "$after" -eq "$before" ] || fail "$*: unexpected notification"
		[ "$($TMUX show -gv @layout_count)" = 0 ] ||
			fail "$*: unexpected layout hook"
	else
		[ "$after" -eq $((before + expected_count)) ] ||
			fail "$*: expected $expected_count notifications, got $((after - before))"
		last_window=$(awk '/^%layout-change / { w = $2 } END { print w }' "$OUT")
		[ "$last_window" = "$expected_window" ] ||
			fail "$*: notification for $last_window, expected $expected_window"
		wait_for_output "$expected_count" $TMUX show -gv @layout_count ||
			fail "$*: expected $expected_count layout hooks"
		[ "$($TMUX show -gv @layout_window)" = "$expected_window" ] ||
			fail "$*: layout hook targeted the wrong window"
	fi
}

$TMUX new-session -d -s notify -n main -x 80 -y 24 'cat' || exit 1
$TMUX set -g status off \; set -g window-size manual || exit 1
$TMUX split-window -dh -t notify:main 'cat' || exit 1
$TMUX new-window -d -t notify: -n other 'cat' || exit 1
main=$($TMUX display -p -t notify:main '#{window_id}') || exit 1
other=$($TMUX display -p -t notify:other '#{window_id}') || exit 1

mkfifo "$DIR/input" || exit 1
: >"$OUT"
$TMUX -C attach-session -f new-layouts -t notify <"$DIR/input" >"$OUT" 2>&1 &
PID=$!
exec 3>"$DIR/input"
printf 'display-message -p ready\n' >&3
wait_until has_output ready || fail "control client did not attach"

$TMUX set-hook -g window-layout-changed \
    'set -gF @layout_count "#{e|+:#{@layout_count},1}"; set -gF @layout_window "#{hook_window}"' ||
	fail "set layout hook failed"

check_change "$main" 1 set -w -t notify:main pane-scrollbars on
check_change none 0 set -w -t notify:main pane-scrollbars on
check_change "$main" 1 set -p -t notify:main.0 pane-scrollbars-style 'width=3,pad=1'
check_change none 0 set -p -t notify:main.0 pane-scrollbars-style 'fg=red,width=3,pad=1'
check_change "$main" 1 set -w -t notify:main pane-scrollbars-position left
check_change none 0 set -w -t notify:main pane-scrollbars-position left
check_change "$main" 1 set -w -t notify:main pane-border-status top
check_change "$main" 1 set -w -t notify:main pane-border-status bottom
check_change "$main" 1 set -w -t notify:main pane-border-status off
check_change "$main" 1 set -w -t notify:main pane-scrollbars off
check_change none 0 set -p -t notify:main.0 pane-scrollbars-style 'width=4,pad=2'
check_change none 0 set -w -t notify:main pane-scrollbars-position right
check_change none 0 set -w -t notify:main pane-scrollbars auto-hide
check_change none 0 set -w -t notify:main pane-scrollbars modal
check_change "$main" 1 set -w -t notify:main pane-scrollbars on
check_change "$main" 1 set -pu -t notify:main.0 pane-scrollbars-style

# The main window has a local override, so only the other window changes.
check_change "$other" 1 set -g pane-scrollbars on

# Keep existing command counts: resize-pane emits twice, split-window once.
check_change "$main" 2 resize-pane -t notify:main.0 -R 1
check_change "$main" 1 split-window -dh -t notify:main.0 'cat'

exit 0
