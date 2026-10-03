#!/bin/sh

# Visual test for sixel-region-scrolling: does this terminal actually move
# SIXEL pixels along with tmux's own scroll-region escapes (DECSTBM/DECSLRM
# + IND/RI/RIN), or does tmux need to fall back to redrawing the image on
# every scroll?
#
# There is no way to query a terminal for this, so it has to be judged by
# eye. Run this from inside the tmux session you want to test (attached
# from whichever terminal emulator you're checking), and watch the
# numbered colour bands in the floating pane while the counter below it
# scrolls slowly. Run it once with the option on, once with it off, and
# compare:
#
#   - "off" is the known-good baseline: tmux redraws the whole image every
#     scroll, so the bands must stay perfectly aligned with the counter no
#     matter what the terminal does on its own.
#   - "on" trusts the terminal to have moved the pixels itself. If it
#     looks identical to "off", this terminal is fine. If a band freezes,
#     duplicates, tears, or drifts out of sync with the counter, this
#     terminal does not scroll SIXEL regions correctly and
#     sixel-region-scrolling should stay off for it.
#
# Usage: sh tools/sixel-scroll-region-test.sh [on|off]

set -eu
cd "$(dirname "$0")/.."

MODE=${1:-on}
case "$MODE" in
on|off) ;;
*) echo "usage: $0 [on|off]" >&2; exit 1 ;;
esac

tmux set -s sixel-region-scrolling "$MODE"
tmux set -as terminal-features ',*:sixel'

echo "Testing with sixel-region-scrolling=$MODE. Watch the coloured bands" >&2
echo "in the floating pane - they should stay locked to the counter." >&2
sleep 2

PANE=$(tmux new-pane -d -x 24 -y 22 -PF '#{pane_id}' \
	"cat '$(pwd)/tools/sixel-ruler.six'; for i in \$(seq 1 50); do echo line \$i; sleep 0.3; done; echo done - press enter; read _")
tmux select-pane -t "$PANE"
