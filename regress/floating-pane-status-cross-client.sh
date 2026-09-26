#!/bin/sh

# redraw_damage_refresh_status() (screen-redraw.c) force-regenerates a
# pane's border-status title when a damage rectangle touches it, guarded
# by the per-pane PANE_NEWSTATUS flag. window_make_pane_status() formats
# pane-border-format using the requesting client's own context (so e.g.
# #{client_name} differs per client), but wp->status_screen/PANE_NEWSTATUS
# are shared by every client viewing the pane. With two clients attached
# to the same session, whichever client's damage pass runs first renders
# its own text and sets the flag; the other client's damage pass, finding
# the flag already set, used to skip rendering entirely and reuse
# whatever was already there.
#
# This checks the actual server-side decision via the -vv log rather than
# a visual capture: an unrelated periodic client status-refresh reliably
# repaints each client's title correctly again within the same tick right
# after the buggy decision is made, before anything is ever flushed to
# either terminal, so the wrong content this bug produces is never
# visible to any external capture - the log is the only place the actual
# bug (or its absence) can be observed.
#
# A floating pane with its own pane-border-status is positioned so that a
# damage rectangle from an *unrelated* palette change (OSC 4) in the
# underlying tiled pane - whose own geometry spans the whole window -
# overlaps the floating pane's title row without touching its content,
# giving a damage-only trigger with no side effect that would otherwise
# force a normal (non-buggy) full per-client status re-render in the same
# pass and mask the result either way.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -vv -Lstatuscc-inner-$$ -f/dev/null"
OUTER1="$TEST_TMUX -Lstatuscc-outer1-$$ -f/dev/null"
OUTER2="$TEST_TMUX -Lstatuscc-outer2-$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER1 kill-server 2>/dev/null
	$OUTER2 kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	cd /
	rm -rf "$DIR"
}
trap cleanup 0 1 15

BASEEMITTER=$DIR/base-emitter.pl
cat >"$BASEEMITTER" <<'PERL'
use strict;
use warnings;

$| = 1;
my $line = <STDIN>;
print "\e]4;1;rgb:11/22/33\e\\";
sleep 100;
PERL

$INNER new-session -d -s inner -x 40 -y 10 "perl '$BASEEMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -g pane-border-status top || exit 1
$INNER set-option -g pane-border-format 'C=#{client_name}' || exit 1
BASE=$($INNER list-panes -t inner -F '#{pane_id}') || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 20 -y 3 -X 5 -Y 4 \
    'sleep 100') || exit 1

$OUTER1 new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER1 set-option -g status off || exit 1
$OUTER1 set-option -g window-size manual || exit 1
$OUTER1 set-option -g default-terminal screen-256color || exit 1
$OUTER1 respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lstatuscc-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1
sleep 0.5
NAME1=$($INNER list-clients -F '#{client_name}') || exit 1

$OUTER2 new-session -d -s outer -x 40 -y 10 'sleep 100' || exit 1
$OUTER2 set-option -g status off || exit 1
$OUTER2 set-option -g window-size manual || exit 1
$OUTER2 set-option -g default-terminal screen-256color || exit 1
$OUTER2 respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Lstatuscc-inner-$$ -f/dev/null attach-session -t inner" ||
    exit 1
sleep 0.5
ALLNAMES=$($INNER list-clients -F '#{client_name}') || exit 1
NAME2=$(echo "$ALLNAMES" | grep -v "^$NAME1\$")
[ -n "$NAME2" ] || fail "sanity: could not identify the second client"

# Let any attach-driven full redraw (and its own, non-buggy, per-client
# status render) finish completely before triggering the damage-only
# palette update.
sleep 1.5

$INNER send-keys -t "$BASE" Enter || exit 1
sleep 0.5

LOG=$(ls tmux-server*.log 2>/dev/null | head -1)
[ -n "$LOG" ] || fail "sanity: no server -vv log was produced"

n1=$(grep -c "regenerated pane .* status for $NAME1\$" "$LOG")
n2=$(grep -c "regenerated pane .* status for $NAME2\$" "$LOG")
[ "$n1" -ge 1 ] || fail "damage pass never regenerated $NAME1's own status - it reused whatever the other client's render left behind"
[ "$n2" -ge 1 ] || fail "damage pass never regenerated $NAME2's own status - it reused whatever the other client's render left behind"

exit 0
