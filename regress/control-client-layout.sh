#!/bin/sh

# Control clients must opt in to the current layout serialization.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lcontrol-client-layout -f/dev/null"
$TMUX kill-server 2>/dev/null

IN1=$(mktemp -u)
IN2=$(mktemp -u)
OUT1=$(mktemp)
OUT2=$(mktemp)
mkfifo "$IN1" "$IN2" || exit 1
trap '$TMUX kill-server 2>/dev/null; rm -f "$IN1" "$IN2" "$OUT1" "$OUT2"' 0 1 15

# Open both ends before starting either client so FIFO setup cannot block.
exec 3<>"$IN1"
exec 4<>"$IN2"

$TMUX new-session -d -x80 -y24 || exit 1
$TMUX split-window -h || exit 1
$TMUX new-pane -x20 -y8 -X5 -Y3 || exit 1

$TMUX -C attach <&3 >"$OUT1" &
$TMUX -C attach -f new-window-layouts <&4 >"$OUT2" &
sleep 1

printf '%s\n' "list-windows -F 'layout:#{client_flags}:#{window_layout}'" >&3
printf '%s\n' "list-panes -F 'layout:#{client_flags}:#{window_layout}'" >&3
printf '%s\n' "list-sessions -F 'layout:#{client_flags}:#{window_layout}'" >&3
printf '%s\n' "display-message -p 'layout:#{client_flags}:#{window_layout}'" >&3
printf '%s\n' "list-windows -F 'layout:#{client_flags}:#{window_layout}'" >&4
sleep 1

# Default control output is legacy; new-window-layouts output is current.
awk '/^layout:/ { print }' "$OUT1" | while IFS= read -r line; do
	case "$line" in
	*new-window-layouts*|*v2:*|*%0,*|*';'*) exit 1 ;;
	esac
done || exit 1
[ "$(grep -ac '^layout:' "$OUT1")" -eq 6 ] || exit 1
grep -a '^layout:.*new-window-layouts.*,v2:.*%0,' "$OUT2" >/dev/null ||
    exit 1

# Toggling the flag changes subsequent format expansion for the same client.
printf '%s\n' "refresh-client -f new-window-layouts" >&3
printf '%s\n' "display-message -p 'new:#{client_flags}:#{window_layout}'" >&3
printf '%s\n' "refresh-client -f '!new-window-layouts'" >&3
printf '%s\n' "display-message -p 'legacy:#{client_flags}:#{window_layout}'" >&3
sleep 1
grep -a '^new:.*new-window-layouts.*,v2:.*%0,' "$OUT1" >/dev/null ||
    exit 1
grep -a '^legacy:' "$OUT1" | tail -1 | grep -v '%0,' >/dev/null || exit 1

# One layout change is formatted independently for each connected client.
: >"$OUT1"
: >"$OUT2"
$TMUX resize-pane -t%0 -R || exit 1
sleep 1
LEGACY=$(grep -a '^%layout-change ' "$OUT1" | tail -1)
CURRENT=$(grep -a '^%layout-change ' "$OUT2" | tail -1)
[ -n "$LEGACY" ] || exit 1
[ -n "$CURRENT" ] || exit 1
case "$LEGACY" in *v2:*|*%0,*|*';'*) exit 1 ;; esac
case "$CURRENT" in *v2:*%0,*) ;; *) exit 1 ;; esac
[ "$(printf '%s' "$CURRENT" | awk '{ print gsub(/%0,/, "") }')" -ge 2 ] ||
    exit 1
[ "$(printf '%s' "$CURRENT" | awk '{ print gsub(/v2:/, "") }')" -ge 2 ] ||
    exit 1

exec 3>&-
exec 4>&-
$TMUX kill-server 2>/dev/null

exit 0
