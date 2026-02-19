#!/bin/sh

# Test Sixel image rendering in display-popup overlays.
#
# Verifies that Sixel images are drawn when a popup is active, both with
# default borders and in borderless (-B) mode.
#
# Uses `script` to capture raw terminal output (including escape sequences)
# via a pty, then checks for the "SIXEL IMAGE" marker that tmux emits when
# its sixel code path processes a DCS sixel sequence.
#
# This test verifies that input_dcs_dispatch() reaches the sixel parser
# inside popups (where wp == NULL). It does NOT test visual pixel rendering
# or cleanup -- that requires a real terminal emulator.
#
# Dependencies: script (util-linux), mktemp, grep, sh -- all standard POSIX.
# No X11, no graphical terminal, no ImageMagick required.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -Ltest"

$TMUX kill-server 2>/dev/null

SIXEL=$(mktemp)
CMD=$(mktemp)
OUT=$(mktemp)
trap "rm -f $SIXEL $CMD $OUT; $TMUX kill-server 2>/dev/null" 0 1 15

# Minimal 1x1 Sixel image (hand-crafted DCS sequence, no external tools):
#   \033P       DCS introducer
#   q           sixel mode
#   #0;2;0;0;0  define color 0: black (RGB 0%,0%,0%)
#   #1;2;100;100;100  define color 1: white (RGB 100%,100%,100%)
#   "1;1;1;1    raster attributes: aspect 1:1, 1x1 pixel
#   #1?         select color 1, data byte '?' (one pixel, bit 0 set)
#   \033\\      ST (string terminator)
printf '\033Pq#0;2;0;0;0#1;2;100;100;100"1;1;1;1#1?\033\\' >"$SIXEL"

# Probe: check that this build has sixel support compiled in.
# Run cat in a plain session and look for the "SIXEL IMAGE" log marker.
# If absent, sixel is not available -- skip gracefully (exit 0, not failure).
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
# Opens display-popup with default border, cats the sixel inside it.
# Asserts that the DCS sixel sequence reaches the parser (grep for marker).
# Before the input_dcs_dispatch() fix, popups had wp==NULL so all DCS
# sequences were dropped before reaching the sixel code path.
cat >"$CMD" <<EOF
#!/bin/sh
TERM=screen $TMUX new-session "tmux display-popup -E 'cat \"$SIXEL\"'; sleep 1"
EOF
script -q -c "$CMD" "$OUT" >/dev/null 2>&1 || exit 1
grep -aq "SIXEL IMAGE" "$OUT" || exit 1

# Test borderless popup (-B).
# Exercises the BOX_LINES_NONE code path where image position calculation
# differs: pd->px + im->px instead of pd->px + 1 + im->px (no border offset).
cat >"$CMD" <<EOF
#!/bin/sh
TERM=screen $TMUX new-session "tmux display-popup -B -E 'cat \"$SIXEL\"'; sleep 1"
EOF
script -q -c "$CMD" "$OUT" >/dev/null 2>&1 || exit 1
grep -aq "SIXEL IMAGE" "$OUT" || exit 1

$TMUX kill-server 2>/dev/null
exit 0
