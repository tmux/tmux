#!/bin/sh

# Keep command boundaries when inline output ends on a skipped wrapped row.

. ./input-common.inc

for keys in emacs vi; do
	for width in 20 80; do
		for length in 1 $((width - 9)) $((width - 8)) $((2 * width + 7)); do
			body=$(printf "%0${length}d" 0 | tr 0 F)
			for end in inline next; do
				seq='\033]133;A\007P0> \033]133;B\007first'
				seq="${seq}\033]133;C\007${body}"
				if [ "$end" = next ]; then
					seq="${seq}\r\n\033]133;D;0\007"
				else
					seq="${seq}\033]133;D;0\007\r\n"
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

exit "$exit_status"
