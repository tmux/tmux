#!/bin/sh

# Observe a nested client's cursor colour in the outer pane, and its cursor
# style escape sequences through a pipe. Pane prompts must use their own
# cursor settings and restore the pane cursor when they close.

PATH=/bin:/usr/bin
TERM=tmux-256color
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT="$TEST_TMUX -LcursorA$$ -f/dev/null"
IN="$TEST_TMUX -LcursorB$$ -f/dev/null"
log=$(mktemp) || exit 1
trap '$OUT kill-server 2>/dev/null; $IN kill-server 2>/dev/null; rm -f "$log"' EXIT

fail() {
	echo "[FAIL] $*"
	exit 1
}

check() {
	sleep 1
	actual=$($OUT display -p '#{cursor_colour}')
	[ "$actual" = "$1" ] || fail "$2: cursor colour $actual, expected $1"
	if [ -n "$3" ]; then
		tail -c +"$((offset + 1))" "$log" |
		    grep -qF "$(printf '\033[%s q' "$3")" ||
		    fail "$2: expected cursor style $3"
	fi
}

$IN new -d -x80 -y23 'exec sleep 1000' || exit 1
$IN set -s escape-time 10 || exit 1
$IN set -g mode-keys vi || exit 1
$IN set -g status-keys vi || exit 1
$IN set -g prompt-cursor-colour '#ff0000' || exit 1
$IN set -g prompt-command-cursor-colour '#00ff00' || exit 1
$IN set -g prompt-cursor-style bar || exit 1
$IN set -g prompt-command-cursor-style block || exit 1
$OUT new -d -x80 -y24 "$IN attach" || exit 1
$OUT set -g status off || exit 1
sleep 1
$OUT pipe-pane -O "cat > '$log'" || exit 1

$OUT send C-b : || exit 1
check '#ff0000' 'status prompt'
$OUT send C-c || exit 1
sleep 1
$OUT send C-b '[' || exit 1
sleep 1
$OUT send / || exit 1
check '#ff0000' 'copy-mode search prompt'
$OUT send -l abc || exit 1
sleep 1
offset=$(wc -c < "$log")
$OUT send Escape || exit 1
check '#00ff00' 'copy-mode search command mode' 2
offset=$(wc -c < "$log")
$OUT send i || exit 1
check '#ff0000' 'copy-mode search insert mode' 6
$OUT send C-c || exit 1
check none 'copy-mode prompt closed'

exit 0
