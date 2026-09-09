#!/bin/sh

# Exercise the broad non-mouse copy-mode command surface: navigation, search,
# jumps, state toggles, line/end-of-line copies, append, and pipe variants.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

x()
{
	$TMUX send-keys -t copy:0 -X "$@" || fail "copy command failed: $*"
}

wait_mode()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t copy:0 '#{pane_in_mode}' 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "pane_in_mode is '$got', expected '$want'"
}

fresh()
{
	$TMUX copy-mode -t copy:0 || exit 1
	wait_mode 1
	x history-top
	x start-of-line
}

select_text()
{
	fresh
	x begin-selection
	x cursor-right
	x cursor-right
}

$TMUX new-session -d -s copy -x40 -y8 \
    "sh -c 'i=0; while [ \$i -lt 40 ]; do printf \"  line-%02d needle (a[b]c) end\\n\" \$i; i=\$((i + 1)); done; printf \"\\nparagraph two needle\\n\\nparagraph three\\n\"; printf \"\\033]133;A\\007prompt-one\\n\\033]133;C\\007output-one\\n\\033]133;A\\007prompt-two\\n\\033]133;C\\007output-two\\n\"; exec sleep 100'" ||
	exit 1
$TMUX set-option -g status off || exit 1
$TMUX set-option -g history-limit 200 || exit 1

i=0
while [ "$i" -lt 50 ]; do
	history=$($TMUX display-message -p -t copy:0 '#{history_size}')
	[ "$history" -ge 30 ] && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "history did not fill"

fresh

# Copy-mode-specific format callbacks expose the word, line, hyperlink and
# active search match at the cursor.
formats=$($TMUX display-message -p -t copy:0 \
    '#{copy_cursor_word}|#{copy_cursor_line}|#{copy_cursor_hyperlink}|#{search_match}') ||
	exit 1
[ -n "$formats" ] || fail "copy-mode cursor formats were empty"

# Cursor, viewport and paragraph commands. Each is asserted by successful
# dispatch and the final mode-liveness check; detailed word/selection geometry
# is covered by the existing copy-mode tests.
for command in back-to-indentation bottom-line top-line middle-line \
    cursor-centre-vertical cursor-centre-horizontal end-of-line history-bottom \
    history-top halfpage-down halfpage-up page-down page-up next-paragraph \
    previous-paragraph next-matching-bracket previous-matching-bracket; do
	x "$command"
done
x goto-line 15
x set-mark
x cursor-down
x jump-to-mark

# OSC 133 prompt and output markers support shell-integration navigation.
x history-top
x next-prompt
case "$($TMUX display-message -p -t copy:0 '#{copy_cursor_line}')" in
*prompt-one*) ;;
*) fail "next-prompt did not find prompt-one" ;;
esac
x next-prompt
x previous-prompt
case "$($TMUX display-message -p -t copy:0 '#{copy_cursor_line}')" in
*prompt-one*) ;;
*) fail "previous-prompt did not return to prompt-one" ;;
esac
x next-prompt -o
case "$($TMUX display-message -p -t copy:0 '#{copy_cursor_line}')" in
*output-one*) ;;
*) fail "next-prompt -o did not find output-one" ;;
esac

# Character jumps remember their direction and character for repeat/reverse.
x jump-forward e
x jump-again
x jump-reverse
x jump-backward e
x jump-to-forward e
x jump-to-backward e

# Direct, text and incremental searches cover both directions and the saved
# search used by again/reverse.
x search-forward needle
x search-again
x search-reverse
x search-backward needle
x search-forward-text needle
x search-backward-text needle
x search-forward-incremental '=needle'
x search-forward-incremental '+needle'
x search-backward-incremental '=needle'
x search-backward-incremental '-needle'

# Stateful display and behaviour toggles.
for command in rectangle-on rectangle-off rectangle-toggle \
    line-numbers-on line-numbers-off line-numbers-toggle \
    refresh-on refresh-now refresh-off refresh-toggle \
    scroll-exit-on scroll-exit-off scroll-exit-toggle toggle-position; do
	x "$command"
done
wait_mode 1
x cancel
wait_mode 0

# Line and end-of-line copy commands work without an explicit selection.
fresh
x select-line
x copy-selection-and-cancel
case "$($TMUX show-buffer)" in
*line-00*) ;;
*) fail "select-line did not select the current line" ;;
esac

fresh
x copy-line
[ -n "$($TMUX show-buffer)" ] || fail "copy-line produced an empty buffer"
x cancel

fresh
x copy-end-of-line
[ -n "$($TMUX show-buffer)" ] || fail "copy-end-of-line produced an empty buffer"
x cancel

# Append variants retain or close the mode as advertised.
$TMUX set-buffer -b append-buffer prefix || exit 1
select_text
x append-selection
appended=$($TMUX show-buffer -b append-buffer)
case "$appended" in
prefix*) ;;
*) fail "append-selection produced '$appended'" ;;
esac
x cancel

$TMUX set-buffer -b append-cancel-buffer prefix || exit 1
select_text
x append-selection-and-cancel
wait_mode 0
appended=$($TMUX show-buffer -b append-cancel-buffer)
case "$appended" in
prefix*) ;;
*) fail "append-selection-and-cancel produced '$appended'" ;;
esac

# The explicit cancel variants close copy mode.
fresh
x copy-line-and-cancel
wait_mode 0
fresh
x copy-end-of-line-and-cancel
wait_mode 0
select_text
x copy-selection-and-cancel
wait_mode 0

# Pipe wrappers have distinct command handlers. /dev/null keeps the test
# deterministic while exercising asynchronous pipe job creation and cleanup.
fresh
x copy-pipe-line 'cat >/dev/null'
x cancel
fresh
x copy-pipe-line-and-cancel 'cat >/dev/null'
wait_mode 0
fresh
x copy-pipe-end-of-line 'cat >/dev/null'
x cancel
fresh
x copy-pipe-end-of-line-and-cancel 'cat >/dev/null'
wait_mode 0

select_text
x copy-pipe-no-clear 'cat >/dev/null'
x pipe-no-clear 'cat >/dev/null'
x copy-pipe 'cat >/dev/null'
x pipe 'cat >/dev/null'
x cancel
select_text
x copy-pipe-and-cancel 'cat >/dev/null'
wait_mode 0
select_text
x pipe-and-cancel 'cat >/dev/null'
wait_mode 0

# Commands which cancel on downward movement have separate handlers.
fresh
x history-bottom
x bottom-line
x cursor-down-and-cancel
wait_mode 0
fresh
x history-bottom
x halfpage-down-and-cancel
wait_mode 0
fresh
x history-bottom
x page-down-and-cancel
wait_mode 0
fresh
x history-bottom
x bottom-line
x scroll-down-and-cancel
wait_mode 0

$TMUX has-session -t copy || fail "server died during copy-mode commands"
exit 0
