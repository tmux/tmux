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

# A transmitted Kitty image followed by a Unicode placeholder placement is
# converted into shared image cells rather than retained as placeholder text.
$TMUX new-window -d "
	printf '\033Ptmux;\033\033_Ga=t,q=2,f=32,o=z,m=1,s=2,v=1,i=588707642;eJz7z8Dw\033\033\\\033\\'
	printf '\033Ptmux;\033\033_Ga=t,q=2;HwQAFvEF+w\033\033\\\033\\'
	printf '\033Ptmux;\033\033_Ga=p,U=1,q=2,i=588707642,c=2,r=1\033\033\\\033\\'
	printf '\033[38;2;22;247;58m\364\216\273\256\314\205\314\205\326\222\364\216\273\256\033[39m'
	sleep 10"
sleep 1
[ -z "$($TMUX capture-pane -pt:3 -S0 -E0)" ] || exit 1

# PNG Kitty input uses the shared image decoder and canonical cell sizing.
$TMUX new-window -d "
	printf '\033_Ga=T,q=2,f=100;iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAIGNIUk0AAHomAACAhAAA+gAAAIDoAAB1MAAA6mAAADqYAAAXcJy6UTwAAAAGUExURf8AAP///0EdNBEAAAABYktHRAH/Ai3eAAAAB3RJTUUH6ggCDAECH324BwAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=\033\\'
	sleep 10"
sleep 1
[ "$($TMUX display-message -pt:4 '#{cursor_y}')" = 1 ] || exit 1

# SIXEL input reaches the same grid marker and copy-mode paths.
$TMUX new-window -d "cat '$FIXTURE'; sleep 10"
sleep 1
[ "$($TMUX display-message -pt:5 '#{cursor_y}')" -gt 0 ] || exit 1
$TMUX copy-mode -t:5 || exit 1
$TMUX send-keys -t:5 -X history-top || exit 1
$TMUX capture-pane -pt:5 >$TMP || exit 1

# A 26-pixel raster occupies two default 16-pixel cells, but the final cell
# remains only partially filled instead of stretching the raster to 32 pixels.
# A client without an image feature exposes this in the sampled text backend.
$TMUX kill-server 2>/dev/null
$TMUX2 new-session -d -x 10 -y 4 "
	printf '\033Pq\"1;1;26;26#0;2;100;100;100#0!26~-!26~-!26~-!26~-!26B\033\\'
	sleep 10" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX new-session -d -x 10 -y 4 || exit 1
$TMUX set -g status off || exit 1
$TMUX send-keys -l "$TMUX2 attach-session" || exit 1
$TMUX send-keys Enter || exit 1
sleep 1
$TMUX capture-pane -pS0 -E0 >$TMP || exit 1
grep -q '^#=' $TMP || exit 1

# A selection redraw uses the single-cell path. It must leave ASCII image
# cells visible (graphical clients skip these cells to avoid erasing pixels).
$TMUX2 copy-mode || exit 1
$TMUX2 send-keys -X history-top || exit 1
$TMUX2 send-keys -X start-of-line || exit 1
$TMUX2 send-keys -X begin-selection || exit 1
$TMUX2 send-keys -X cursor-right || exit 1
sleep 1
$TMUX capture-pane -pS0 -E0 >$TMP || exit 1
grep -q '^#=' $TMP || exit 1

# A retained Kitty image may be placed repeatedly using source rectangles.
# Crop the green and white right column from a red/green/blue/white image.
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
$TMUX2 new-session -d -x 10 -y 4 "
	printf '\033_Ga=t,q=2,f=32,s=2,v=2,i=10;/wAA/wD/AP8AAP///////w==\033\\'
	printf '\033_Ga=p,q=2,i=10,x=1,y=0,w=1,h=2,c=1,r=2\033\\'
	sleep 10" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX new-session -d -x 10 -y 4 || exit 1
$TMUX set -g status off || exit 1
$TMUX send-keys -l "$TMUX2 attach-session" || exit 1
$TMUX send-keys Enter || exit 1
sleep 1
$TMUX capture-pane -pS0 -E1 >$TMP || exit 1
[ "$(sed -n 1p $TMP)" = "*" ] || exit 1
[ "$(sed -n 2p $TMP)" = "@" ] || exit 1

# C=1 places the image without moving the cursor or scrolling. The four-row
# image is clipped at the bottom of this four-row pane, leaving its white last
# row off screen.
$TMUX2 new-window -d "
	printf '\033[2;3H'
	printf '\033_Ga=T,q=2,C=1,f=32,s=1,v=4,c=1,r=4;AAAA/1VVVf+qqqr//////w==\033\\'
	sleep 10" || exit 1
$TMUX2 select-window -t:1 || exit 1
sleep 1
[ "$($TMUX2 display-message -p '#{cursor_x},#{cursor_y}')" = "2,1" ] || exit 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ "$(sed -n 3p $TMP)" = "  -" ] || exit 1
[ "$(sed -n 4p $TMP)" = "  *" ] || exit 1

# With normal cursor movement, scrolling is calculated from the full image
# height. Rows which scrolled above the pane are then cropped from the top, so
# the bottom three source rows remain visible and the cursor is on the last row.
$TMUX2 new-window -d "
	printf '\033[3;1H'
	printf '\033_Ga=T,q=2,f=32,s=1,v=4,c=1,r=4;AAAA/1VVVf+qqqr//////w==\033\\'
	sleep 10" || exit 1
$TMUX2 select-window -t:2 || exit 1
sleep 1
[ "$($TMUX2 display-message -p '#{cursor_y}')" = 3 ] || exit 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ "$(sed -n 1p $TMP)" = "-" ] || exit 1
[ "$(sed -n 2p $TMP)" = "*" ] || exit 1
[ "$(sed -n 3p $TMP)" = "@" ] || exit 1

# Image marker rows remain cell-aligned when a narrower terminal causes text
# reflow. The ten-column rows are clipped to five columns, not split into four
# wrapped rows.
$TMUX2 new-window -d "
	printf '\033_Ga=T,q=2,C=1,f=32,s=1,v=2,c=10,r=2;/wAA//////8=\033\\'
	sleep 10" || exit 1
$TMUX2 select-window -t:3 || exit 1
sleep 1
$TMUX resize-window -x 5 -y 4 || exit 1
sleep 1
[ "$($TMUX2 display-message -p '#{window_width}x#{window_height}')" = "5x4" ] || exit 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ "$(sed -n 1p $TMP)" = "....." ] || exit 1
[ "$(sed -n 2p $TMP)" = "@@@@@" ] || exit 1
[ -z "$(sed -n 3p $TMP)" ] || exit 1

# Kitty virtual placements use U+10EEEE placeholder cells. Keep their rows at
# fixed coordinates when narrowing the terminal, clipping instead of reflowing
# the second half onto the following row.
$TMUX resize-window -x 10 -y 4 || exit 1
sleep 1
$TMUX2 new-window -d "
	printf '\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205'
	printf '\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205\\364\\216\\273\\256\\314\\205'
	sleep 10" || exit 1
$TMUX2 select-window -t:4 || exit 1
sleep 1
$TMUX resize-window -x 5 -y 4 || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ -n "$(sed -n 1p $TMP)" ] || exit 1
[ -z "$(sed -n 2p $TMP)" ] || exit 1
$TMUX resize-window -x 10 -y 4 || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ "$(sed -n 1p $TMP | wc -c)" = 61 ] || exit 1
[ -z "$(sed -n 2p $TMP)" ] || exit 1

# A placement ID supplied with transmit-and-place is reused by a later place.
# This is the sequence used by chawan: the newline moves the cursor down before
# the second command, so the folder moves down one row without leaving a copy.
PLACEMENT_WINDOW=$($TMUX2 new-window -dP -F '#{window_id}' "
	printf '\033_GC=1,s=26,v=26,p=1,q=2,i=1,a=T,f=100,m=0;iVBORw0KGgoAAAANSUhEUgAAABoAAAAaCAYAAACpSkzOAAACmUlEQVR4XmOgF2CEWVRfX8/w//9/BkZGRgy7YeKNjY1kuwtsam1tLQMTE5MG0MBsoEWW6KYBxf8DxScD6UVNTU1kWcZYU1MD0qjLzMx8EmgYJy5TgJb8/ffvn+2vX7+Od3Z2kmwZC9AnIE0FIEuAhl0H4ulA9k9kk4Bi4UB1TkA8H2iRYXl5+Xd2dna8loGCG2Q2LLgZQXED1PEIaLjs379/g4Ds9VDLwQYBxRiAPpFmZWW9DFQjCDRgLxDfJeQloJpdQHPWA9X9A1kGs+gZ0BBJoKQtUOIIeqSDgheoKR6IF5ASZkAH3gKaCzLzFQsxGltaWhjq6uoWguIJ6hi82oCG8wIV+AEdpga0LBfIrmUh1oWg1AYM5iVA9UuI1JMGVDcTiCNIsghkOChIQ0NDwfasXr0ap33QeL8GUgD0HTeIZmIgAUAtYQZqYYZZSKx2oi2CWaKpqbkIhEm1jOg40tLSAjleEBgUUSAGkJ8PpN8Q6yOCFoF8AvQBTvOAqZHh+vXrDPjijGAcIcXJUqBPQOXda5iNQP5rkBiQv5SYYMTrIxUVFZC5IkDCCk8QWQHVgdS8xBeMeBNDe3s7A7CwfQl0uSmwKDIBYhdQGQbCIDZIDCQHUgNSS7ZFII2gUgEU6UADzwJz+kUgDa6zoOyzIDmoGgaKLAJpBheKkArxC7BIeQDCQPEvIEuJrQyJzkdQA38ADbcHYaBFP0ipcVGqCWDYBwPxOkYs1TkDCeA/pEkQBDRnLZD9GKhVjgUkCGTsBgomAekWIJYG4p8MlAFQrZgJMgJo/m4QzQJ1fT9QIBLI1gTiSRRaAm/gAM38DjSvH2QeuHECLW1xNk7IsRhoyXGgJVOBem+AExPMEHzNLTItYiAlVTJQCwAAJxkMkgbHo2UAAAAASUVORK5CYII=\033\\'
	printf '\n'
	sleep 1
	printf '\033_GC=1,s=26,v=26,p=1,q=2,i=1,a=p;\033\\'
	printf '\n'
	sleep 10") || exit 1
$TMUX2 select-window -t"$PLACEMENT_WINDOW" || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ -z "$(sed -n 1p $TMP)" ] || exit 1
[ -n "$(sed -n 2p $TMP)" ] || exit 1

# A nonzero Kitty placement ID identifies one placement of an image. Reusing
# the same image and placement IDs moves it rather than leaving the old cells.
PLACEMENT_WINDOW=$($TMUX2 new-window -dP -F '#{window_id}' "
	printf '\033_Ga=t,q=2,f=32,s=1,v=1,i=11;/////w==\033\\'
	printf '\033[2;2H\033_Ga=p,q=2,C=1,i=11,p=7,c=2,r=1\033\\'
	printf '\033[4;6H\033_Ga=p,q=2,C=1,i=11,p=7,c=2,r=1\033\\'
	sleep 10") || exit 1
$TMUX2 select-window -t"$PLACEMENT_WINDOW" || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ -z "$(sed -n 2p $TMP)" ] || exit 1
[ "$(sed -n 4p $TMP)" = "     @@" ] || exit 1

# Deleting one placement ID leaves other placements of the image intact.
PLACEMENT_WINDOW=$($TMUX2 new-window -dP -F '#{window_id}' "
	printf '\033_Ga=t,q=2,f=32,s=1,v=1,i=12;/////w==\033\\'
	printf '\033[2;2H\033_Ga=p,q=2,C=1,i=12,p=7,c=2,r=1\033\\'
	printf '\033[4;6H\033_Ga=p,q=2,C=1,i=12,p=8,c=2,r=1\033\\'
	printf '\033_Ga=d,d=i,q=2,i=12,p=7\033\\'
	sleep 10") || exit 1
$TMUX2 select-window -t"$PLACEMENT_WINDOW" || exit 1
sleep 1
$TMUX capture-pane -pS0 -E3 >$TMP || exit 1
[ -z "$(sed -n 2p $TMP)" ] || exit 1
[ "$(sed -n 4p $TMP)" = "     @@" ] || exit 1

# A weighted median at the maximum channel level must still leave colours on
# both sides of the split. This skewed black, grey and white image used to stop
# palette generation after one colour instead of producing three.
$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
$TMUX2 new-session -d -x 20 -y 4 "
	printf '\033_Ga=T,q=2,C=1,f=24,s=100,v=1,c=10,r=1;AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICA////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\033\\'
	sleep 10" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -as terminal-features ',*:sixel' || exit 1
$TMUX new-session -d -x 20 -y 4 || exit 1
$TMUX set -g status off || exit 1
$TMUX pipe-pane -O "cat >$TMP" || exit 1
$TMUX send-keys -l "$TMUX2 attach-session" || exit 1
$TMUX send-keys Enter || exit 1
sleep 1
grep -a '#2;2;' $TMP >/dev/null || exit 1

exit 0
