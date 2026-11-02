#!/bin/sh

# Pane transitions identify the affected pane, independently of focus, and
# fire only when state changes. Hook commands must not recurse.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
trap 'stop_server $TMUX; rm -rf "$DIR"' 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
fail()
{
	echo "$*" >&2
	exit 1
}

# Run a command against the test server.
run()
{
	$TMUX "$@" || fail "failed: $*"
}

# Wait for all expected hook events to be recorded in order.
events()
{
	wait_for_output "$1" $TMUX show -gv @events ||
	    fail "expected events: $1"
}

A=$(run new-session -dP -F '#{pane_id}' -x 120 -y 40 'exec cat') || exit 1
B=$(run split-window -hdP -F '#{pane_id}' -t "$A" '') || exit 1
run set -g @events ''
for hook in pane-floated pane-tiled pane-hidden pane-shown; do
	run set-hook -g "$hook" \
	    'set -agF @events "#{hook}:#{hook_pane};"'
done

# Float and tile explicit targets without changing the active pane.
run break-pane -Wd -s "$B"
events "pane-floated:$B;"
run join-pane -d -s "$B" -t "$B"
events "pane-floated:$B;pane-tiled:$B;"
[ "$(run display -p '#{pane_id}')" = "$A" ] || fail 'focus changed'

# Repeating hide or show does not fire another transition.
run set -g @events ''
run resize-pane -h -t "$B"
run resize-pane -h -t "$B"
run resize-pane -dH -t "$B"
run resize-pane -dH -t "$B"
events "pane-hidden:$B;pane-shown:$B;"

# Selecting a hidden pane fires pane-shown, marking it does not.
run set -g @events ''
run resize-pane -h -t "$B"
run select-pane -m -t "$B"
events "pane-hidden:$B;"
run select-pane -t "$B"
events "pane-hidden:$B;pane-shown:$B;"

# Refocusing after hiding the active pane does not change hook_pane.
run set -g @events ''
run resize-pane -h -t "$B"
events "pane-hidden:$B;"
[ "$(run display -p '#{pane_id}')" = "$A" ] || fail 'hide did not refocus'
run resize-pane -H
events "pane-hidden:$B;pane-shown:$B;"

# Zoom/unzoom are separate operations, not float/tile hook transitions.
run set -g @events ''
run resize-pane -Z -t "$B"
run resize-pane -Z -t "$B"
events ''

# Implicit hiding of fullscreen covers and desktop cycling use the same hooks.
run resize-pane -Z -t "$B"
run select-pane -t "$A"
events "pane-hidden:$B;"
run resize-pane -H
events "pane-hidden:$B;pane-shown:$B;"
run set -g @events ''
run resize-pane -C
events "pane-hidden:$B;"
run resize-pane -C
events "pane-hidden:$B;pane-shown:$B;"
run resize-pane -Z -t "$B"

# Hook commands can redistribute without reentering transition hooks.
run set -g @events ''
run set-hook -g pane-floated[redistribute] 'select-layout -aE'
run set-hook -g pane-tiled[redistribute] 'select-layout -aE'
run break-pane -Wd -s "$B"
run join-pane -d -s "$B" -t "$B"
events "pane-floated:$B;pane-tiled:$B;"

# Moving a hidden float to another tiled position reports both transitions.
run set -g @events ''
run break-pane -Wd -s "$B"
run resize-pane -h -t "$B"
run join-pane -d -s "$B" -t "$A"
events "pane-floated:$B;pane-hidden:$B;pane-tiled:$B;pane-shown:$B;"

# Breaking a hidden pane into its own window also shows it.
run set -g @events ''
run resize-pane -h -t "$B"
run break-pane -d -s "$B"
events "pane-hidden:$B;pane-shown:$B;"

exit 0
