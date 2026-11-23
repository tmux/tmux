#!/bin/sh

# Folded line numbers must have room for the unfolded source's row indices.

. ./input-common.inc

start_pane_hlimit numbers 80 12 '' 20000
$TMUX respawn-pane -k \
	"printf '\033]133;A\007p> \033]133;B\007first\r\n\033]133;C\007'; seq 1 12000; printf '\033]133;D;0\007\033]133;A\007NEXT> \033]133;B\007\033]2;numbers-ready\007'; $INPUT_HOLD" || exit 1
wait_for_output numbers-ready $TMUX display -p '#{pane_title}' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g copy-mode-position-format '' || exit 1
OUTER="$TEST_TMUX -Ln$$ -f/dev/null"
trap 'rm -f "$TMP" "$EXP"; stop_server $OUTER; stop_server $TMUX' 0 1 15
$OUTER new-session -d -x 80 -y 12 "$TMUX attach -t numbers" || exit 1
$OUTER set -g status off || exit 1
wait_for_clients 1 $TMUX || exit 1

# Inspect the actual client screen, including the line-number gutter.
number_matches()
{
	$OUTER capture-pane -p >"$TMP" || return 1
	grep -q '^12002 .*NEXT>' "$TMP"
}

for mode in default absolute hybrid; do
	$TMUX set -g copy-mode-line-numbers "$mode" || exit 1
	$TMUX copy-mode -c || exit 1
	wait_until number_matches || fail "$mode source line number"
	$TMUX send -X cancel || exit 1
done

exit "$exit_status"
