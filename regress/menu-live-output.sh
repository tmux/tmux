#!/bin/sh

# check the actual terminal image while a pane writes underneath a menu.
# an outer tmux captures the display of an attached inner tmux client.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
TMP=$(mktemp -d)
MENU_ROW=4

cleanup() {
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup 0 1 15

fail() {
	echo "$*" >&2
	exit 1
}

capture_menu() {
	$TMUX capturep -p -S"$MENU_ROW" -E"$((MENU_ROW + 3))" >"$TMP/rows" || exit 1
	cut -c7-19 "$TMP/rows" >"$1" || exit 1
}

check_update() {
	printf '%b' "$2" >&3
	printf '\033[1;1H\033[2K%s' "$1" >&3
	sleep 1
	$TMUX capturep -p >"$TMP/screen" || exit 1
	grep -q "^$1$" "$TMP/screen" || fail "$1: pane stopped updating"
	capture_menu "$TMP/actual"
	cmp -s "$TMP/expected" "$TMP/actual" ||
		fail "$1: pane output overwrote the menu"
}

mkfifo "$TMP/output" || exit 1
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

# simple borders keep the captured rectangle single-byte and portable.
$TMUX2 display-menu -b simple -T live -C 0 -x6 -y8 \
	alpha a "" beta b "" || exit 1
sleep 1
capture_menu "$TMP/expected"
grep -q 'alpha' "$TMP/expected" || fail "menu did not appear"

check_update text '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789'
check_update clear-line '\033[6;1H\033[2K'
check_update clear-screen '\033[2J'
check_update scroll '\033[14;1Hone\r\ntwo\r\nthree\r\nfour\r\n'
check_update insert-character '\033[6;1H\033[3@'
check_update delete-character '\033[6;1H\033[3P'
check_update insert-line '\033[5;1H\033[2L'
check_update delete-line '\033[5;1H\033[2M'
check_update wide-text '\033[6;6H\347\225\214\347\225\214\347\225\214'
check_update insert-mode '\033[6;1H\033[4habc\033[4l'

# a clipped client viewport must not fall back to drawing over the menu.
$TMUX2 resizew -x60 -y14 || exit 1
sleep 1
check_update viewport '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwx'
$TMUX2 resizew -x40 -y14 || exit 1

# menu clipping uses window coordinates even with a status line above it.
$TMUX2 set -g status on || exit 1
$TMUX2 set -g status-position top || exit 1
MENU_ROW=5
sleep 1
capture_menu "$TMP/expected"
grep -q 'alpha' "$TMP/expected" || fail "menu missing with top status"
check_update top-status '\033[6;1Habcdefghijklmnopqrstuvwxyz0123456789\033[2K'

# nonvisual terminal output must still be delivered while the menu is open.
printf '\033]52;c;bWVudS1jbGlwYm9hcmQ=\007' >&3
sleep 1
[ "$($TMUX show-buffer 2>/dev/null)" = menu-clipboard ] ||
	fail "menu blocked the clipboard update"

# closing the menu must reveal the latest contents of the pane beneath it.
printf '\033[2J\033[6;7Hlatest content' >&3
sleep 1
$TMUX send Escape || exit 1
sleep 1
$TMUX capturep -p >"$TMP/screen" || exit 1
grep -q 'latest content' "$TMP/screen" || fail "menu close left stale content"

exit 0
