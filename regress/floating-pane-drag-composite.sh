#!/bin/sh

# Each drag motion must draw the floating pane content once.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -Ldoublecomp-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Ldoublecomp-outer-$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	cd /
	rm -rf "$DIR"
}
trap cleanup 0 1 15

mouse()
{
	sequence=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$OUTER send-keys -t outer:0.0 -l "$sequence" || exit 1
	sleep 0.15
}

$INNER new-session -d -s inner -x 40 -y 15 "printf '\\033[15;1HOUTSIDE'; exec sleep 100" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -g status-interval 0 || exit 1
$INNER set-option -g automatic-rename off || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 15 -y 5 -X 5 -Y 2 \
    "printf 'DRAGMARK'; exec sleep 100") || exit 1

$OUTER new-session -d -s outer -x 40 -y 15 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER set-option -g default-terminal screen-256color || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Ldoublecomp-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1
sleep 0.5

XOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
YOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_top}')
GRABCOL=$((XOFF + 3))
BORDERROW=$YOFF

# Begin capture after focus changes from the initial mouse press have settled.
mouse 0 "$GRABCOL" "$BORDERROW" M
$OUTER pipe-pane -O -t outer:0.0 "cat >'$DIR/output'" || exit 1
$INNER refresh-client || exit 1
sleep 0.5
grep -aq DRAGMARK "$DIR/output" || fail "capture missed floating pane content"
grep -aq OUTSIDE "$DIR/output" || fail "capture missed untouched row"
offset=$(wc -c <"$DIR/output")

i=0
steps=6
while [ "$i" -lt "$steps" ]; do
	GRABCOL=$((GRABCOL + 1))
	mouse 32 "$GRABCOL" "$BORDERROW" M
	i=$((i + 1))
done
mouse 0 "$GRABCOL" "$BORDERROW" m
sleep 0.3

NEWXOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
[ "$NEWXOFF" -eq "$((XOFF + steps))" ] || fail "floating pane did not move six columns"
tail -c +"$((offset + 1))" "$DIR/output" >"$DIR/drag-output"
n=$(perl -0777 -ne '$n = () = /DRAGMARK/g; print "$n\n"' "$DIR/drag-output")
[ "$n" -eq "$steps" ] ||
	fail "$steps drag motions wrote the floating pane content $n times"

exit 0
