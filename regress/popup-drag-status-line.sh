#!/bin/sh

# Moving a popup away from the tmux status area must restore status and window
# cells. Window damage alone cannot describe cells outside the window scene,
# and top status lines must be removed before translating to window rows.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
BASE=$DIR/base
CAPTURE=$DIR/capture
INNER=
OUTER=
POPUP_PID=
N=0

fail()
{
	echo "$*" >&2
	[ -s "$CAPTURE" ] && cat "$CAPTURE" >&2
	exit 1
}

cleanup_scene()
{
	[ -n "$POPUP_PID" ] && kill "$POPUP_PID" 2>/dev/null
	[ -n "$OUTER" ] && $OUTER kill-server 2>/dev/null
	[ -n "$INNER" ] && $INNER kill-server 2>/dev/null
	POPUP_PID=
}

cleanup()
{
	cleanup_scene
	rm -rf "$DIR"
}
trap cleanup 0 1 15

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CLIENT=$($INNER list-clients -F '#{client_name}' 2>/dev/null)
		[ -n "$CLIENT" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

wait_outer_has()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "outer client did not show $marker"
}

wait_rows_restored()
{
	first=$1
	last=$2
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$CAPTURE" 2>/dev/null || true
		sed -n "${first},${last}p" "$BASE" >"$DIR/want"
		sed -n "${first},${last}p" "$CAPTURE" >"$DIR/got"
		cmp -s "$DIR/want" "$DIR/got" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "rows $first-$last under the old popup were not restored"
}

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.1
}

setup()
{
	position=$1
	lines=$2

	cleanup_scene
	N=$((N + 1))
	INNER="$TEST_TMUX -Lpopup-status-inner-$$-$N -f/dev/null"
	OUTER="$TEST_TMUX -Lpopup-status-outer-$$-$N -f/dev/null"

	C="sh -c 'i=1; while [ \$i -le 10 ]; do printf \"\\033[%d;1HAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\" \$i; i=\$((i + 1)); done; exec sleep 100'"
	$INNER new-session -d -s inner -x 40 -y 10 "$C" || exit 1
	$INNER set-option -g window-size manual || exit 1
	$INNER set-option -g mouse on || exit 1
	if [ "$lines" -eq 1 ]; then
		$INNER set-option -g status on || exit 1
	else
		$INNER set-option -g status "$lines" || exit 1
	fi
	$INNER set-option -g status-position "$position" || exit 1
	$INNER set-option -g status-format[0] \
	    'STATUS-LINE-MARK-01234567890123456789012' || exit 1
	if [ "$lines" -gt 1 ]; then
		$INNER set-option -g status-format[1] \
		    'SECOND-STATUS-MARK-012345678901234567890' || exit 1
	fi
	$INNER set-option -g status-interval 0 || exit 1

	$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
	$OUTER set-option -g status off || exit 1
	$OUTER set-option -g window-size manual || exit 1
	$OUTER respawn-pane -k -t outer:0.0 \
	    "$TEST_TMUX -Lpopup-status-inner-$$-$N -f/dev/null attach-session -t inner" ||
	    exit 1

	wait_for_client
	wait_outer_has STATUS-LINE-MARK
	$OUTER capture-pane -p -t outer:0.0 >"$BASE" || exit 1
}

open_popup()
{
	y=$1
	height=$2
	$INNER display-popup -t "$CLIENT" -x 0 -y "$y" -w 16 -h "$height" -E \
	    "sh -c 'printf POPUP-MARKER; exec sleep 100'" &
	POPUP_PID=$!
	wait_outer_has POPUP-MARKER
}

if [ "${STATUS_CASE:-all}" != top ]; then
	# A popup clipped against a one-line bottom status must restore that line.
	setup bottom 1
	open_popup 10 3
	mouse 0 10 8 M
	mouse 32 11 8 M
	mouse 32 25 4 M
	mouse 0 25 4 m
	wait_rows_restored 10 10
fi

if [ "${STATUS_CASE:-all}" != bottom ]; then
	# A popup beginning in a two-line top status also covers the first window
	# rows. Restore both coordinate spaces after it moves.
	setup top 2
	open_popup 1 4
	mouse 0 10 1 M
	mouse 32 11 1 M
	mouse 32 25 7 M
	mouse 0 25 7 m
	wait_rows_restored 1 4
fi

exit 0
