#!/bin/sh

# Observe a nested client's cursor through a pipe and from the outer server.
# Copy mode has its own cursor settings and restores the pane cursor on exit.

PATH=/bin:/usr/bin
TERM=tmux-256color
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT="$TEST_TMUX -LcopycursorA$$ -f/dev/null"
IN="$TEST_TMUX -LcopycursorB$$ -f/dev/null"
log=$(mktemp) || exit 1
trap '$OUT kill-server 2>/dev/null; $IN kill-server 2>/dev/null; rm -f "$log"' EXIT

fail() {
	echo "[FAIL] $*"
	exit 1
}

check_style() {
	tail -c +"$((offset + 1))" "$log" |
	    grep -qF "$(printf '\033[%s q' "$1")" ||
	    fail "$2: expected cursor style $1"
	offset=$(wc -c < "$log")
}

check_colour() {
	actual=$($OUT display -p '#{cursor_colour}')
	[ "$actual" = "$1" ] ||
	    fail "$2: cursor colour $actual, expected $1"
}

$IN new -d -x80 -y23 "printf '\033[4 q'; exec sleep 1000" || exit 1
$IN set -s escape-time 10 || exit 1
$IN set -gw cursor-colour '#ff0000' || exit 1
$IN set -gw cursor-style bar || exit 1
$OUT new -d -x80 -y24 "$IN attach" || exit 1
$OUT set -g status off || exit 1
sleep 1
$OUT pipe-pane -O "cat > '$log'" || exit 1

[ "$($IN show -gwv copy-mode-cursor-style)" = inherit ] ||
    fail "default is not inherit"
[ -z "$($IN show -gwv copy-mode-cursor-colour)" ] ||
    fail "default colour is not empty"

offset=$(wc -c < "$log")
$OUT send C-b '[' || exit 1
sleep 1
check_style 6 "inherited copy mode"
check_colour '#ff0000' "inherited copy mode"
$OUT send q || exit 1
sleep 1
check_style 4 "inherited copy mode exit"
check_colour '#ff0000' "inherited copy mode exit"

$IN set -gw copy-mode-cursor-colour '#00ff00' || exit 1
$IN set -gw copy-mode-cursor-style block || exit 1
$OUT send C-b '[' || exit 1
sleep 1
check_style 2 "configured copy mode"
check_colour '#00ff00' "configured copy mode"
$IN set -gw copy-mode-cursor-colour '#0000ff' || exit 1
$IN set -gw copy-mode-cursor-style blinking-underline || exit 1
sleep 1
check_style 3 "updated copy mode"
check_colour '#0000ff' "updated copy mode"
$OUT send q || exit 1
sleep 1
check_style 4 "configured copy mode exit"
check_colour '#ff0000' "configured copy mode exit"

exit 0
