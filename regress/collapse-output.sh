#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

$TMUX kill-server 2>/dev/null
$TMUX new-session -d -x80 -y20 "sh -c 'printf \"\\033]133;A\\007p\\$ \\033]133;B\\007echo hi\\n\\033]133;C\\007hello\\n\\033]133;D;0\\007separator\\n\\033]133;A\\007p\\$ \\033]133;B\\007\"; exec sleep 100'" || exit 1
sleep 1

$TMUX copy-mode -c || exit 1
$TMUX send-keys -X search-backward hello || exit 1
hidden=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$hidden" != hello ] || exit 1
$TMUX send-keys -X search-backward separator || exit 1
hidden=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$hidden" != separator ] || exit 1

$TMUX send-keys BTab || exit 1
$TMUX send-keys -X search-backward hello || exit 1
shown=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$shown" = hello ] || exit 1
$TMUX send-keys -X select-line || exit 1
$TMUX send-keys -X copy-selection || exit 1
selected=$($TMUX show-buffer)
[ "$selected" = hello ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX kill-server
$TMUX new-session -d -x80 -y20 "sh -c 'printf \"first\\n\\033]133;D;0\\007separator\\n\\033]133;A\\007p\\$ \\033]133;B\\007\"; exec sleep 100'" || exit 1
sleep 1

$TMUX copy-mode -c || exit 1
$TMUX send-keys -X search-backward separator || exit 1
hidden=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$hidden" != separator ] || exit 1
$TMUX send-keys BTab || exit 1
$TMUX send-keys -X search-backward separator || exit 1
shown=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$shown" = separator ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX kill-server
$TMUX new-session -d -x80 -y20 "sh -c 'printf \"\\033]133;A\\007P> \\033]133;B\\007for i in 1 2 3; do\\n\\033]133;A;k=s\\007> \\033]133;B\\007echo \\$i\\n\\033]133;P;k=s\\007> \\033]133;B\\007done\\033]133;C\\007\\nRESULT\\n\\033]133;D;0\\007separator\\n\\033]133;A\\007P> \\033]133;B\\007\"; exec sleep 100'" || exit 1
sleep 1

$TMUX copy-mode -c || exit 1
$TMUX send-keys -X search-backward RESULT || exit 1
hidden=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$hidden" != RESULT ] || exit 1
$TMUX send-keys -X search-backward 'echo $i' || exit 1
shown=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$shown" = 'echo $i' ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX copy-mode -U || exit 1
$TMUX send-keys -X search-backward RESULT || exit 1
$TMUX send-keys Tab || exit 1
cursor=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$cursor" = done ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX kill-server
$TMUX new-session -d -x80 -y20 "sh -c 'printf \"\\033]133;A\\007P0> \\033]133;B\\007\\r\\n\\033]133;D;0\\007empty-separator\\n\\033]133;A\\007P1> \\033]133;B\\007\"; exec sleep 100'" || exit 1
sleep 1

$TMUX copy-mode -c || exit 1
$TMUX send-keys -X search-backward empty-separator || exit 1
hidden=$($TMUX display-message -p '#{copy_cursor_line}')
[ "$hidden" != empty-separator ] || exit 1
$TMUX send-keys -X cancel || exit 1

$TMUX kill-server 2>/dev/null
exit 0
