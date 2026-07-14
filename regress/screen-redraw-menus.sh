#!/bin/sh

# Exercise drawing of window menus in the screen-redraw.c scene. Menus are
# window-owned and drawn as REDRAW_MENU spans over panes. Captures use -e so
# menu-style, menu-selected-style, and menu-border-style are included.
#
# Run with GENERATE=1 to (re)create the golden files.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX=
TMUX2=
SETUP=0
RESULTS=screen-redraw-results

TMP=$(mktemp)
TMP2=$(mktemp)

cleanup() {
	rm -f "$TMP" "$TMP2"
	[ -n "$TMUX" ] && $TMUX kill-server 2>/dev/null
	[ -n "$TMUX2" ] && $TMUX2 kill-server 2>/dev/null
}
trap cleanup 0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

trim() {
	awk '
		{ lines[NR] = $0 }
		END {
			n = NR
			while (n > 0 && lines[n] == "")
				n--
			for (i = 1; i <= n; i++)
				print lines[i]
		}
	' "$TMP" >"$TMP2" || exit 1
	mv "$TMP2" "$TMP" || exit 1
}

compare() {
	sleep 1
	$TMUX capturep -pe >$TMP || exit 1
	trim
	if [ -n "$GENERATE" ]; then
		cp $TMP "$RESULTS/$1.result" || exit 1
		echo "generated $1"
	else
		cmp -s $TMP "$RESULTS/$1.result" || \
			fail "scene $1 differs from $RESULTS/$1.result"
	fi
}

C="sh -c 'i=0; while [ \$i -lt 13 ]; do printf \"MENU%02d abcdefghij\n\" \$i; i=\$((i + 1)); done; exec sleep 100'"

setup() {
	[ -n "$TMUX" ] && $TMUX kill-server 2>/dev/null
	[ -n "$TMUX2" ] && $TMUX2 kill-server 2>/dev/null
	SETUP=$((SETUP + 1))
	TMUX="$TEST_TMUX -LtestA$$-$SETUP -f/dev/null"
	TMUX2="$TEST_TMUX -LtestB$$-$SETUP -f/dev/null"
	$TMUX2 new -d -x"$1" -y"$2" "$C" || exit 1
	$TMUX2 set -g status off || exit 1
	$TMUX2 set -g window-size manual || exit 1
	$TMUX2 resizew -x"$1" -y"$2" || exit 1
	$TMUX2 setw menu-style "fg=colour250,bg=colour235" || exit 1
	$TMUX2 setw menu-selected-style "fg=colour16,bg=colour220" || exit 1
	$TMUX2 setw menu-border-style "fg=colour45,bg=colour235" || exit 1

	$TMUX new -d -x"$1" -y"$2" || exit 1
	$TMUX set -g status off || exit 1
	$TMUX set -g window-size manual || exit 1
	$TMUX set -g default-terminal "tmux-256color" || exit 1
	$TMUX send -l "$TMUX2 attach" || exit 1
	$TMUX send Enter || exit 1
	sleep 1
}

menu() {
	$TMUX2 display-menu -T "Menu" -C 1 "$@" \
	    "Alpha item" a "" \
	    "Beta item" b "" \
	    "" "" "" \
	    "Gamma item" g "" || exit 1
	sleep 1
}

# Basic menu over a single pane.
setup 40 14
menu -x6 -y8
compare menu-basic

# Menu over a split: drawn on top of the pane border.
setup 40 14
$TMUX2 splitw -h "$C" || exit 1
menu -x8 -y9
compare menu-over-split

# Menu with no border lines.
setup 40 14
menu -b none -x6 -y8
compare menu-noborder

# Menu with double border lines.
setup 40 14
menu -b double -x6 -y8
compare menu-double

# Window menu styles are re-expanded from window options when redrawn.
setup 40 14
menu -x6 -y8
$TMUX2 setw menu-style "fg=colour196,bg=colour22" || exit 1
$TMUX2 setw menu-selected-style "fg=colour231,bg=colour88" || exit 1
$TMUX2 setw menu-border-style "fg=colour51,bg=colour22" || exit 1
$TMUX2 refresh-client -S || exit 1
compare menu-restyled

# Menu wider than the window: it should be accepted and clipped by the scene.
setup 12 6
$TMUX2 display-menu -T "Menu" -C 0 -x0 -y5 \
    "This item is much wider than the window" t "" \
    "Second item is also too wide" s "" || exit 1
compare menu-clipped

exit 0
