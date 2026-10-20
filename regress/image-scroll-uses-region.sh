#!/bin/sh

# The PR's narrow-pane scroll check. Margins permit native scrolling;
# without them imagescroll must not suppress a necessary repaint.
# Different clients must make that decision independently.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_scroll import check_scroll, check_clients

check_scroll(sys.argv[1], True, margins=True)
check_scroll(sys.argv[1], True, margins=False)
check_clients(sys.argv[1])
check_clients(sys.argv[1], narrow=True)
PY
