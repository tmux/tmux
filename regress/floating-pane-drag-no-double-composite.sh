#!/bin/sh

# server_client_check_redraw() had `(~c->flags & CLIENT_ALLREDRAWFLAGS)` as
# a fallback condition guarding a call to redraw_client_damage() - for a
# multi-bit mask, `~x & MASK` means "at least one of these bits is unset"
# (almost always true), not "none of these bits are set" as the comment
# and surrounding logic clearly intend. Every floating-pane drag command
# unconditionally sets CLIENT_REDRAWBORDERS (server_redraw_window_borders()
# in cmd-resize-pane.c/cmd-join-pane.c/cmd-split-window.c) alongside
# reporting window damage, so this fallback fired on every single drag
# step, composing the exact same damage rectangle a second time a few
# lines later at the CLIENT_ALLREDRAWFLAGS block - wasted work, not a
# correctness issue, but a clean, deterministic signal to check for via
# the server's own -vv log.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -vv -Ldoublecomp-inner-$$ -f/dev/null"
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

$INNER new-session -d -s inner -x 40 -y 10 'sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g mouse on || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 15 -y 5 -X 5 -Y 2 \
    'sleep 100') || exit 1

$OUTER new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
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

# Top-border drag (a move): each step both reports window damage and sets
# CLIENT_REDRAWBORDERS (server_redraw_window_borders() in the caller),
# which is exactly the combination the buggy fallback misfired on.
mouse 0 "$GRABCOL" "$BORDERROW" M
i=0
steps=6
while [ $i -lt $steps ]; do
	GRABCOL=$((GRABCOL + 1))
	mouse 32 "$GRABCOL" "$BORDERROW" M
	i=$((i + 1))
done
mouse 0 "$GRABCOL" "$BORDERROW" m
sleep 0.3

NEWXOFF=$($INNER display-message -p -t "$FLOAT" '#{pane_left}')
[ "$NEWXOFF" != "$XOFF" ] || fail "sanity: floating pane did not move (still at $XOFF)"

LOG=$(ls tmux-server*.log 2>/dev/null | head -1)
[ -n "$LOG" ] || fail "sanity: no server -vv log was produced"

# Each drag step should compose its damage exactly once. If any rectangle
# was composed twice, the same "x,y WxH" text appears on two consecutive
# composing-damage lines - compare the position+size together, since
# distinct steps commonly share the same size (only the position differs).
dup=$(grep "composing damage" "$LOG" | awk '{print $(NF-1), $NF}' |
    uniq -d | wc -l)
[ "$dup" -eq 0 ] ||
	fail "$dup damage rectangle(s) were composed twice in the same pass"

exit 0
