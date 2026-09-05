#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -L${TEST_SOCKET:-testO$$} -f/dev/null"
OUT=/tmp/tmux-output-commands-$$
EXPECTED=$(printf 'one\ntwo')

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -f "$OUT"
}
trap cleanup EXIT HUP INT TERM

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -x80 -y20 "sh -c 'printf \"\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\ntwo\\n\\033]133;D;0\\007separator\\n\\033]133;A\\007p\\$ \\033]133;B\\007broken\\n\\033]133;C\\007unfinished\"; exec sleep 100'" || exit 1
sleep 1

$TMUX copy-output || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1

$TMUX pipe-output "wc -c >$OUT" || exit 1
sleep 1
[ "$(cat "$OUT")" = 10 ] || exit 1

$TMUX copy-pipe-output "wc -c >$OUT" output || exit 1
sleep 1
[ "$(cat "$OUT")" = 10 ] || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1

$TMUX select-output || exit 1
[ "$($TMUX display-message -p '#{pane_in_mode}')" = 1 ] || exit 1
$TMUX send-keys -X copy-selection || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode -U || exit 1
$TMUX send-keys -X search-backward one || exit 1
$TMUX send-keys -X copy-output || exit 1
copied=$($TMUX show-buffer)
[ "$copied" = "$EXPECTED" ] || exit 1

$TMUX send-keys -X pipe-output "wc -l >$OUT" || exit 1
sleep 1
[ "$(cat "$OUT")" = 2 ] || exit 1

$TMUX send-keys -X copy-pipe-output "wc -l >$OUT" output || exit 1
sleep 1
[ "$(cat "$OUT")" = 2 ] || exit 1
copied=$($TMUX show-buffer)
[ "$copied" = "$EXPECTED" ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode -c || exit 1
$TMUX send-keys -X search-backward echo || exit 1
$TMUX send-keys -X select-output || exit 1
$TMUX send-keys -X copy-selection || exit 1
selected=$($TMUX show-buffer)
[ "$selected" = "$EXPECTED" ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX set-buffer -b keep unchanged || exit 1
$TMUX copy-mode -U || exit 1
$TMUX send-keys -X search-backward unfinished || exit 1
$TMUX send-keys -X copy-output || exit 1
[ "$($TMUX show-buffer)" = unfinished ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX new-window -d -n plain "printf 'alpha\\nbeta\\n'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-output -a -t :plain || exit 1
all=$($TMUX show-buffer)
case "$all" in
*alpha*beta*) ;;
*) exit 1 ;;
esac

$TMUX set-buffer -b keep unchanged || exit 1
$TMUX copy-output -t :plain || exit 1
[ "$($TMUX show-buffer -b keep)" = unchanged ] || exit 1

$TMUX select-output -t :plain || exit 1
[ "$($TMUX display-message -p -t :plain.0 '#{pane_in_mode}')" = 0 ] || exit 1
$TMUX select-output -a -t :plain || exit 1
[ "$($TMUX display-message -p -t :plain.0 '#{pane_in_mode}')" = 1 ] || exit 1
$TMUX send-keys -t :plain.0 -X copy-selection || exit 1
all=$($TMUX show-buffer)
case "$all" in
*alpha*beta*) ;;
*) exit 1 ;;
esac
$TMUX send-keys -t :plain.0 -X cancel || exit 1

$TMUX new-window -d -n empty "printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\033]133;C\\007one\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007true\\033]133;C\\033]133;D;0\\007'; exec sleep 100" || exit 1
sleep 1
$TMUX copy-output -t :empty || exit 1
[ "$($TMUX show-buffer)" = one ] || exit 1

$TMUX copy-mode -t :plain || exit 1
$TMUX send-keys -t :plain.0 -X copy-output -a || exit 1
all=$($TMUX show-buffer)
case "$all" in
*alpha*beta*) ;;
*) exit 1 ;;
esac
$TMUX send-keys -t :plain.0 -X cancel || exit 1

exit 0
