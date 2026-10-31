#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -LtestA$$"
stop_server $TMUX || exit 1
trap 'stop_server $TMUX' 0
trap 'exit 1' 1 2 15

$TMUX new -d -x40 -y10 \
      "cat copy-mode-test.txt; printf '\e[9;15H\033]2;ready\007'; cat" || exit 1
wait_for_output ready $TMUX display -p '#{pane_title}' || exit 1
$TMUX set -g window-size manual || exit 1

# Enter copy mode and go to the first column of the first row.
$TMUX set-window-option -g mode-keys vi
$TMUX copy-mode
$TMUX send-keys -X history-top
$TMUX send-keys -X start-of-line

# Test that `previous-word` and `previous-space`
# do not go past the start of text.
$TMUX send-keys -X begin-selection
$TMUX send-keys -X previous-word
$TMUX send-keys -X previous-space
$TMUX send-keys -X previous-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "A" ] || exit 1

# Test that `next-word-end` skips single-letter words
# and `previous-word` does not skip multi-letter words.
$TMUX send-keys -X next-word-end
$TMUX send-keys -X begin-selection
$TMUX send-keys -X previous-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "line" ] || exit 1

# Test that `next-word-end` stops at the end of the line.
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word-end
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "words" ] || exit 1

# Move to the next word for the following tests.
$TMUX send-keys -X next-word

# Test that `next-word` wraps around un-indented line breaks.
$TMUX send-keys -X next-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "line\nA")" ] || exit 1

# Test that `next-word-end` does not treat periods as letters.
$TMUX send-keys -X next-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "line" ] || exit 1

# Test that `next-space-end` treats periods as letters.
$TMUX send-keys -X previous-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-space-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "line..." ] || exit 1

# Test that `previous-space` and `next-space` treat periods as letters.
$TMUX send-keys -X previous-space
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-space
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "line...\n.")" ] || exit 1

# Test that `next-word` and `next-word-end` do not treat other symbols as letters.
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word-end
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "... @nd then" ] || exit 1

# Test that `next-space` wraps around for indented symbols
$TMUX send-keys -X next-space
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-space
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "\$ym_bols[]{}\n ?")" ] || exit 1

# Test that `next-word-end` treats digits as letters
$TMUX send-keys -X next-word-end
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "? 500xyz" ] || exit 1

# Test that `previous-word` treats digits as letters
$TMUX send-keys -X begin-selection
$TMUX send-keys -X previous-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "500xyz" ] || exit 1

# Test that `next-word`, `next-word-end`,
# `next-space`, and `next-space-end` stop at the end of text.
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word-end
$TMUX send-keys -X next-word
$TMUX send-keys -X next-space
$TMUX send-keys -X next-space-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "500xyz" ] || exit 1

# Test that vi cursor movement does not stop on the padding cell of a wide
# character at the end of a line.
stop_server $TMUX || exit 1
$TMUX new -d -x20 -y5 \
      "printf 'abc中\nxyz\n\033]2;ready\007'; exec cat" || exit 1
wait_for_output ready $TMUX display -p '#{pane_title}' || exit 1
$TMUX set-window-option -g mode-keys vi
$TMUX copy-mode
$TMUX send-keys -X history-top
$TMUX send-keys -X start-of-line
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
$TMUX send-keys -X cursor-right
[ "$($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')" = "3,0" ] ||
    exit 1
$TMUX send-keys -X cursor-right
[ "$($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')" = "0,1" ] ||
    exit 1
$TMUX send-keys -X cursor-left
[ "$($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')" = "3,0" ] ||
    exit 1
$TMUX send-keys -X cursor-left
[ "$($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')" = "2,0" ] ||
    exit 1

stop_server $TMUX || exit 1
exit 0
