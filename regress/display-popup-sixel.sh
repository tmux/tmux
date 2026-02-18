#!/bin/sh

# Test Sixel image rendering in display-popup overlays.
#
# Verifies that Sixel images are drawn when a popup is active, both with
# default borders and in borderless (-B) mode.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"

$TMUX kill-server 2>/dev/null

SIXEL=$(mktemp)
CMD=$(mktemp)
OUT=$(mktemp)
trap "rm -f $SIXEL $CMD $OUT; $TMUX kill-server 2>/dev/null" 0 1 15

# Minimal 1x1 Sixel image.
printf '\033Pq#0;2;0;0;0#1;2;100;100;100"1;1;1;1#1?\033\\' >"$SIXEL"

# First check that this build has Sixel support at all.
cat >"$CMD" <<EOF
#!/bin/sh
TERM=screen $TMUX new-session "cat '$SIXEL'; sleep 1"
EOF
chmod +x "$CMD"
script -q -c "$CMD" "$OUT" >/dev/null 2>&1
if ! grep -aq "SIXEL IMAGE" "$OUT"; then
	# Sixel not available in this build, skip.
	exit 0
fi

# Test bordered popup (default).
cat >"$CMD" <<EOF
#!/bin/sh
TERM=screen $TMUX new-session "tmux display-popup -E 'cat \"$SIXEL\"'; sleep 1"
EOF
script -q -c "$CMD" "$OUT" >/dev/null 2>&1 || exit 1
grep -aq "SIXEL IMAGE" "$OUT" || exit 1

# Test borderless popup (-B).
cat >"$CMD" <<EOF
#!/bin/sh
TERM=screen $TMUX new-session "tmux display-popup -B -E 'cat \"$SIXEL\"'; sleep 1"
EOF
script -q -c "$CMD" "$OUT" >/dev/null 2>&1 || exit 1
grep -aq "SIXEL IMAGE" "$OUT" || exit 1

$TMUX kill-server 2>/dev/null
exit 0
