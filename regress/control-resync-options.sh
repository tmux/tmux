#!/bin/sh

# Option surface for the control-mode resync feature. No timing: every check is
# the exit status of a set-option/show-options.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "$TMUX kill-server 2>/dev/null; rm -f $TMP" 0 1 15

$TMUX -f/dev/null new -d || exit 1
sleep 1

# control-resync defaults to on.
$TMUX show-options -g control-resync >$TMP || exit 1
echo "control-resync on" | cmp -s $TMP - || exit 1

# All three choices are settable; a bogus value is rejected.
$TMUX set -g control-resync off || exit 1
$TMUX set -g control-resync on || exit 1
$TMUX set -g control-resync keep-pause || exit 1
$TMUX set -g control-resync bogus 2>/dev/null && exit 1

# control-resync-size is gone: the trigger is measured against history-limit,
# not a byte option, so set and show both fail.
$TMUX set -g control-resync-size 65536 2>/dev/null && exit 1
$TMUX show-options -g control-resync-size 2>/dev/null && exit 1

# The rest of the old option surface is gone too: set and show both fail on
# every removed name.
$TMUX set -g control-resync-bytes 1 2>/dev/null && exit 1
$TMUX set -g control-resync-seconds 1 2>/dev/null && exit 1
$TMUX set -g control-resync-marker x 2>/dev/null && exit 1
$TMUX set -g control-resync-pause-after 1 2>/dev/null && exit 1
$TMUX show-options -g control-resync-bytes 2>/dev/null && exit 1

$TMUX kill-server 2>/dev/null

exit 0
