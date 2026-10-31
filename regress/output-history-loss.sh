#!/bin/sh

. ./input-common.inc

check_output()
{
	name=$1
	search=$2

	for position in inside after; do
		for keys in emacs vi; do
			$TMUX set-option -w -t "$name:" mode-keys "$keys" ||
			    exit 1
			$TMUX copy-mode -t "$name:" || exit 1
			if [ "$position" = inside ]; then
				$TMUX send-keys -t "$name:" -X search-backward \
				    "$search" || exit 1
			fi
			$TMUX set-buffer sentinel || exit 1
			$TMUX send-keys -t "$name:" -X copy-output || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" ||
			    fail "$name $position $keys output"

			$TMUX send-keys -t "$name:" -X select-output || exit 1
			present=$($TMUX display-message -p -t "$name:" \
			    '#{selection_present}')
			if [ "$present" != 1 ]; then
				echo "FAIL: $name $position $keys no selection"
				exit_status=1
			fi
			$TMUX set-buffer sentinel || exit 1
			$TMUX send-keys -t "$name:" -X copy-selection || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" ||
			    fail "$name $position $keys selection"
			$TMUX send-keys -t "$name:" -X cancel || exit 1
		done
	done
}

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

	printf '%s' "$OUTPUT" >"$EXP"
	check_output lost one
done

# A long output evicts both A and C but leaves D.
output='\033]133;A\007p>\033]133;B\007echo\n\033]133;C\007'
i=0
while [ "$i" -lt 30 ]; do
	output="${output}$(printf 'row%02d' "$i")\n"
	i=$((i + 1))
done
for state in complete next-prompt same-line-prompt; do
	seq="${output}\033]133;D;0\007separator"
	if [ "$state" != complete ]; then
		if [ "$state" = next-prompt ]; then
			seq="${seq}\n"
		fi
		seq="${seq}\033]133;A\007p>\033]133;B\007"
	fi
	start_pane_hlimit clipped 40 6 "$seq" 3
	check_raw_no_matches clipped 'START_OUTPUT'
	if [ "$state" = complete ]; then
		check_raw_no_matches clipped 'START_PROMPT'
	fi
	check_raw_matches clipped 'END_OUTPUT'
	$TMUX capture-pane -p -t clipped: -S - |
	    sed '/^separator/,$d' >"$EXP"
	check_output clipped row29
done

# Retained command text does not belong to a later prompt's output.
for newline in '' '\n'; do
	seq="${command}\033]133;A\007p>\033]133;B\007echo${newline}"
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
done

exit $exit_status
