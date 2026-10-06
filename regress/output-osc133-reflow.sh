#!/bin/sh

. ./input-common.inc

check_output()
{
	name=$1
	expected=$2

	$TMUX copy-mode -t "$name:" || exit 1
	$TMUX send-keys -t "$name:" -X search-backward echo || exit 1
	$TMUX set-buffer sentinel || exit 1
	$TMUX send-keys -t "$name:" -X copy-output || exit 1
	$TMUX save-buffer "$TMP" || exit 1
	printf '%s' "$expected" >"$EXP"
	cmp "$TMP" "$EXP" || fail "$name output"
	$TMUX send-keys -t "$name:" -X cancel || exit 1
}

check_selection()
{
	name=$1
	expected=$2

	for keys in emacs vi; do
		$TMUX set-option -w -t "$name:" mode-keys "$keys" || exit 1
		$TMUX copy-mode -t "$name:" || exit 1
		$TMUX send-keys -t "$name:" -X search-backward echo || exit 1
		$TMUX send-keys -t "$name:" -X select-output || exit 1
		$TMUX set-buffer sentinel || exit 1
		$TMUX send-keys -t "$name:" -X copy-selection || exit 1
		$TMUX save-buffer "$TMP" || exit 1
		printf '%s' "$expected" >"$EXP"
		cmp "$TMP" "$EXP" || fail "$name $keys selection"
		$TMUX send-keys -t "$name:" -X cancel || exit 1
	done
}

check_pipe()
{
	name=$1
	expected=$2

	$TMUX copy-mode -t "$name:" || exit 1
	$TMUX send-keys -t "$name:" -X search-backward echo || exit 1
	printf '%s' "$expected" >"$EXP"
	: >"$TMP"
	$TMUX send-keys -t "$name:" -X pipe-output "cat >'$TMP'" || exit 1
	i=0
	while [ "$i" -lt 50 ]; do
		cmp -s "$TMP" "$EXP" && break
		sleep 0.1
		i=$((i + 1))
	done
	cmp "$TMP" "$EXP" || fail "$name pipe"
	$TMUX send-keys -t "$name:" -X cancel || exit 1
}

prompt='\033]133;A\007p>\033]133;B\007echo\n'
next='\n\033]133;A\007p>\033]133;B\007'
short=$(printf '%060d' 0)
long=$(printf '%0120d' 0)

# Completed output keeps its end marker when split or joined.
for payload in "$short" "$long"; do
	seq="${prompt}\033]133;C\007${payload}"
	seq="${seq}\033]133;D;7\007${next}"
	start_pane_history resize 80 20 "$seq"
	for width in 80 40 100 160 40 80; do
		$TMUX resize-window -t resize: -x "$width" || exit 1
		check_output resize "$payload"
	done
	check_selection resize "$payload"
	check_pipe resize "$payload"
done

# Nonzero columns and cuts exactly at marker positions.
seq='xx\033]133;A\007p>\033]133;B\007echo'
seq="${seq}\033]133;C\007output\033]133;D;9\007tail"
start_pane_history columns 80 20 "$seq"
$TMUX resize-window -t columns: -x 8 || exit 1
check_raw_matches columns \
    'L 0 .*flags=WRAPPED,START_PROMPT,START_COMMAND.*osc133=2,4,0,0,0' \
    'L 1 .*flags=WRAPPED,START_OUTPUT,END_OUTPUT.*osc133=0,0,0,6,9'
check_output columns output
$TMUX resize-window -t columns: -x 10 || exit 1
check_raw_matches columns \
    'L 0 .*START_PROMPT,START_COMMAND,START_OUTPUT.*osc133=2,4,8,0,0' \
    'L 1 .*flags=END_OUTPUT.*osc133=0,0,0,4,9'
check_output columns output
$TMUX resize-window -t columns: -x 14 || exit 1
check_raw_matches columns \
    'L 1 .*flags=END_OUTPUT.*osc133=0,0,0,0,9'
check_output columns output
$TMUX resize-window -t columns: -x 80 || exit 1
check_raw_matches columns \
    'L 0 .*START_COMMAND,START_OUTPUT,END_OUTPUT.*osc133=2,4,8,14,9'

start_pane_history secondary 80 20 'abcdefgh\033]133;P;k=s\007more'
$TMUX resize-window -t secondary: -x 8 || exit 1
check_raw_matches secondary 'L 1 .*flags=SECOND_PROMPT.*osc133=0,0,0,0,0'
$TMUX resize-window -t secondary: -x 80 || exit 1
check_raw_matches secondary 'L 0 .*flags=SECOND_PROMPT.*osc133=8,0,0,0,0'

# A marker-only line must survive resizing.
start_pane_history empty 8 20 \
    "${prompt}\033]133;C\007abcdefgh\n\033]133;D;3\007"
$TMUX resize-window -t empty: -x 4 || exit 1
$TMUX resize-window -t empty: -x 16 || exit 1
expected=$(printf 'abcdefgh\n_')
check_output empty "${expected%_}"
check_raw_matches empty \
    'flags=START_OUTPUT.*osc133=0,0,0,0,0' \
    'flags=END_OUTPUT.*0/0 osc133=0,0,0,0,3'

# UTF-8 cells use the same offsets as the text moved by reflow.
LC_ALL=C.UTF-8
export LC_ALL
wide=$(printf '\347\225\214a\347\225\214b\347\225\214c')
start_pane_history wide 80 20 \
    "${prompt}zzx\033]133;C\007${wide}\033]133;D;5\007${next}"
for width in 3 6; do
	$TMUX resize-window -t wide: -x "$width" || exit 1
	check_raw_matches wide \
	    "START_OUTPUT.*osc133=0,0,$((3 % width)),0,0" \
	    'END_OUTPUT.*osc133=0,0,0,[0-9]+,5'
	$TMUX resize-window -t wide: -x 80 || exit 1
	check_output wide "$wide"
done
check_selection wide "$wide"

# Unfinished output includes retained text beyond and below the cursor.
start_pane_history unfinished 80 20 "${prompt}\033]133;C\007abcdefgh\rxy"
check_output unfinished xycdefgh
check_selection unfinished xycdefgh
check_pipe unfinished xycdefgh

start_pane_history below 80 20 \
    "${prompt}\033]133;C\007first\n\nlast\033[2A\rXY"
expected=$(printf 'XYrst\n\nlast')
check_output below "$expected"
check_selection below "$expected"
check_pipe below "$expected"

exit $exit_status
