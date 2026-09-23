#!/bin/sh

# Windows Terminal cannot be identified by the XTVERSION mechanism
# tty_default_features() (tty-features.c) otherwise uses for every other
# terminal in its table - its maintainers have explicitly declined to
# implement it (github.com/microsoft/terminal issue 18382, closed
# not_planned). tty_term_create() (tty-term.c) instead detects it via the
# WT_SESSION environment variable Windows Terminal sets for every child
# process, granting it the "margins" (DECSLRM) terminal feature.
#
# Without DECSLRM, a pane that doesn't span the terminal's full width -
# because pane-scrollbars is on (the scrollbar occupies a column) or the
# pane is one of a side-by-side split - can't use the fast native-scroll
# path (tty_cmd_linefeed()/scrollup()/scrolldown()/reverseindex(), tty.c:
# "(!tty_full_width(tty, ctx) && !tty_use_margin(tty))") and falls back to
# tty_redraw_region()'s full manual repaint on every single scroll - a
# real, confirmed source of flicker (and, separately, of image content not
# surviving a scroll).
#
# This checks the actual server-side decision via the -vv log: a pane with
# pane-scrollbars on, scrolled several times, must never fall back to
# tty_redraw_region() when the attaching client's environment carries
# WT_SESSION - and must (as a sanity check that this scenario would
# otherwise hit the fallback at all) when it does not.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
cd "$DIR" || exit 1
INNER="$TEST_TMUX -vv -Lwtsession-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lwtsession-outer-$$ -f/dev/null"

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
	attachcmd=$2

	rm -f tmux-server*.log

	$INNER new-session -d -s inner -x 40 -y 6 'exec sh' || exit 1
	$INNER set -g status off || exit 1
	$INNER set -g window-size manual || exit 1
	$INNER set -g pane-scrollbars on || exit 1

	$OUTER new-session -d -x 40 -y 6 || exit 1
	OUTERPANE=$($OUTER list-panes -F '#{pane_id}') || exit 1
	$OUTER set -g status off || exit 1
	$OUTER set -g window-size manual || exit 1
	$OUTER set -g default-terminal screen-256color || exit 1
	$OUTER send-keys -t "$OUTERPANE" -l "$attachcmd" || exit 1
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

# Phase 1: WT_SESSION present in the attaching client's environment - must
# never fall back to a full region redraw.
n_with=$(run_scroll_phase "with WT_SESSION" \
    "WT_SESSION=deadbeef-0000-0000-0000-000000000000 $INNER attach -t inner")
[ "$n_with" -eq 0 ] ||
	fail "with WT_SESSION set, scrolling a scrollbar-enabled pane still fell back to a full region redraw ($n_with times) - margins was not granted"

# Phase 2: sanity check - without WT_SESSION, the same scenario must
# actually hit the fallback, proving phase 1 wasn't accidentally trivial.
# Explicitly strip WT_SESSION/WSLENV rather than relying on them being
# unset in the ambient environment - this test may itself be run from
# inside a real Windows Terminal/WSL session.
n_without=$(run_scroll_phase "without WT_SESSION" \
    "env -u WT_SESSION -u WSLENV $INNER attach -t inner")
[ "$n_without" -gt 0 ] ||
	fail "sanity: without WT_SESSION, scrolling a scrollbar-enabled pane never fell back to a full region redraw - this scenario no longer exercises the bug this test checks for"

exit 0
