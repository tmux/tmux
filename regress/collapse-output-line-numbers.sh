#!/bin/sh

# Folded line numbers must have room for the unfolded source's row indices.

. ./input-common.inc

start_pane_hlimit numbers 80 12 '' 20000
target=numbers:
$TMUX respawn-pane -t "$target" -k \
	"printf '\033]133;A\007p> \033]133;B\007first\r\n\033]133;C\007'; seq 1 12000; printf '\033]133;D;0\007\033]133;A\007NEXT> \033]133;B\007\033]2;numbers-ready\007'; $INPUT_HOLD" || exit 1
wait_for_output numbers-ready $TMUX display -t "$target" -p '#{pane_title}' || exit 1
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
	grep -q "$1" "$TMP"
}

for mode in default absolute hybrid; do
	$TMUX set -g copy-mode-line-numbers "$mode" || exit 1
	$TMUX copy-mode -t "$target" -c || exit 1
	wait_until number_matches '^12002 .*NEXT>' || fail "$mode source line number"
	$TMUX send -t "$target" -X cancel || exit 1
done

# Navigation and the position indicator use the displayed source numbers.
seq=
i=0
while [ "$i" -lt 15 ]; do
	seq="${seq}\033]133;A\007P$i> \033]133;B\007cmd\r\n"
	seq="${seq}\033]133;C\007OUTPUT\r\n\033]133;D;0\007"
	i=$((i + 1))
done
seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
$TMUX new-window -t numbers: -n navigation \
	"printf '$seq'; printf '\033]2;navigation-ready\007'; $INPUT_HOLD" || exit 1
target=numbers:navigation
wait_for_output navigation-ready $TMUX display -t "$target" -p '#{pane_title}' || exit 1
$TMUX set -w -t "$target" window-size manual || exit 1
$TMUX resize-window -t "$target" -y5 || exit 1
for keys in emacs vi; do
	$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
	for mode in default absolute relative hybrid; do
		$TMUX set -w -t "$target" copy-mode-line-numbers "$mode" || exit 1
		$TMUX copy-mode -t "$target" -c || exit 1
		for number in 19 20; do
			$TMUX send -t "$target" -X goto-line "$number" || exit 1
			wait_for_text 'P9> cmd' $OUTER capture-pane -p -S0 -E0 || \
				fail "$mode goto-line $number"
			actual=$($TMUX display -t "$target" -p \
				'#{copy_position}/#{copy_position_limit}')
			[ "$actual" = 19/31 ] || fail "$mode source position"
		done
		$TMUX send -t "$target" -X cancel || exit 1
	done
done

# Joined rows are numbered from their first source row, rather than the last.
$TMUX new-window -t numbers: -n reflow "$INPUT_HOLD" || exit 1
target=numbers:reflow
$TMUX set -w -t "$target" window-size manual || exit 1
$TMUX resize-window -t "$target" -x 20 || exit 1
$TMUX respawn-pane -t "$target" -k \
	"printf '\033]133;A\007p> \033]133;B\007first\r\n\033]133;C\007'; printf '%060d' 0 | tr 0 A; printf '\033]133;D;0\007\r\n\033]133;A\007NEXT> \033]133;B\007\033]2;reflow-ready\007'; $INPUT_HOLD" || exit 1
wait_for_output reflow-ready $TMUX display -t "$target" -p '#{pane_title}' || exit 1
joined=$(printf '%060d' 0 | tr 0 A)
for mode in default absolute hybrid; do
	$TMUX resize-window -t "$target" -x 20 || exit 1
	$TMUX set -g copy-mode-line-numbers "$mode" || exit 1
	$TMUX copy-mode -t "$target" -U || exit 1
	$TMUX send -t "$target" -X history-top || exit 1
	$TMUX send -t "$target" -X search-forward-text AAAAA || exit 1
	$TMUX resize-window -t "$target" -x 80 || exit 1
	wait_until number_matches "^  2 .*$joined" || \
		fail "$mode joined source line number"
	$TMUX send -t "$target" -X cancel || exit 1
done

exit "$exit_status"
