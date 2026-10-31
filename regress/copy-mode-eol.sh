#!/bin/sh

# Keep the cursor column across short lines without padding copied text.
PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lkeep-column-$$ -f/dev/null"

fail()
{
	echo "$mode: $*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0 1 15

x()
{
	$TMUX send-keys -X "$@" || fail "copy command failed: $*"
}

check_cursor()
{
	actual=$($TMUX display-message -p '#{copy_cursor_x},#{copy_cursor_y}')
	[ "$actual" = "$1" ] || fail "expected cursor $1, got $actual"
}

enter_with_default()
{
	default_keep_column=$1
	shift
	$TMUX set-window-option -g copy-mode-keep-column "$default_keep_column" ||
		fail "set copy-mode-keep-column failed"
	$TMUX copy-mode "$@" || fail "copy-mode failed"
}

wait_cursor()
{
	i=0
	while [ "$i" -lt 50 ]; do
		actual=$($TMUX display-message -p '#{cursor_x},#{cursor_y}')
		[ "$actual" = "$1" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "pane cursor did not reach $1"
}

mode=emacs
$TMUX new-session -d -x40 -y10 \
    "printf '%s\r\n' ABCDEFGHIJKLMNOPQRST abc ABCDEFGHIJKLMNOPQRST \
	abcdefgh ABCDEFGHIJKLMNOP ab 01234567890123456789; \
	printf '\033[4;9H'; exec cat" ||
    fail "new-session failed"
$TMUX set-option -g status off || fail "set status failed"
$TMUX set-option -g window-size manual || fail "set window-size failed"
wait_cursor 8,3
[ "$($TMUX show -gwv copy-mode-keep-column)" = off ] ||
	fail "copy-mode-keep-column did not default to off"

for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"

	# The default follows line ends. Turning it on preserves the entry column.
	for direction in up down; do
		if [ "$direction" = up ]; then
			row=2
			end=20
		else
			row=4
			end=16
		fi
		[ "$mode" = emacs ] || end=$((end - 1))
		enter_with_default off
		check_cursor 8,3
		x "cursor-$direction"
		check_cursor "$end,$row"
		x cancel

		for entry in '' -e -H; do
			enter_with_default on $entry
			check_cursor 8,3
			x "cursor-$direction"
			check_cursor "8,$row"
			x cancel
		done
	done

	# Short lines must not clamp the cursor in either key mode.
	enter_with_default on
	x cursor-up
	check_cursor 8,2
	x cursor-up
	check_cursor 8,1
	x cursor-up
	check_cursor 8,0
	x cancel

	enter_with_default on
	x cursor-down
	check_cursor 8,4
	x cursor-down
	check_cursor 8,5
	x cursor-down
	check_cursor 8,6
	x cancel

	# Horizontal movement before the first vertical move replaces the initial
	# preference, rather than leaving the entry column permanently fixed.
	enter_with_default on
	x cursor-left
	check_cursor 7,3
	x cursor-up
	check_cursor 7,2
	x cancel

	# Explicit movement to EOL must replace a previous preferred column.
	end=8
	steps=2
	if [ "$mode" = vi ]; then
		end=7
		steps=1
	fi
	for command in end-of-line cursor-right; do
		for direction in up down; do
			enter_with_default on
			$TMUX send-keys -N2 -X cursor-left ||
			    fail "cursor-left failed"
			x cursor-up
			check_cursor 6,2
			x cursor-down
			check_cursor 6,3
			if [ "$command" = end-of-line ]; then
				x "$command"
			else
				$TMUX send-keys -N"$steps" -X "$command" ||
				    fail "$command failed"
			fi
			check_cursor "$end,3"
			x "cursor-$direction"
			row=2
			[ "$direction" = up ] || row=4
			check_cursor "$end,$row"
			x cancel
		done
	done

	# Explicit EOL moves back from a column beyond the line end.
	enter_with_default on
	x cursor-up
	x cursor-up
	short=3
	[ "$mode" = emacs ] || short=2
	check_cursor 8,1
	x end-of-line
	x cursor-up
	check_cursor "$short,0"
	x cancel

	# Moving left from column zero also chooses a new EOL column.
	enter_with_default on
	$TMUX send-keys -N2 -X cursor-left || fail "cursor-left failed"
	x cursor-up
	x cursor-down
	check_cursor 6,3
	x start-of-line
	x cursor-left
	end=20
	[ "$mode" = emacs ] || end=19
	check_cursor "$end,2"
	x cursor-down
	check_cursor "$end,3"
	x cursor-up
	check_cursor "$end,2"
	x cancel
done

# Commands change the current visit, not the configured default.
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	if [ "$mode" = emacs ]; then
		end=20
		expected=DEFGH
	else
		end=19
		expected=DEFGHI
	fi

	enter_with_default off
	x keep-column-on
	x cursor-up
	check_cursor 8,2
	[ "$($TMUX show -gwv copy-mode-keep-column)" = off ] ||
		fail "keep-column-on changed the default"
	x cursor-up
	check_cursor 8,1
	x keep-column-on
	x cursor-up
	check_cursor 8,0
	x cancel
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor "$end,2"
	x cancel

	enter_with_default on
	x keep-column-off
	x keep-column-off
	x cursor-up
	check_cursor "$end,2"
	[ "$($TMUX show -gwv copy-mode-keep-column)" = on ] ||
		fail "keep-column-off changed the default"
	x cancel
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor 8,2
	x cancel

	enter_with_default off
	x keep-column-toggle
	x cursor-up
	check_cursor 8,2
	x cancel
	enter_with_default on
	x keep-column-toggle
	x cursor-up
	check_cursor "$end,2"
	x cancel
	enter_with_default off
	x keep-column-toggle
	x keep-column-toggle
	x cursor-up
	check_cursor "$end,2"
	x cancel

	# Option changes apply only to the next visit, even after copy-mode again.
	enter_with_default off
	$TMUX set-window-option -g copy-mode-keep-column on ||
	    fail "set copy-mode-keep-column failed"
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor "$end,2"
	x cancel
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor 8,2
	x cancel

	# Toggling must not move the cursor or reshape an existing selection.
	enter_with_default on
	x history-top
	$TMUX send-keys -N3 -X cursor-right || fail "cursor-right failed"
	x begin-selection
	$TMUX send-keys -N5 -X cursor-right || fail "cursor-right failed"
	selection_format='#{selection_start_x},#{selection_start_y}'
	selection_format="$selection_format,#{selection_end_x},#{selection_end_y}"
	selection=$($TMUX display-message -p "$selection_format")
	for command in keep-column-off keep-column-on keep-column-toggle; do
		x "$command"
		check_cursor 8,0
		[ "$($TMUX display-message -p "$selection_format")" = "$selection" ] ||
			fail "$command changed the selection"
		x copy-selection-no-clear
		[ "$($TMUX show-buffer)" = "$expected" ] ||
			fail "$command changed the copied text"
	done
	x rectangle-on
	for command in keep-column-off keep-column-on keep-column-toggle; do
		x "$command"
		[ "$($TMUX display-message -p '#{rectangle_toggle}')" = 1 ] ||
			fail "$command disabled rectangle selection"
		x copy-selection-no-clear
		[ "$($TMUX show-buffer)" = "$expected" ] ||
			fail "$command changed rectangle copying"
	done
	x cancel
done

# Repeating copy-mode must not reinitialize the active visit.
mode=emacs
$TMUX set-window-option -g mode-keys "$mode" || fail "set mode-keys failed"
enter_with_default on
x cursor-left
x cursor-up
x cursor-up
check_cursor 7,1
enter_with_default on
check_cursor 7,1
x cursor-up
check_cursor 7,0
x cancel

# Trailing blanks must not replace the initial preference on the first move.
for entry_line in 'prompt> ' '        '; do
	$TMUX respawn-pane -k \
	    "printf '\033[H\033[2J'; \
		printf '%s\r\n' ABCDEFGHIJKLMNOPQRST abc ABCDEFGHIJKLMNOPQRST \
		    '$entry_line' ABCDEFGHIJKLMNOP ab 01234567890123456789; \
		printf '\033[4;9H'; exec cat" || fail "respawn-pane failed"
	wait_cursor 8,3
	for mode in emacs vi; do
		$TMUX set-window-option -g mode-keys "$mode" ||
		    fail "set mode-keys failed"
		for direction in up down; do
			if [ "$direction" = up ]; then
				row=2
				end=20
				last=0
			else
				row=4
				end=16
				last=6
			fi
			[ "$mode" = emacs ] || end=$((end - 1))
			enter_with_default off
			x "cursor-$direction"
			check_cursor "$end,$row"
			x cancel

			enter_with_default on
			check_cursor 8,3
			x "cursor-$direction"
			check_cursor "8,$row"
			x "cursor-$direction"
			x "cursor-$direction"
			check_cursor "8,$last"
			x cancel
		done

		# Returning to the entry mark must not reenable EOL following.
		enter_with_default on
		x cursor-up
		check_cursor 8,2
		x jump-to-mark
		check_cursor 8,3
		enter_with_default off
		x cursor-up
		check_cursor 8,2
		x cancel

		# Changing the default does not change an already active visit.
		enter_with_default off
		enter_with_default on
		x cursor-up
		end=20
		[ "$mode" = emacs ] || end=19
		check_cursor "$end,2"
		x cancel
	done
done

# An empty entry line starts at column zero with column preservation on.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2JABCDEFGHIJKLMNOPQRST\033[2;1H'; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 0,1
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on
	check_cursor 0,1
	x cursor-up
	check_cursor 0,0
	x cancel
done

# The initial preference counts terminal columns, not characters.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2J'; \
	printf '%s\r\n' ABCDEFGHIJKLMNOPQRST ABCDEFGHIJKLMNOPQRST; \
	printf 'abc中def'; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 8,2
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on
	check_cursor 8,2
	x cursor-up
	check_cursor 8,1
	x cancel
done

# Initialize before the optional entry-time page movement.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2JABCDEFGHIJKLMNOPQRST'; \
	printf '\033[9;1Hprompt> \033[10;1HABCDEFGHIJKLMNOP\033[9;9H'; \
	exec cat" ||
    fail "respawn-pane failed"
wait_cursor 8,8
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on -u
	check_cursor 8,0
	x cancel
	enter_with_default on -d
	check_cursor 8,9
	x cancel

	# The same policy applies to page and half-page commands after entry.
	for command in page-up halfpage-up; do
		enter_with_default on
		x "$command"
		x page-up
		check_cursor 8,0
		x cancel
	done
	for command in page-down halfpage-down; do
		enter_with_default on
		x "$command"
		check_cursor 8,9
		x cancel
	done

	enter_with_default off -u
	end=20
	[ "$mode" = emacs ] || end=19
	check_cursor "$end,0"
	x cancel
	enter_with_default off -d
	end=16
	[ "$mode" = emacs ] || end=15
	check_cursor "$end,9"
	x cancel
done

# Right-arrow movement preserves its EOL column on equal-length lines.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2J'; \
	printf '%s\r\n' ABCDEFGHIJKLMNOPQRST abcdefgh abcdefgh abcdefg \
	    ABCDEFGHIJKLMNOPQRST; printf '\033[2;4H'; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 3,1
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on
	end=8
	[ "$mode" = emacs ] || end=7
	$TMUX send-keys -N"$((end - 4))" -X cursor-right ||
	    fail "cursor-right failed"
	check_cursor "$((end - 1)),1"
	x cursor-up
	x cursor-down
	check_cursor "$((end - 1)),1"
	x cursor-right
	check_cursor "$end,1"
	x cursor-down
	check_cursor "$end,2"
	x cursor-down
	check_cursor "$end,3"
	x cursor-down
	check_cursor "$end,4"
	x cancel
done

# Nicm's long prompt must keep column 21 across the short command output.
$TMUX resize-window -x80 || fail "resize-window failed"
$TMUX respawn-pane -k \
    "printf '\033[H\033[2J'; printf '%s\r\n' \
	'01234567890123456789 echo foo; echo foo bar baz' foo 'foo bar baz' \
	'01234567890123456789 tmux set -g copy-mode-keep-column on'; \
	printf '01234567890123456789 '; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 21,4
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on
	check_cursor 21,4
	for row in 3 2 1 0; do
		x cursor-up
		check_cursor "21,$row"
	done

	# Character selections must not pad the short output lines with spaces.
	x begin-selection
	for row in 1 2; do
		x cursor-down
		check_cursor "21,$row"
	done
	x copy-selection-no-clear
	expected=$(printf 'echo foo; echo foo bar baz\nfoo\nfoo bar baz')
	[ "$($TMUX show-buffer)" = "$expected" ] ||
	    fail "column preservation padded copied text"
	for row in 3 4; do
		x cursor-down
		check_cursor "21,$row"
	done
	x cancel

	# Page movement must also keep the column on empty lines.
	enter_with_default on
	x page-down
	check_cursor 21,9
	x page-up
	check_cursor 21,1
	x halfpage-down
	check_cursor 21,6
	x halfpage-up
	check_cursor 21,1
	x cancel
done

# Swapping endpoints beyond EOL must preserve the selection and cursor column.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2Jfoo\r\nbar\033[1;9H'; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 8,0
for mode in vi emacs; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on
	x begin-selection
	x cursor-down
	check_cursor 8,1
	expected=$(printf '\nbar')
	x copy-selection-no-clear
	[ "$($TMUX show-buffer)" = "$expected" ] ||
	    fail "selection beyond EOL copied unexpected text"
	for row in 0 1 0 1; do
		x other-end
		x copy-selection-no-clear
		[ "$($TMUX show-buffer)" = "$expected" ] ||
		    fail "other-end changed the copied text"
		check_cursor "8,$row"
	done
	x cancel
done

# Cross the top of the viewport into history without losing the column.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2J'; i=0; while [ \$i -lt 20 ]; do \
	printf 'foo\r\n'; i=\$((i + 1)); done; \
	printf '01234567890123456789 '; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 21,9
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default on
	$TMUX send-keys -N15 -X cursor-up || fail "cursor-up failed"
	check_cursor 21,0
	[ "$($TMUX display-message -p '#{scroll_position}')" -gt 0 ] ||
	    fail "cursor did not scroll into history"
	$TMUX send-keys -N15 -X cursor-down || fail "cursor-down failed"
	check_cursor 21,9
	x cancel
done

exit 0
