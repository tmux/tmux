#!/bin/sh

# Reusing a session name must not reuse an unrelated session group.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0 1 15

$TMUX new -d -s a || exit 1
$TMUX new -d -t a -s b || exit 1
$TMUX kill-session -t a || exit 1

# Occupy the first suffix too, without creating a session of that name.
$TMUX new -d -t a-1 -s other || exit 1
$TMUX new -d -s a || exit 1
AW=$($TMUX display -p -t a: '#{window_id}')
BW=$($TMUX display -p -t b: '#{window_id}')
$TMUX new -d -t a -s c || exit 1

[ "$($TMUX display -p -t a: '#{session_group}')" = a-2 ] || exit 1
[ "$($TMUX display -p -t c: '#{session_group}')" = a-2 ] || exit 1
[ "$($TMUX display -p -t b: '#{session_group_list}')" = b ] || exit 1
[ "$($TMUX display -p -t a: '#{window_id}')" = "$AW" ] || exit 1
[ "$($TMUX display -p -t c: '#{window_id}')" = "$AW" ] || exit 1
[ "$AW" != "$BW" ] || exit 1

# Window changes propagate only within the new group.
$TMUX new-window -d -t a || exit 1
[ "$($TMUX display -p -t c: '#{session_windows}')" = 2 ] || exit 1
[ "$($TMUX display -p -t b: '#{session_windows}')" = 1 ] || exit 1

# An existing session still joins its group; a group-only target still works.
$TMUX new -d -t c -s d || exit 1
[ "$($TMUX display -p -t d: '#{session_group}')" = a-2 ] || exit 1
$TMUX new -d -t a-1 -s e || exit 1
[ "$($TMUX display -p -t e: '#{session_group}')" = a-1 ] || exit 1
[ "$($TMUX display -p -t e: '#{window_id}')" = \
    "$($TMUX display -p -t other: '#{window_id}')" ] || exit 1

exit 0
