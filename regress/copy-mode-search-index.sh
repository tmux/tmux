#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d -x40 -y10 \
      "printf 'zero\nmatch one\nmiddle\nmatch two\nmatch three\n'; cat" ||
	exit 1
sleep 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g wrap-search on || exit 1
$TMUX set-window-option -g mode-keys vi || exit 1
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -X search-forward-text match || exit 1

index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "1/3" ] || exit 1

$TMUX send-keys -X search-again || exit 1
index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "2/3" ] || exit 1

$TMUX send-keys -X cursor-right || exit 1
index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "/3" ] || exit 1
$TMUX send-keys -X search-again || exit 1
index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "3/3" ] || exit 1

$TMUX send-keys -X search-reverse || exit 1
index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "2/3" ] || exit 1

$TMUX send-keys -X search-reverse || exit 1
index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "1/3" ] || exit 1

$TMUX send-keys -X clear-selection || exit 1
index=$($TMUX display-message -p '#{search_index}/#{search_count}')
[ "$index" = "/" ] || exit 1

$TMUX kill-server 2>/dev/null
exit 0
