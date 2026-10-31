#!/bin/sh

# The PR's trusted-scroll erase regression: inspect terminal erases directly
# rather than depending on a particular debug log message.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_scroll import check_scroll

check_scroll(sys.argv[1], True, no_erase=True)
PY
