#!/bin/sh

# Inspect an attached client's terminal while panes write behind a menu.
# Incremental output must agree with the scene renderer, including floating
# panes and a client viewport smaller than the window.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
TMP=$(mktemp -d)

cleanup() {
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -f "$TMP/output" "$TMP/float" "$TMP/actual" "$TMP/expected"
	rmdir "$TMP"
}
trap cleanup 0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

# A full scene redraw is the reference for each incremental update. Check an
# uncovered marker before refreshing, so a frozen pane cannot pass the test.
check_update() {
	printf '%b' "$2" >&3
	printf '\033[1;1H\033[2K%s' "$1" >&3
	sleep 1
	$TMUX capturep -p >"$TMP/actual" || exit 1
	grep -q "^$1" "$TMP/actual" || fail "$1: pane stopped updating"
	$TMUX capturep -pe >"$TMP/actual" || exit 1
	$TMUX2 refresh-client || exit 1
	sleep 1
	$TMUX capturep -pe >"$TMP/expected" || exit 1
	if ! cmp -s "$TMP/actual" "$TMP/expected"; then
		diff -u "$TMP/expected" "$TMP/actual" >&2
		fail "$1: incremental output differs from scene redraw"
	fi
}

menu() {
	$TMUX2 display-menu -b simple -T live -C 0 "$@" \
	    alpha a "" beta b "" || exit 1
	sleep 1
	$TMUX capturep -p >"$TMP/actual" || exit 1
	grep -q alpha "$TMP/actual" || fail "menu did not appear"
}

mkfifo "$TMP/output" "$TMP/float" || exit 1
$TMUX2 new -d -x40 -y14 "exec cat '$TMP/output'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1
$TMUX2 resizew -x40 -y14 || exit 1
$TMUX2 set -as terminal-features ',screen:clipboard' || exit 1
$TMUX2 set -g set-clipboard on || exit 1
exec 3>"$TMP/output"

$TMUX new -d -x40 -y14 "$TMUX2 attach" || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX resizew -x40 -y14 || exit 1
$TMUX set -g set-clipboard on || exit 1
sleep 1

menu -x6 -y8
check_update text '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789'
check_update clear-line '\033[6;1H\033[2K'
check_update clear-end-line '\033[6;1H\033[K'
check_update clear-start-line '\033[6;40H\033[1K'
check_update erase-character '\033[6;1H\033[40X'
check_update clear-screen '\033[2J'
check_update clear-end-screen '\033[3;1H\033[J'
check_update clear-start-screen '\033[10;40H\033[1J'
check_update scroll '\033[14;1Hone\r\ntwo\r\nthree\r\nfour\r\n'
check_update reverse-scroll '\033[1;1H\033M'
check_update insert-character '\033[6;1Habcdefghijklmnopqrstuvwxyz\033[6;1H\033[3@'
check_update delete-character '\033[6;1Habcdefghijklmnopqrstuvwxyz\033[6;1H\033[3P'
check_update insert-line '\033[5;1H\033[2L'
check_update delete-line '\033[5;1H\033[2M'
check_update wide-text '\033[6;6H\347\225\214\347\225\214\347\225\214'
check_update overwrite-wide '\033[6;5H\347\225\214\033[6;6Hx'
check_update insert-mode '\033[6;1H\033[4habc\033[4l'
check_update sync '\033[?2026h\033[6;1Habcdefghijklmnopqrstuvwxyz\033[?2026l'
check_update sync-scroll '\033[?2026h\033[14;1Hone\r\ntwo\r\nthree\r\n\033[?2026l'

# Text on the right of the menu crosses the client viewport edge. A terminal
# fallback which redraws the entire pane line would overwrite the menu.
$TMUX2 resizew -x60 -y14 || exit 1
sleep 1
check_update viewport '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwx'
$TMUX2 resizew -x40 -y14 || exit 1

# Window coordinates must still be used when the status line is above it.
$TMUX2 set -g status on || exit 1
$TMUX2 set -g status-position top || exit 1
$TMUX2 set -g status-format[0] '' || exit 1
$TMUX2 resizew -y13 || exit 1
sleep 1
check_update top-status '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789'
$TMUX2 set -g status off || exit 1
$TMUX2 resizew -y14 || exit 1

# Both the menu and floating panes obscure the base pane. The menu also
# obscures incremental output from the floating pane itself.
FLOAT=$($TMUX2 new-pane -dPF '#{pane_id}' -x24 -y8 -X4 -Y2 \
    "exec cat '$TMP/float'") || exit 1
exec 4>"$TMP/float"
sleep 1
check_update behind-float '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789'
printf '\033[4;1HABCDEFGHIJKLMNOPQRSTUVWX' >&4
check_update float-text ''
printf '\033[4;1H\033[4hxyz\033[4l' >&4
check_update float-insert ''
printf '\033[8;1Hone\r\ntwo\r\nthree\r\n' >&4
check_update float-scroll ''
$TMUX2 kill-pane -t "$FLOAT" || exit 1

# Without a menu, the same whole-line and insert-mode paths must continue to
# respect floating panes. This also exercises split visible ranges.
$TMUX send Escape || exit 1
FLOAT=$($TMUX2 new-pane -dPF '#{pane_id}' -x12 -y6 -X6 -Y3 \
    'exec sleep 100') || exit 1
$TMUX2 resizew -x60 -y14 || exit 1
sleep 1
check_update float-viewport '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwx'
check_update float-insert-mode '\033[6;1H\033[4habc\033[4l'
$TMUX2 kill-pane -t "$FLOAT" || exit 1
$TMUX2 resizew -x40 -y14 || exit 1

# A menu covering an entire line leaves an empty visible range. Borderless
# menus and menus larger than the window must use their actual screen size.
menu -b none -x0 -y8
check_update borderless '\033[7;1Habcdefghijklmnopqrstuvwxyz0123456789'
$TMUX send Escape || exit 1
$TMUX2 display-menu -b simple -T live -C 0 -x0 -y8 \
    'This menu is wider than the whole window' a '' beta b '' || exit 1
sleep 1
check_update full-width '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789'

# Nonvisual output is still delivered while a menu is open.
printf '\033]52;c;bWVudS1jbGlwYm9hcmQ=\007' >&3
sleep 1
[ "$($TMUX show-buffer 2>/dev/null)" = menu-clipboard ] ||
    fail "menu blocked the clipboard update"

# The pane grid must keep updating under the menu, both for capture-pane and
# for the redraw when the menu closes.
printf '\033[2J\033[6;7Hlatest content' >&3
sleep 1
$TMUX2 capturep -p -t%0 >"$TMP/actual" || exit 1
grep -q 'latest content' "$TMP/actual" || fail "underlying pane is stale"
$TMUX send Escape || exit 1
sleep 1
$TMUX capturep -p >"$TMP/actual" || exit 1
grep -q 'latest content' "$TMP/actual" || fail "menu close left stale content"

exit 0
