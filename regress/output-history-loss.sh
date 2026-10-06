#!/bin/sh

. ./input-common.inc

# A multiline command evicts its prompt before producing output.
command='\033]133;A\007p>\033]133;B\007echo\n'
i=0
while [ "$i" -lt 30 ]; do
	command="${command}continued command\n"
	i=$((i + 1))
done
OUTPUT=$(printf 'one\ntwo')

for state in complete unfinished next-prompt; do
	seq="${command}zz\033]133;C\007one\ntwo"
	case "$state" in
	complete|next-prompt)
		seq="${seq}\033]133;D;0\007separator"
		;;
	esac
	if [ "$state" = next-prompt ]; then
		seq="${seq}\n\033]133;A\007p>\033]133;B\007"
	fi
	start_pane_hlimit lost 40 5 "$seq" 3
	check_raw_matches lost 'START_OUTPUT.*osc133=0,0,2,0,0'
	if [ "$state" != next-prompt ]; then
		check_raw_no_matches lost 'START_PROMPT'
	fi
	if [ "$state" != unfinished ]; then
		check_raw_matches lost 'END_OUTPUT.*osc133=0,0,0,3,0'
	fi

	for position in inside after; do
		for keys in emacs vi; do
			$TMUX set-option -w -t lost: mode-keys "$keys" ||
			    exit 1
			$TMUX copy-mode -t lost: || exit 1
			if [ "$position" = inside ]; then
				$TMUX send-keys -t lost: -X search-backward one ||
				    exit 1
			fi
			$TMUX set-buffer sentinel || exit 1
			$TMUX send-keys -t lost: -X copy-output || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			printf '%s' "$OUTPUT" >"$EXP"
			cmp "$TMP" "$EXP" ||
			    fail "$state $position $keys output"

			$TMUX send-keys -t lost: -X select-output || exit 1
			present=$($TMUX display-message -p -t lost: \
			    '#{selection_present}')
			if [ "$present" != 1 ]; then
				echo "FAIL: $state $position $keys no selection"
				exit_status=1
			fi
			$TMUX set-buffer sentinel || exit 1
			$TMUX send-keys -t lost: -X copy-selection || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" ||
			    fail "$state $position $keys selection"
			$TMUX send-keys -t lost: -X cancel || exit 1
		done
	done
done

# Retained command text does not belong to a later prompt's output.
seq="${command}\033]133;A\007p>\033]133;B\007echo\n"
seq="${seq}\033]133;C\007later\033]133;D;0\007"
start_pane_hlimit later 40 5 "$seq" 3
$TMUX copy-mode -t later: || exit 1
$TMUX send-keys -t later: -X history-top || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send-keys -t later: -X copy-output || exit 1
[ "$($TMUX show-buffer)" = sentinel ] || exit 1
$TMUX send-keys -t later: -X select-output || exit 1
[ "$($TMUX display-message -p -t later: '#{selection_present}')" = 0 ] ||
    exit 1

exit $exit_status
