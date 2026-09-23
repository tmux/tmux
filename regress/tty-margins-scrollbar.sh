#!/bin/sh

# A pane that doesn't span the terminal's full width - because
# pane-scrollbars is on (the scrollbar occupies a column) or the pane is
# one of a side-by-side split - needs DECSLRM (left/right margin) support
# to use the fast native-scroll path (tty_cmd_linefeed()/scrollup()/
# scrolldown()/reverseindex(), tty.c:
# "(!tty_full_width(tty, ctx) && !tty_use_margin(tty))"). Without it, every
# single scroll falls back to tty_redraw_region()'s full manual repaint of
# the whole region - a real, confirmed source of flicker (and, separately,
# of image content not surviving a scroll in branches with image support).
#
# tty_default_features() (tty-features.c) grants the "margins" feature to
# several terminals it can positively identify via XTVERSION/DA2 (mintty,
# iTerm2, WezTerm, ghostty, XTerm-as-VT420) - this checks the actual
# server-side scroll decision via the -vv log for the underlying mechanism
# those table entries all rely on, using the terminal-features option
# directly (which any of them - or a user's own terminal-overrides -
# ultimately feed into) rather than simulating any one terminal's
# identification handshake.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -vv -Lmarginsscrollbar-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lmarginsscrollbar-outer-$$ -f/dev/null"

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

run_scroll_phase()
{
	label=$1
	margins=$2

	rm -f tmux-server*.log

	$INNER new-session -d -s inner -x 40 -y 6 'exec sh' || exit 1
	$INNER set -g status off || exit 1
	$INNER set -g window-size manual || exit 1
	$INNER set -g pane-scrollbars on || exit 1
	if [ "$margins" = "on" ]; then
		$INNER set -as terminal-features ',*:margins' || exit 1
	fi

	$OUTER new-session -d -x 40 -y 6 || exit 1
	OUTERPANE=$($OUTER list-panes -F '#{pane_id}') || exit 1
	$OUTER set -g status off || exit 1
	$OUTER set -g window-size manual || exit 1
	$OUTER set -g default-terminal screen-256color || exit 1
	$OUTER send-keys -t "$OUTERPANE" -l "$INNER attach -t inner" || exit 1
	$OUTER send-keys -t "$OUTERPANE" Enter || exit 1
	sleep 1

	wait_for_client

	i=0
	while [ "$i" -lt 8 ]; do
		$INNER send-keys -t inner Enter || exit 1
		sleep 0.2
		i=$((i + 1))
	done
	sleep 0.3

	LOG=$(ls tmux-server*.log 2>/dev/null | head -1)
	[ -n "$LOG" ] || fail "$label: sanity: no server -vv log was produced"

	n=$(grep -c "tty_redraw_region.*large region redraw" "$LOG")

	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null

	echo "$n"
}

# Phase 1: margins granted - must never fall back to a full region redraw.
n_with=$(run_scroll_phase "with margins" "on")
[ "$n_with" -eq 0 ] ||
	fail "with margins granted, scrolling a scrollbar-enabled pane still fell back to a full region redraw ($n_with times)"

# Phase 2: sanity check - without margins, the same scenario must actually
# hit the fallback, proving phase 1 wasn't accidentally trivial.
n_without=$(run_scroll_phase "without margins" "off")
[ "$n_without" -gt 0 ] ||
	fail "sanity: without margins, scrolling a scrollbar-enabled pane never fell back to a full region redraw - this scenario no longer exercises the bug this test checks for"

exit 0
