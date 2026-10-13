#!/bin/sh

# Keep output mappings and individual folds across resize and refresh.

. ./input-common.inc

first=$(printf '%060d' 0 | tr 0 F)
second=SECOND-OUTPUT
seq="\033]133;A\007P0> \033]133;B\007first\r\n"
seq="${seq}\033]133;C\007${first}\r\n\033]133;D;7\007"
seq="${seq}\033]133;A\007P1> \033]133;B\007second\r\n"
seq="${seq}\033]133;C\007${second}\r\n\033]133;D;0\007"
seq="${seq}\033]133;A\007P2> \033]133;B\007"
commands_seq=$seq

# Narrow output can create more backing rows than there are source rows.
for hlimit in 2000 1; do
	start_pane_hlimit mapping 80 8 "$commands_seq" "$hlimit"
	$TMUX copy-mode -t mapping: -U || exit 1
	$TMUX resize-window -t mapping: -x 5 || exit 1
	$TMUX send -t mapping: -X search-backward-text FFFFF || exit 1
	$TMUX set-buffer sentinel || exit 1
	$TMUX send -t mapping: -X copy-output || exit 1
	$TMUX save-buffer "$TMP" || exit 1
	printf '%s\n' "$first" >"$EXP"
	cmp "$TMP" "$EXP" || fail "copy-output with history-limit $hlimit"
done

# Selection endpoints must land on the appropriate wrapped segment.
for keys in emacs vi; do
	for length in 60 61; do
		payload=$(printf "%0${length}d" 0 | tr 0 F)
		selection_seq="\033]133;A\007p> \033]133;B\007first\r\n"
		selection_seq="${selection_seq}\033]133;C\007${payload}"
		selection_seq="${selection_seq}\033]133;D;7\007\r\n"
		selection_seq="${selection_seq}\033]133;A\007p> \033]133;B\007"
		for width in 5 20 100; do
			start_pane_history selection 80 12 "$selection_seq"
			$TMUX set -w -t selection: mode-keys "$keys" || exit 1
			$TMUX copy-mode -t selection: -U || exit 1
			$TMUX resize-window -t selection: -x "$width" || exit 1
			$TMUX send -t selection: -X search-backward-text FFF || exit 1
			$TMUX set-buffer sentinel || exit 1
			$TMUX send -t selection: -X select-output || exit 1
			$TMUX send -t selection: -X copy-selection || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			printf '%s' "$payload" >"$EXP"
			cmp "$TMP" "$EXP" || \
				fail "$keys $length cells at width $width"
		done
	done
done

# Refresh must keep a mixture of collapsed and expanded commands.
for refresh in refresh-now refresh-on; do
	start_cmd refresh 80 12 \
		"printf '$commands_seq'; sleep 1; printf LIVE; sleep 1; printf LIVE; $INPUT_HOLD"
	$TMUX copy-mode -t refresh: -c || exit 1
	$TMUX send -t refresh: -X search-backward-text second || exit 1
	$TMUX send -t refresh: -X expand-output || exit 1
	for width in 20 100; do
		$TMUX resize-window -t refresh: -x "$width" || exit 1
		$TMUX send -t refresh: -X "$refresh" || exit 1
		sleep 1
		$TMUX send -t refresh: -X history-top || exit 1
		$TMUX capture-pane -t refresh: -Mp >"$TMP" || exit 1
		grep -q FFFFF "$TMP" && fail "$refresh expanded the first command"
		grep -q "$second" "$TMP" || \
			fail "$refresh collapsed the second command"
	done
done

# Initial output and commands without C use different fold anchors.
seq='initial-control\r\nHIDDEN-INITIAL\r\n'
seq="${seq}\033]133;A\007p> \033]133;B\007silent\r\n"
seq="${seq}\033]133;D;7\007HIDDEN-SEPARATOR\r\n"
seq="${seq}\033]133;A\007p> \033]133;B\007"
start_pane_history initial 80 12 "$seq"
$TMUX copy-mode -t initial: -c || exit 1
for width in 20 100; do
	$TMUX resize-window -t initial: -x "$width" || exit 1
	$TMUX send -t initial: -X refresh-now || exit 1
	$TMUX send -t initial: -X history-top || exit 1
	$TMUX capture-pane -t initial: -Mp >"$TMP" || exit 1
	grep -q HIDDEN "$TMP" && fail "initial folds lost at width $width"
	grep -q initial-control "$TMP" || fail 'initial control line lost'
done

exit "$exit_status"
