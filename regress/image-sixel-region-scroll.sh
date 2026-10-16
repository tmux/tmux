#!/bin/sh

# The PR's SIXEL imagescroll check, with known cell pixels and no shell race.
# Check the default repaint and an overwrite merged with trusted scroll damage.

PATH=/bin:/usr/bin
TERM=screen
export TERM
[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

python3 -B - "$TEST_TMUX" <<'PY'
import sys
from image_scroll import check_scroll, check_capability

for command in ('linefeed', 'up', 'down', 'reverse'):
    check_scroll(sys.argv[1], True, command=command)
    check_scroll(sys.argv[1], False, command=command)
check_scroll(sys.argv[1], True, mixed=True)
check_capability(sys.argv[1], True)
check_capability(sys.argv[1], False)
PY
