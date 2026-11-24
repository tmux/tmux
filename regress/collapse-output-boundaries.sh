#!/bin/sh

# Preserve tabs, sparse OSC 133 markers and cursor hyperlinks in folded views.

. ./input-common.inc
OUTER="$TEST_TMUX -Lb$$ -f/dev/null"
trap 'rm -f "$TMP" "$EXP"; stop_server $OUTER; stop_server $TMUX' 0 1 15

# A tab must remain one copied byte even when its columns span backing rows.
for keys in emacs vi; do
	for payload in '\tX' 'AB\tC\tX' '\t\tX'; do
		for width in 1 2 5 8 20; do
			seq='\033]133;A\007p> \033]133;B\007first\r\n'
			seq="${seq}\033]133;C\007${payload}\033]133;D;0\007\r\n"
			seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
			start_pane_history tabs 80 24 "$seq"
			$TMUX set -g mode-keys "$keys" || exit 1
			$TMUX copy-mode -U || exit 1
			$TMUX resize-window -x "$width" || exit 1
			$TMUX send -X search-backward-text X || exit 1
			printf "$payload" >"$EXP"
			$TMUX send -X copy-output || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" || fail "$keys tab output at $width"
			$TMUX set-buffer sentinel || exit 1
			$TMUX send -X select-output || exit 1
			$TMUX send -X copy-selection || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" || fail "$keys tab selection at $width"
		done
	done
done

# Unused rows below a short transcript must not prevent selecting it all.
seq='\033]133;A\007p> \033]133;B\007first\r\n'
seq="${seq}\033]133;C\007hello\033]133;D;0\007\r\n"
seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
for keys in emacs vi; do
	for view in -U -c; do
		start_pane_history short 80 24 "$seq"
		$TMUX set -g mode-keys "$keys" || exit 1
		$TMUX copy-mode "$view" || exit 1
		printf 'p> first\nhello\nNEXT>' >"$EXP"
		$TMUX send -X copy-output -a || exit 1
		$TMUX save-buffer "$TMP" || exit 1
		cmp "$TMP" "$EXP" || fail "$keys $view short all output"
		$TMUX set-buffer sentinel || exit 1
		$TMUX send -X select-output -a || exit 1
		$TMUX send -X copy-selection || exit 1
		$TMUX save-buffer "$TMP" || exit 1
		cmp "$TMP" "$EXP" || fail "$keys $view short all selection"
	done
done

# Wide characters wrapping early must not add spaces when copying either grid.
for keys in emacs vi; do
	for payload in 'AB界CD' 'A界界界Z' 'é界Z' 'AB 界CD' 'AB  界CD'; do
		for sizes in '80 2' '80 3' '80 5' '80 8' '80 20' \
		    '3 80' '5 80' '20 80'; do
			set -- $sizes
			initial_width=$1
			width=$2
			seq='\033]133;A\007p> \033]133;B\007first\r\n'
			seq="${seq}\033]133;C\007${payload}\033]133;D;0\007\r\n"
			seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
			start_pane_history wide "$initial_width" 24 "$seq"
			$TMUX set -g mode-keys "$keys" || exit 1
			$TMUX copy-mode -U || exit 1
			$TMUX resize-window -x "$width" || exit 1
			$TMUX send -X history-top || exit 1
			printf '%s' "$payload" >"$EXP"
			$TMUX send -X copy-output || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" || \
				fail "$keys wide output, $initial_width to $width"
			$TMUX set-buffer sentinel || exit 1
			$TMUX send -X select-output || exit 1
			$TMUX send -X copy-selection || exit 1
			$TMUX save-buffer "$TMP" || exit 1
			cmp "$TMP" "$EXP" || \
				fail "$keys wide selection, $initial_width to $width"
		done
	done
done

# All four markers can be placed beyond the last stored text cell.
seq='\033[11G\033]133;A\007\033[21G\033]133;B\007\r\n'
seq="${seq}\033[11G\033]133;C\007\r\nHIDDEN"
seq="${seq}\033[21G\033]133;D;7\007\r\n"
seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
start_pane_history markers 80 12 "$seq"
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1
$TMUX set -g copy-mode-exit-status-format \
	'#{?exit_status_present,RC=#{exit_status},}' || exit 1
$TMUX copy-mode -U || exit 1
$TMUX capture-pane -pMF -S0 -E0 >"$TMP" || exit 1
grep -q '^P ' "$TMP" || fail 'sparse A marker'
$TMUX capture-pane -pMF -S1 -E1 >"$TMP" || exit 1
grep -q '^O ' "$TMP" || fail 'sparse C marker'

# The D marker's status must reach the control row on the real client.
$OUTER new-session -d -x80 -y12 "$TMUX attach -t markers" || exit 1
$OUTER set -g status off || exit 1
wait_for_clients 1 $TMUX || exit 1
wait_for_text RC=7 $OUTER capture-pane -p || fail 'sparse D status'
stop_server $OUTER || exit 1

$TMUX send -X collapse-output -a || exit 1
$TMUX capture-pane -Mp >"$TMP" || exit 1
grep -q HIDDEN "$TMP" && fail 'output after sparse C was not collapsed'
grep -q NEXT "$TMP" || fail 'prompt after sparse D was hidden'

# B without C still makes separator text after D collapsible.
seq='\033]133;A\007p> \033[11G\033]133;B\007\r\n'
seq="${seq}\033]133;D;0\007HIDDEN-SEPARATOR\r\n"
seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
start_pane_history command 80 12 "$seq"
$TMUX copy-mode -c || exit 1
$TMUX capture-pane -Mp >"$TMP" || exit 1
grep -q HIDDEN "$TMP" && fail 'sparse B marker was missed'

# Gaps traversed by the cursor alone do not make empty output expandable.
seq='\033]133;A\007p> \033]133;B\007empty\r\n'
seq="${seq}\033]133;C\007\033[11G\033]133;D;0\007\r\n"
seq="${seq}\033]133;A\007NEXT> \033]133;B\007"
start_pane_history empty 80 12 "$seq"
$TMUX copy-mode -c || exit 1
$TMUX send -X search-backward-text empty || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX send -X copy-output || exit 1
[ "$($TMUX show-buffer)" = sentinel ] || fail 'cursor-only gap became output'

# Hyperlinks are looked up at the displayed cursor, after either gutter.
uri=https://example.com/fold-review
seq='\033]133;A\007p> \033]133;B\007link\r\n\033]133;C\007'
seq="${seq}\033]8;;${uri}\033\\LABEL\033]8;;\033\\"
seq="${seq}\033]133;D;0\007\r\n\033]133;A\007NEXT> \033]133;B\007"
start_pane_history hyperlinks 80 12 "$seq"
for numbers in off default absolute relative hybrid; do
	$TMUX set -g copy-mode-line-numbers "$numbers" || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send -X search-backward-text LABEL || exit 1
	for toggle in 0 1 2; do
		actual=$($TMUX display -p '#{copy_cursor_hyperlink}')
		[ "$actual" = "$uri" ] || fail "$numbers gutter hyperlink $toggle"
		$TMUX send -X fold-view-toggle || exit 1
	done
	$TMUX send -X cancel || exit 1
done

exit "$exit_status"
