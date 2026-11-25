#!/bin/sh

# C and a preceding or following command's D have independent fold state.

. ./input-common.inc

# Check both fold regions across the complete backing, including history.
check_regions()
{
	first_visible=$1
	second_visible=$2
	screen=$($TMUX capture-pane -t "$target" -pMJ -S - -E -) || exit 1
	case "$screen" in
	*"$first"*) first_shown=1 ;;
	*) first_shown=0 ;;
	esac
	case "$screen" in
	*"$second"*) second_shown=1 ;;
	*) second_shown=0 ;;
	esac
	if [ "$first_shown:$second_shown" != "$first_visible:$second_visible" ]; then
		echo "$keys $order: expected $first_visible:$second_visible, got $first_shown:$second_shown"
		printf '%s\n' "$screen"
		exit 1
	fi
}

# Apply a manual control from a command's prompt.
control()
{
	$TMUX send -t "$target" -X search-backward-text "$1>" || exit 1
	$TMUX send -t "$target" -X "$2" || exit 1
}

for keys in emacs vi; do
	for order in DC CD; do
		seq='\033]133;A\007P0> \033]133;B\007first\r\n'
		if [ "$order" = DC ]; then
			first=SEPARATOR
			second=SECOND
			output_prompt=P1
			seq="${seq}\033]133;D;0\007${first}\033]133;A\007P1> "
			seq="${seq}\033]133;B\007second\033]133;C\007${second}\r\n"
			seq="${seq}\033]133;D;0\007\r\n"
		else
			first=FIRST
			second=SEPARATOR
			output_prompt=P0
			seq="${seq}\033]133;C\007${first}\033]133;A\007P1> "
			seq="${seq}\033]133;B\007silent\033]133;D;0\007${second}\r\n"
		fi
		seq="${seq}\033]133;A\007P2> \033]133;B\007"
		for initial_width in 20 80; do
			start_pane_history anchors "$initial_width" 16 "$seq"
			target=anchors:
			$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
			$TMUX set -w -t "$target" window-size manual || exit 1
			$TMUX copy-mode -t "$target" -U || exit 1
			$TMUX send -t "$target" -X search-backward-text "$output_prompt>" || exit 1
			$TMUX send -t "$target" -X copy-output || exit 1
			output=$($TMUX show-buffer) || exit 1
			control P0 collapse-output
			check_regions 0 1

			# Refresh must preserve each bit as reflow splits or joins anchors.
			for width in 5 13 80; do
				$TMUX resize-window -t "$target" -x "$width" || exit 1
				$TMUX send -t "$target" -X refresh-now || exit 1
				check_regions 0 1
			done
			control P1 collapse-output
			check_regions 0 0
			control P0 expand-output
			check_regions 1 0
			for width in 13 80; do
				$TMUX resize-window -t "$target" -x "$width" || exit 1
				$TMUX send -t "$target" -X refresh-now || exit 1
				check_regions 1 0
			done
			$TMUX send -t "$target" -X toggle-output -a || exit 1
			check_regions 1 1
			$TMUX send -t "$target" -X toggle-output -a || exit 1
			check_regions 0 0

			# Selecting C's output must leave the separator fold collapsed.
			$TMUX send -t "$target" -X search-backward-text "$output_prompt>" || exit 1
			$TMUX set-buffer sentinel || exit 1
			$TMUX send -t "$target" -X select-output || exit 1
			$TMUX send -t "$target" -X copy-selection || exit 1
			[ "$($TMUX show-buffer)" = "$output" ] || exit 1
			if [ "$order" = DC ]; then
				check_regions 0 1
			else
				check_regions 1 0
			fi
		done
	done
done

# Retained C markers before the first prompt use the initial region's bit.
for keys in emacs vi; do
	for start in inline next shared; do
		seq=initial-control
		if [ "$start" != inline ]; then
			seq="${seq}\r\n"
		fi
		seq="${seq}\033]133;C\007LEADING"
		if [ "$start" != shared ]; then
			seq="${seq}\r\n"
		fi
		seq="${seq}\033]133;D;0\007"
		seq="${seq}\033]133;A\007P0> \033]133;B\007command\r\n"
		seq="${seq}\033]133;C\007OUTPUT\r\n\033]133;D;0\007"
		seq="${seq}\033]133;A\007P1> \033]133;B\007"
		start_pane_history initial 80 16 "$seq"
		target=initial:
		first=LEADING
		second=OUTPUT
		$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
		$TMUX copy-mode -t "$target" -c || exit 1
		check_regions 0 0
		$TMUX send -t "$target" -X search-backward-text initial-control || exit 1
		$TMUX send -t "$target" -X toggle-output || exit 1
		check_regions 1 0
		$TMUX send -t "$target" -X toggle-output || exit 1
		check_regions 0 0
		$TMUX send -t "$target" -X select-output || exit 1
		$TMUX send -t "$target" -X copy-selection || exit 1
		[ "$($TMUX show-buffer)" = LEADING ] || exit 1
		check_regions 1 0
		$TMUX send -t "$target" -X toggle-output -a || exit 1
		check_regions 1 1
		$TMUX send -t "$target" -X toggle-output -a || exit 1
		check_regions 0 0
	done
done

exit "$exit_status"
