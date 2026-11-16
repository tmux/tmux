#!/bin/sh

. ./regress-common.inc

# Capture an attached client's pane borders through an outer tmux and compare
# with screen-redraw-results. Check focus changes and option transitions on the
# same client so stale borders are visible. Run with GENERATE=1 to update goldens.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$TMP/outer -f/dev/null"
TMUX2="$TEST_TMUX -S$TMP/inner -f/dev/null"
RESULTS=screen-redraw-results
window=

cleanup()
{
	stop_servers
	rm -rf "$TMP"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$*" >&2
	exit 1
}

compare()
{
	if [ -n "$GENERATE" ]; then
		sleep 1
		$TMUX capture-pane -pe >"$TMP/capture" || exit 1
		cp "$TMP/capture" "$RESULTS/$1.result" || exit 1
		echo "generated $1"
	else
		wait_until capture_matches_file "$RESULTS/$1.result" "$TMP/capture" \
		    $TMUX capture-pane -pe || {
			diff -u "$RESULTS/$1.result" "$TMP/capture" >&2
			fail "scene $1 differs from $RESULTS/$1.result"
		}
	fi
}

# Keep a blank initial window alive while replacing the visible scene. Pane
# content labels are independent of pane IDs, which increase between scenes.
new_scene()
{
	if [ -n "$window" ]; then
		$TMUX2 kill-window -t "$window" || exit 1
	fi
	window=$($TMUX2 new-window -P -F '#{window_id}' \
	    'printf P0; exec cat') || exit 1
	$TMUX2 resize-window -t "$window" -x40 -y12 || exit 1
	case "$1" in
	horizontal|nested)
		$TMUX2 split-window -dh -t "$window.0" \
		    'printf P1; exec cat' || exit 1 ;;
	esac
	case "$1" in
	vertical)
		$TMUX2 split-window -dv -t "$window.0" \
		    'printf P1; exec cat' || exit 1 ;;
	nested)
		$TMUX2 split-window -dv -t "$window.1" \
		    'printf P2; exec cat' || exit 1 ;;
	esac
	$TMUX2 select-pane -t "$window.0" || exit 1
}

$TMUX2 new-session -d -x40 -y12 'exec cat' || exit 1
$TMUX2 set -g status off \; set -g window-size manual \; \
    set -g pane-border-lines simple \; set -g pane-border-indicators off \; \
    set -g pane-border-format 'T#{pane_index}' \; \
    set -g pane-border-style fg=red \; \
    set -g pane-active-border-style fg=green || exit 1
$TMUX new-session -d -x40 -y12 'exec cat' || exit 1
$TMUX set -g status off \; set -g window-size manual \; \
    set -g default-terminal tmux-256color \; \
    respawn-pane -k "$TMUX2 attach-session" || exit 1
wait_for_clients 1 $TMUX2 || fail "inner client did not attach"

# Each tiled pane gets its own complete rectangle, including the window edges.
for layout in single horizontal vertical; do
	new_scene "$layout"
	$TMUX2 set -w pane-border-type separate || exit 1
	compare separate-$layout
	if [ "$layout" = single ]; then
		$TMUX2 set -w pane-border-type separate-active || exit 1
		compare separate-single
	fi
done

# Focus changes must hide the previous active border and draw the next one.
# Returning to joined must remove the outer and doubled borders completely.
new_scene nested
compare separate-joined
$TMUX2 set -w pane-border-type separate || exit 1
compare separate-nested
$TMUX2 set -w pane-border-type separate-active || exit 1
for pane in 0 1 2; do
	$TMUX2 select-pane -t "$window.$pane" || exit 1
	compare separate-active-$pane
done
$TMUX2 select-pane -t "$window.0" \; \
    set -w pane-border-type separate || exit 1
compare separate-nested
$TMUX2 set -w pane-border-type joined || exit 1
compare separate-joined
$TMUX2 set -w pane-border-type separate || exit 1
compare separate-nested

# Pane content and border ownership follow swaps and joins. Removing the new
# pane restores the original frame without requiring a forced redraw.
$TMUX2 swap-pane -s "$window.0" -t "$window.2" \; \
    select-pane -t "$window.0" || exit 1
compare separate-swap
$TMUX2 swap-pane -s "$window.0" -t "$window.2" \; \
    select-pane -t "$window.0" || exit 1
compare separate-nested
donor=$($TMUX2 new-window -dP -F '#{pane_id}' 'printf JOIN; exec cat') || exit 1
$TMUX2 join-pane -dh -s "$donor" -t "$window.0" || exit 1
compare separate-join
$TMUX2 kill-pane -t "$donor" || exit 1
compare separate-nested

# Titles occupy their border row; the opposite edge remains drawn. Inactive
# title text remains visible with separate-active, but its border is blank.
for position in top bottom; do
	$TMUX2 set -w pane-border-status "$position" || exit 1
	compare separate-status-$position
	$TMUX2 set -w pane-border-type separate-active \; \
	    select-pane -t "$window.1" || exit 1
	compare separate-active-status-$position
	$TMUX2 set -w pane-border-type separate \; \
	    select-pane -t "$window.0" || exit 1
done
$TMUX2 set -w pane-border-status off || exit 1
compare separate-nested

# Reserved scrollbars stay inside the border, on either side. Capturing with
# escapes also checks scrollbar placement and active/inactive border colours.
for side in left right; do
	$TMUX2 set -w pane-scrollbars on \; \
	    set -w pane-scrollbars-position "$side" \; \
	    set -w pane-scrollbars-style 'fg=blue,bg=yellow,width=2,pad=1' || exit 1
	compare separate-scrollbar-$side
	$TMUX2 set -w pane-border-type separate-active \; \
	    select-pane -t "$window.1" || exit 1
	compare separate-active-scrollbar-$side
	$TMUX2 set -w pane-border-type separate \; \
	    select-pane -t "$window.0" || exit 1
done
$TMUX2 set -w pane-scrollbars off || exit 1
compare separate-nested

# A floating pane obscures the doubled tiled borders underneath. Moving focus
# in and out must preserve that occlusion even when its own border is hidden.
fp=$($TMUX2 new-pane -dP -F '#{pane_id}' -x12 -y6 -X14 -Y3 -B simple \
    'printf FLOAT; exec cat') || exit 1
compare separate-floating
$TMUX2 set -w pane-border-type separate-active \; \
    select-pane -t "$fp" || exit 1
compare separate-active-floating
$TMUX2 select-pane -t "$window.0" || exit 1
compare separate-active-floating-inactive
$TMUX2 kill-pane -t "$fp" \; set -w pane-border-type separate || exit 1
compare separate-nested

# Preview borders use their own rendering path and hide reserved scrollbars.
$TMUX2 set -w pane-scrollbars on \; \
    set -w pane-scrollbars-position left \; \
    set -w pane-scrollbars-style 'fg=blue,bg=yellow,width=2,pad=1' \; \
    set -w display-panes-format 'O#{pane_index}' \; \
    display-panes -d0 -t "$window.0" || exit 1
compare separate-display-panes
$TMUX2 send-keys -t "$window.0" q || exit 1
$TMUX2 set -w pane-border-type separate-active \; \
    display-panes -d0 -t "$window.0" || exit 1
compare separate-display-panes
$TMUX2 send-keys -t "$window.0" q || exit 1

new_scene single
$TMUX2 set -w pane-border-type separate \; \
    set -w display-panes-format 'O#{pane_index}' \; display-panes -d0 || exit 1
compare separate-display-panes-single
$TMUX2 send-keys q || exit 1

exit 0
