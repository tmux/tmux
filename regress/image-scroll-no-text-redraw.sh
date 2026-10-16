#!/bin/sh

# Adapt the PR's Kitty marker check to SIXEL. Native image scrolling must
# skip the surrounding text repaint as well as image retransmission.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_scroll import check_scroll

check_scroll(sys.argv[1], True)
check_scroll(sys.argv[1], False)
PY
