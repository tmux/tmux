#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
export TEST_TMUX
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x80 -y24 \
    'i=1; while [ $i -le 60 ]; do printf "\033]133;A\007cmd%s\nout\nout\nout\n" "$i"; i=$((i + 1)); done; "$TEST_TMUX" wait-for -S ready; exec cat' || exit 1
$TMUX wait-for ready || exit 1
failed=0
for keys in vi emacs; do
	$TMUX set -g mode-keys "$keys" || exit 1
	for direction in previous next; do
		for count in 1 3 100; do
			for method in repeated individual; do
				$TMUX copy-mode || exit 1
				if [ "$direction" = next ]; then
					$TMUX send -X history-top || exit 1
				fi
				if [ "$method" = repeated ]; then
					$TMUX send -N "$count" -X "$direction-prompt" || exit 1
				else
					i=0
					while [ "$i" -lt "$count" ]; do
						$TMUX send -X "$direction-prompt" || exit 1
						i=$((i + 1))
					done
				fi
				position=$($TMUX display -p '#{scroll_position}:#{copy_cursor_x}:#{copy_cursor_y}') || exit 1
				if [ "$method" = repeated ]; then
					repeated=$position
				elif [ "$position" != "$repeated" ]; then
					echo "$keys $direction count $count: $repeated != $position"
					failed=1
				fi
				$TMUX send -X cancel || exit 1
			done
		done
	done
done
exit "$failed"
