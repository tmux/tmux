#!/bin/sh

# Grid-resident SIXEL image markers: history, copy, overwrite and fallback.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Limage$$ -f/dev/null"
TMUX2="$TEST_TMUX -Limage-client$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null; rm -f $TMP" 0 1 15

$TMUX new-session -d -x 20 -y 8 "
	printf '\033Pq\"1;1;8;16#0;2;100;100;100#0~~~~~~~~\044-~~~~~~~~\033\\'
	i=0
	while [ \$i -lt 10 ]; do printf '\nline-%d' \$i; i=\$((i + 1)); done
	sleep 10"

sleep 1
[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0

# Images scroll as grid cells, while capture output contains ordinary spaces.
[ "$($TMUX display-message -p '#{history_size}')" -gt 0 ] || exit 1
$TMUX capture-pane -pS- >$TMP || exit 1
grep 'Pq' $TMP >/dev/null && exit 1

# Copy mode duplicates the backing grid, including image marker references.
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX capture-pane -p >$TMP || exit 1
$TMUX send-keys -X cancel || exit 1

# Ordinary text overwrites marker cells through the normal grid write path.
$TMUX new-window -d "
	printf '\033Pq\"1;1;8;16#0;2;100;100;100#0~~~~~~~~\044-~~~~~~~~\033\\'
	printf '\033[HXY'
	sleep 10"
sleep 1
[ "$($TMUX capture-pane -pt:1 -S0 -E0)" = "XY" ] || exit 1

# A client without an image feature gets the brightness-based text backend.
$TMUX kill-server 2>/dev/null
$TMUX2 new-session -d -x 10 -y 4 "
	printf '\033Pq\"1;1;8;16#0;2;100;100;100#0~~~~~~~~\044-~~~~~~~~\033\\'
	sleep 10" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX new-session -d -x 10 -y 4 || exit 1
$TMUX set -g status off || exit 1
$TMUX send-keys -l "$TMUX2 attach-session" || exit 1
$TMUX send-keys Enter || exit 1
sleep 1
$TMUX capture-pane -pS0 -E0 >$TMP || exit 1
grep -q '[.:-=+*#%@]' $TMP || exit 1

# Selection redraws must leave text image cells visible.
$TMUX2 copy-mode || exit 1
$TMUX2 send-keys -X history-top || exit 1
$TMUX2 send-keys -X start-of-line || exit 1
$TMUX2 send-keys -X begin-selection || exit 1
sleep 1
$TMUX capture-pane -pS0 -E0 >$TMP || exit 1
grep -q '[.:-=+*#%@]' $TMP || exit 1

exit 0
