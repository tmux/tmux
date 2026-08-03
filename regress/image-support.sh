#!/bin/sh

# Grid-resident image markers: Kitty input, history, copy and overwrite.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Limage$$ -f/dev/null"
TMUX2="$TEST_TMUX -Limage-client$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
FIXTURE=$(pwd)/monkey-2.sixel.txt
trap "$TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null; rm -f $TMP" 0 1 15

$TMUX new-session -d -x 20 -y 8 "
	printf '\033_Ga=T,q=2,f=32,s=2,v=2,c=2,r=2,m=1;/wAA/wD/\033\\'
	printf '\033_Gm=0;AP8AAP///////w==\033\\'
	i=0
	while [ \$i -lt 10 ]; do printf '\nline-%d' \$i; i=\$((i + 1)); done
	sleep 10"

sleep 1
[ "$($TMUX display-message -p '#{image_support}')" = 0 ] && exit 0

# Images scroll as grid cells, while capture output contains ordinary spaces.
[ "$($TMUX display-message -p '#{history_size}')" -gt 0 ] || exit 1
$TMUX capture-pane -pS- >$TMP || exit 1
grep '_G' $TMP >/dev/null && exit 1

# Copy mode duplicates the backing grid, including image marker references.
$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX capture-pane -p >$TMP || exit 1
$TMUX send-keys -X cancel || exit 1

# Ordinary text overwrites marker cells through the normal grid write path.
$TMUX new-window -d "
	printf '\033_Ga=T,q=2,f=32,s=2,v=2,c=2,r=2;/wAA/wD/AP8AAP///////w==\033\\'
	printf '\033[HXY'
	sleep 10"
sleep 1
[ "$($TMUX capture-pane -pt:1 -S0 -E0)" = "XY" ] || exit 1

# The protocol default is transmit-only; a later placement uses the retained
# immutable image and advances by the same canonical one-cell footprint.
$TMUX new-window -d "
	printf '\033_Gq=2,f=32,s=1,v=1,c=1,r=1,i=9;/wAA/w==\033\\'
	printf '\033_Ga=p,q=2,i=9\033\\'
	printf '\033_Ga=d,d=I,q=2,i=9\033\\'
	sleep 10"
sleep 1
[ "$($TMUX display-message -pt:2 '#{cursor_y}')" = 1 ] || exit 1

# PNG Kitty input uses the shared image decoder and canonical cell sizing.
$TMUX new-window -d "
	printf '\033_Ga=T,q=2,f=100;iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAIGNIUk0AAHomAACAhAAA+gAAAIDoAAB1MAAA6mAAADqYAAAXcJy6UTwAAAAGUExURf8AAP///0EdNBEAAAABYktHRAH/Ai3eAAAAB3RJTUUH6ggCDAECH324BwAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=\033\\'
	sleep 10"
sleep 1
[ "$($TMUX display-message -pt:3 '#{cursor_y}')" = 1 ] || exit 1

# SIXEL input reaches the same grid marker and copy-mode paths.
$TMUX new-window -d "cat '$FIXTURE'; sleep 10"
sleep 1
[ "$($TMUX display-message -pt:4 '#{cursor_y}')" -gt 0 ] || exit 1
$TMUX copy-mode -t:4 || exit 1
$TMUX send-keys -t:4 -X history-top || exit 1
$TMUX capture-pane -pt:4 >$TMP || exit 1

# A client without an image feature gets the brightness-based ASCII backend.
$TMUX kill-server 2>/dev/null
$TMUX2 new-session -d -x 10 -y 4 "
	printf '\033_Ga=T,q=2,f=32,s=2,v=2,c=2,r=2;/wAA/wD/AP8AAP///////w==\033\\'
	sleep 10" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX new-session -d -x 10 -y 4 || exit 1
$TMUX set -g status off || exit 1
$TMUX send-keys -l "$TMUX2 attach-session" || exit 1
$TMUX send-keys Enter || exit 1
sleep 1
$TMUX capture-pane -pS0 -E1 >$TMP || exit 1
[ "$(sed -n 1p $TMP)" = ".*" ] || exit 1
[ "$(sed -n 2p $TMP)" = " @" ] || exit 1

# A selection redraw uses the single-cell path. It must leave ASCII image
# cells visible (graphical clients skip these cells to avoid erasing pixels).
$TMUX2 copy-mode || exit 1
$TMUX2 send-keys -X history-top || exit 1
$TMUX2 send-keys -X start-of-line || exit 1
$TMUX2 send-keys -X begin-selection || exit 1
$TMUX2 send-keys -X cursor-right || exit 1
sleep 1
$TMUX capture-pane -pS0 -E1 >$TMP || exit 1
sed -n 1p $TMP | grep -q '^\.\*' || exit 1
sed -n 2p $TMP | grep -q '^ @' || exit 1

exit 0
