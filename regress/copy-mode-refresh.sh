#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

# A pane that prints a fresh numbered line several times a second. The leading
# newline leaves the cursor on the printed line, so copy_cursor_line tracks it.
$TMUX new -d -x40 -y10 \
    'i=0; while :; do i=$((i+1)); printf "\nline-%d" "$i"; sleep 0.1; done' || exit 1
$TMUX set -g window-size manual || exit 1
sleep 1

# Enter copy mode at the bottom; auto-refresh is off by default.
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-bottom

# Off by default: the view holds still even as output streams.
a=$($TMUX display -p '#{copy_cursor_line}')
sleep 1
b=$($TMUX display -p '#{copy_cursor_line}')
[ "$a" = "$b" ] || exit 1

# refresh-toggle (r) turns it on: the cursor line now advances.
$TMUX send-keys r
c=$($TMUX display -p '#{copy_cursor_line}')
sleep 1
d=$($TMUX display -p '#{copy_cursor_line}')
[ "$c" != "$d" ] || exit 1

# refresh-toggle again turns it off: the view holds still once more.
$TMUX send-keys r
e=$($TMUX display -p '#{copy_cursor_line}')
sleep 1
f=$($TMUX display -p '#{copy_cursor_line}')
[ "$e" = "$f" ] || exit 1

$TMUX kill-server 2>/dev/null

exit 0
