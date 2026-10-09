#!/bin/sh

# Erasing a line preserves Kitty graphics; erasing the screen removes them.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lkitty-clear-inner$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lkitty-clear-outer$$ -f/dev/null"
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX new-session -d -x 10 -y 4 "
	printf '\033_Ga=T,q=2,f=24,s=1,v=1,c=1,r=1,C=1;////\033\\'
	read line
	printf '\033[H\033[2K'
	read line
	printf '\033[H\033[2J'
	sleep 30" || exit 1
[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0
$TMUX set -g status off || exit 1
$TMUX set -g scroll-on-clear off || exit 1
$TMUX set -as terminal-features ',*:sixel@' || exit 1
$TMUX set -as terminal-features ',*:kitty@' || exit 1
$TMUX set -as terminal-features ',*:RGB' || exit 1
$TMUX2 new-session -d -x 10 -y 4 "$TMUX attach-session" || exit 1
$TMUX2 set -g status off || exit 1
sleep 1
$TMUX2 capture-pane -peS0 -E0 | grep -q '48;2;255;255;255m' || exit 1

$TMUX send-keys Enter || exit 1
sleep 1
$TMUX2 capture-pane -peS0 -E0 | grep -q '48;2;255;255;255m' || exit 1

$TMUX send-keys Enter || exit 1
sleep 1
[ -z "$($TMUX2 capture-pane -peS0 -E3)" ] || exit 1

exit 0
