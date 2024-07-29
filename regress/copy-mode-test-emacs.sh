#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"
$TMUX kill-server 2>/dev/null

$TMUX new -d -x40 -y10 \
      "cat copy-mode-test.txt; printf '\e[9;15H'; cat" || exit 1
$TMUX set -g window-size manual || exit 1

# Enter copy mode and go to the first column of the first row.
$TMUX set-window-option -g mode-keys emacs
$TMUX set-window-option -g word-separators ""
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
[ "$($TMUX show-buffer 2>/dev/null)" = "" ] || exit 1

# Test that `next-word-end` does not skip single-letter words.
$TMUX send-keys -X next-word-end
$TMUX send-keys -X begin-selection
$TMUX send-keys -X previous-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "A" ] || exit 1

# Test that `next-word-end` wraps around indented line breaks.
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word-end
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "words\n        Indented")" ] || exit 1

# Test that `next-word` wraps around un-indented line breaks.
$TMUX send-keys -X next-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "line\n")" ] || exit 1

# Test that `next-word-end` treats periods as letters.
$TMUX send-keys -X next-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "line..." ] || exit 1

# Test that `previous-word` and `next-word` treat periods as letters.
$TMUX send-keys -X previous-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "line...\n")" ] || exit 1

# Test that `previous-space` and `next-space` treat periods as letters.
$TMUX send-keys -X previous-space
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-space
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "line...\n")" ] || exit 1

# Test that `next-word` and `next-word-end` treat other symbols as letters.
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word-end
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "... @nd then \$ym_bols[]{}" ] || exit 1

# Test that `previous-word` treats other symbols as letters
# and `next-word` wraps around for indented symbols
$TMUX send-keys -X previous-word
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "$(printf "\$ym_bols[]{}\n ")" ] || exit 1

# Test that `next-word-end` treats digits as letters
$TMUX send-keys -X next-word-end
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = " 500xyz" ] || exit 1

# Test that `previous-word` treats digits as letters
$TMUX send-keys -X begin-selection
$TMUX send-keys -X previous-word
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "500xyz" ] || exit 1

# Test that `next-word` and `next-word-end` stop at the end of text.
$TMUX send-keys -X begin-selection
$TMUX send-keys -X next-word
$TMUX send-keys -X next-word-end
$TMUX send-keys -X next-word
$TMUX send-keys -X next-space
$TMUX send-keys -X next-space-end
$TMUX send-keys -X copy-selection
[ "$($TMUX show-buffer)" = "500xyz" ] || exit 1

$TMUX kill-server 2>/dev/null
exit 0
