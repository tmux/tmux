#!/bin/sh

# Preserve a preferred column while clamping the cursor to shorter lines.
PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -f/dev/null -LtestA$$"
OUT=$(mktemp -d) || exit 1

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -f "$OUT/actual" "$OUT/expected"
	rmdir "$OUT"
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

fail()
{
	echo "$*" >&2
	exit 1
}

send()
{
	$TMUX send-keys -X "$@" || exit 1
}

check_cursor()
{
	actual=$($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')
	[ "$actual" = "$1" ] || fail "$mode: expected cursor $1, got $actual"
}

check_buffer()
{
	printf '%s' "$1" >"$OUT/expected"
	$TMUX save-buffer "$OUT/actual" || exit 1
	if ! cmp -s "$OUT/expected" "$OUT/actual"; then
		od -An -tx1 "$OUT/expected" "$OUT/actual"
		fail "$mode: incorrect copied text"
	fi
}

start()
{
	$TMUX copy-mode || exit 1
	send history-top
}

check_eol_up()
{
	$TMUX send -N3 -X cursor-down || exit 1
	send end-of-line
	$TMUX send -N3 -X cursor-up || exit 1
	check_cursor "$1,0"
}

$TMUX new -d -x40 -y10 \
	"printf '%s\n' abcdefghijklmnopqrst abc '' abcdefghij \
	    abcdefghijklmnopqrst; exec sleep 300" || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g window-size manual || exit 1

# Wait for the fixture before entering copy mode.
i=0
while [ "$($TMUX capture-pane -p | sed -n 1p)" != abcdefghijklmnopqrst ]; do
	i=$((i + 1))
	[ "$i" -lt 100 ] || fail 'fixture did not appear'
	sleep 0.01
done
[ "$($TMUX show -gwv copy-mode-sticky-eol)" = on ] ||
	fail 'option defaults to off'

for mode in emacs vi; do
	$TMUX set -g mode-keys "$mode" || exit 1
	if [ "$mode" = vi ]; then
		end=9
		short=2
		follow=2
		suffix='
'
	else
		end=10
		short=3
		follow=0
		suffix=
	fi

	# With the option on, end-of-line retains the existing behavior.
	$TMUX set -g copy-mode-sticky-eol on || exit 1
	start
	check_eol_up "$follow"
	send cancel

	# Commands override the default only for the current visit.
	start
	send sticky-eol-off
	check_eol_up "$end"
	[ "$($TMUX show -gwv copy-mode-sticky-eol)" = on ] ||
		fail 'sticky-eol-off changed the option'
	$TMUX copy-mode || exit 1
	send history-top
	check_eol_up "$end"
	send cancel
	start
	check_eol_up "$follow"
	send cancel

	start
	send sticky-eol-toggle
	check_eol_up "$end"
	send sticky-eol-toggle
	send history-top
	check_eol_up "$follow"
	send cancel

	$TMUX set -g copy-mode-sticky-eol off || exit 1
	start
	send sticky-eol-on
	check_eol_up "$follow"
	[ "$($TMUX show -gwv copy-mode-sticky-eol)" = off ] ||
		fail 'sticky-eol-on changed the option'
	send cancel
	start
	check_eol_up "$end"
	send cancel

	start
	send sticky-eol-toggle
	check_eol_up "$follow"
	send sticky-eol-toggle
	send history-top
	check_eol_up "$end"
	send cancel

	# Changing the default does not change an existing visit.
	$TMUX set -g copy-mode-sticky-eol on || exit 1
	start
	$TMUX set -g copy-mode-sticky-eol off || exit 1
	check_eol_up "$follow"
	send cancel
	start
	check_eol_up "$end"
	send cancel

	$TMUX set -g copy-mode-sticky-eol off || exit 1
	start
	$TMUX send -N3 -X cursor-down || exit 1
	send end-of-line
	check_cursor "$end,3"
	send cursor-up
	check_cursor '0,2'
	# Repeating sticky-eol-off must not discard the remembered column.
	send sticky-eol-off
	send cursor-up
	check_cursor "$short,1"
	send cursor-up
	check_cursor "$end,0"
	$TMUX send -N3 -X cursor-down || exit 1
	check_cursor "$end,3"

	# An explicit end-of-line on a clamped row chooses a new column.
	$TMUX send -N2 -X cursor-up || exit 1
	send end-of-line
	send cursor-up
	check_cursor "$short,0"
	send cancel

	# Horizontal movement on a clamped row chooses its visible column.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send cursor-down
	send cursor-left
	send cursor-up
	check_cursor "$((short - 1)),0"
	send cancel

	# Changing state chooses the visible column, not an old clamped goal.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send begin-selection
	send cursor-down
	send sticky-eol-on
	send sticky-eol-off
	send cursor-up
	check_cursor "$short,0"
	send copy-selection-no-clear
	if [ "$mode" = vi ]; then
		check_buffer cdefghi
	else
		check_buffer defgh
	fi
	send cancel

	# Enabling sticky EOL mid-line must not use a stale line length.
	start
	$TMUX send -N4 -X cursor-down || exit 1
	$TMUX send -N8 -X cursor-right || exit 1
	send sticky-eol-on
	send sticky-eol-on
	send cursor-up
	check_cursor '8,3'
	send cancel

	# Start-of-line resets the column even on an already clamped empty row.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	$TMUX send -N2 -X cursor-down || exit 1
	send start-of-line
	$TMUX send -N2 -X cursor-up || exit 1
	check_cursor '0,0'
	send cancel

	# Short and empty rows keep their line breaks in the selection.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send begin-selection
	$TMUX send -N2 -X cursor-down || exit 1
	check_cursor '0,2'
	send copy-selection-no-clear
	check_buffer "ijklmnopqrst
abc
$suffix"
	# Selection operations on a clamped row must retain the preferred column.
	send cursor-down
	check_cursor '8,3'
	send cancel

	# Switching selection ends uses the existing clamped endpoint rules.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send begin-selection
	send cursor-down
	send copy-selection-no-clear
	check_buffer "ijklmnopqrst
abc$suffix"
	send other-end
	send other-end
	send copy-selection-no-clear
	check_buffer 'ijklmnopqrst
abc'
	send cancel

	# Returning from rectangle mode must not revive a stale preferred column.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send begin-selection
	send cursor-down
	send rectangle-on
	send rectangle-off
	send cursor-up
	check_cursor "$short,0"
	send cancel

	# Starting on a short row must not copy its last character in vi mode.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send cursor-down
	send begin-selection
	$TMUX send -N2 -X cursor-down || exit 1
	send copy-selection-no-clear
	if [ "$mode" = vi ]; then
		check_buffer '

abcdefghi'
	else
		check_buffer '

abcdefgh'
	fi
	send cancel

	# Rectangle copying retains empty rows without padding them with spaces.
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send cursor-down
	send rectangle-toggle
	send begin-selection
	$TMUX send -N2 -X cursor-down || exit 1
	send copy-selection-no-clear
	check_buffer '

d'
	send cancel

	# Page movement also restores the preferred column after empty rows.
	$TMUX new-window \
		"i=0
		while [ \$i -lt 30 ]; do
			printf 'abcdefghijklmnopqrst\nabc\n\n'
			i=\$((i + 1))
		done
		printf '\033[9G'
		exec sleep 300" || exit 1
	i=0
	while [ "$($TMUX display -p '#{cursor_x}')" != 8 ]; do
		i=$((i + 1))
		[ "$i" -lt 100 ] || fail 'history did not appear'
		sleep 0.01
	done

	# The default applies before movement inside the entry command.
	for entry in '' -u -d -e; do
		$TMUX copy-mode $entry || exit 1
		i=0
		while [ "$($TMUX display -p '#{copy_cursor_line}')" != \
		    abcdefghijklmnopqrst ]; do
			send cursor-up
			i=$((i + 1))
			[ "$i" -lt 4 ] || fail "$entry: long row not found"
		done
		[ "$($TMUX display -p '#{copy_cursor_x}')" = 8 ] ||
			fail "$mode $entry: entry lost the cursor column"
		send cancel
	done

	start
	$TMUX send -N8 -X cursor-right || exit 1
	send page-down
	check_cursor '0,0'
	send page-up
	check_cursor '8,0'
	send halfpage-down
	check_cursor '0,0'
	send halfpage-up
	check_cursor '8,0'
	send cancel

	# A wrapped row joins the following row without an extra line break.
	$TMUX new-window \
		"printf '%s\n' abcdefghijklmnopqrstuvwxyzABCD abc \
		    abcdefghijklmnopqrst; exec sleep 300" || exit 1
	$TMUX resize-window -x20 || exit 1
	i=0
	while [ "$($TMUX capture-pane -p -S- | sed -n 1p)" != \
	    abcdefghijklmnopqrst ]; do
		i=$((i + 1))
		[ "$i" -lt 100 ] || fail 'wrapped fixture did not appear'
		sleep 0.01
	done
	start
	$TMUX send -N8 -X cursor-right || exit 1
	send begin-selection
	$TMUX send -N2 -X cursor-down || exit 1
	send copy-selection-no-clear
	check_buffer "ijklmnopqrstuvwxyzABCD
abc$suffix"
	send cancel

	# End-of-line on wrapped text chooses the end of the logical line.
	start
	send end-of-line
	if [ "$mode" = vi ]; then
		check_cursor '9,1'
	else
		check_cursor '10,1'
	fi
	send cursor-down
	check_cursor "$short,2"
	send cursor-down
	if [ "$mode" = vi ]; then
		check_cursor '9,3'
	else
		check_cursor '10,3'
	fi
	send cancel

	# Restore the first window and its original width for the next mode.
	$TMUX select-window -t:0 || exit 1
	$TMUX resize-window -x40 || exit 1
done

# Wide characters must not turn a preferred cell column into a character count.
$TMUX new-window \
	"printf 'ab界defghijklmnop\nabc\nabcdefghij\n'; exec sleep 300" || exit 1
i=0
while [ "$($TMUX capture-pane -p | sed -n 1p)" != ab界defghijklmnop ]; do
	i=$((i + 1))
	[ "$i" -lt 100 ] || fail 'wide-character fixture did not appear'
	sleep 0.01
done
for mode in emacs vi; do
	$TMUX set -g mode-keys "$mode" || exit 1
	if [ "$mode" = vi ]; then
		short=2
	else
		short=3
	fi
	start
	$TMUX send -N2 -X cursor-down || exit 1
	$TMUX send -N8 -X cursor-right || exit 1
	send begin-selection
	send cursor-up
	check_cursor "$short,1"
	send cursor-up
	check_cursor '8,0'
	send copy-selection-no-clear
	if [ "$mode" = vi ]; then
		check_buffer 'hijklmnop
abc
abcdefghi'
	else
		check_buffer 'hijklmnop
abc
abcdefgh'
	fi
	send cancel
done
