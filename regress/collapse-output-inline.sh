#!/bin/sh

# Keep visible command mappings across hidden inline regions.

. ./input-common.inc

for keys in emacs vi; do
	for width in 20 80; do
		for length in 1 $((width - 9)) $((width - 8)) 40 $((2 * width + 7)); do
			body=$(printf "%0${length}d" 0 | tr 0 F)
			for end in inline next shared; do
				# Keep prompts on distinct source rows.
				if [ "$end" = shared ] && [ "$length" -le $((width - 9)) ]; then
					continue
				fi
				seq='\033]133;A\007P0> \033]133;B\007first'
				seq="${seq}\033]133;C\007${body}"
				if [ "$end" = next ]; then
					seq="${seq}\r\n\033]133;D;0\007"
				elif [ "$end" = inline ]; then
					seq="${seq}\033]133;D;0\007\r\n"
				else
					seq="${seq}\033]133;D;0\007"
				fi
				seq="${seq}\033]133;A\007P1> \033]133;B\007second\r\n"
				seq="${seq}\033]133;C\007SECOND\r\n"
				seq="${seq}\033]133;D;0\007\033]133;A\007P2> \033]133;B\007"
				start_pane_history inline "$width" 16 "$seq"
				target=inline:
				$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
				for view in -U -c; do
					$TMUX copy-mode -t "$target" "$view" || exit 1
					$TMUX send -t "$target" -X search-backward-text P0 || exit 1
					$TMUX send -t "$target" -X collapse-output || exit 1
					$TMUX capture-pane -t "$target" -pMJ -S0 -E1 >"$TMP"
					printf 'P0> first\nP1> second\n' >"$EXP"
					cmp "$TMP" "$EXP" || \
						fail "$keys $width $length $end $view command boundaries"
					cursor=$($TMUX display -t "$target" -p '#{copy_cursor_line}')
					[ "$cursor" = 'P0> first' ] || \
						fail "$keys $width $length $end $view collapsed cursor"

					# Reopen without moving the cursor.
					$TMUX send -t "$target" -X toggle-output || exit 1
					$TMUX capture-pane -t "$target" -pMJ -S - -E - >"$TMP" || exit 1
					grep -Fq "$body" "$TMP" || \
						fail "$keys $width $length $end $view immediate reopen"
					$TMUX send -t "$target" -X toggle-output || exit 1
					$TMUX set-buffer sentinel || exit 1
					$TMUX send -t "$target" -X copy-output || exit 1
					[ "$($TMUX show-buffer)" = "$body" ] || \
						fail "$keys $width $length $end $view output identity"
					$TMUX send -t "$target" -X select-output || exit 1
					$TMUX send -t "$target" -X copy-selection || exit 1
					[ "$($TMUX show-buffer)" = "$body" ] || \
						fail "$keys $width $length $end $view selection"
					$TMUX send -t "$target" -X cancel || exit 1
				done
			done
		done
	done
done

# A separator on a later command row must preserve that row's cursor too.
for keys in emacs vi; do
	for width in 20 80; do
		seq='\033]133;A\007P0> \033]133;B\007first\r\nlast'
		seq="${seq}\033]133;D;0\007REMOVED\033]133;A\007P1> "
		seq="${seq}\033]133;B\007second\r\n\033]133;C\007SECOND\r\n"
		seq="${seq}\033]133;D;0\007\033]133;A\007P2> \033]133;B\007"
		start_pane_history separator "$width" 16 "$seq"
		target=separator:
		$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
		$TMUX copy-mode -t "$target" -U || exit 1
		$TMUX send -t "$target" -X search-backward-text last || exit 1
		$TMUX send -t "$target" -X collapse-output || exit 1
		$TMUX capture-pane -t "$target" -pMJ -S0 -E2 >"$TMP" || exit 1
		printf 'P0> first\nlast\nP1> second\n' >"$EXP"
		cmp "$TMP" "$EXP" || fail "$keys separator command boundaries"
		cursor=$($TMUX display -t "$target" -p '#{copy_cursor_line}')
		[ "$cursor" = last ] || fail "$keys separator cursor mapping"
		$TMUX send -t "$target" -X toggle-output || exit 1
		$TMUX capture-pane -t "$target" -pMJ -S - -E - >"$TMP" || exit 1
		grep -Fq REMOVED "$TMP" || fail "$keys separator immediate reopen"
		$TMUX send -t "$target" -X cancel || exit 1
	done
done

# An empty C/D pair must not split text with an unchanged source mapping.
command=$(printf '%040d' 0 | tr 0 a)
seq="\033]133;A\007P0> \033]133;B\007${command}\033]133;C\007"
seq="${seq}\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second"
for keys in emacs vi; do
	start_pane_history empty 20 16 "$seq"
	target=empty:
	$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
	$TMUX copy-mode -t "$target" -U || exit 1
	$TMUX send -t "$target" -X search-backward-text P0 || exit 1
	$TMUX capture-pane -t "$target" -pMJ -S - -E - >"$EXP" || exit 1
	$TMUX send -t "$target" -X collapse-output || exit 1
	$TMUX capture-pane -t "$target" -pMJ -S - -E - >"$TMP" || exit 1
	cmp "$TMP" "$EXP" || fail "$keys empty output changed wrapping"
done

exit "$exit_status"
