#!/bin/sh

# Disable EOL following without changing cursor limits or selection rules.
PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Lsticky-eol-$$ -f/dev/null"

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
	default_eol=$1
	shift
	$TMUX set-window-option -g copy-mode-sticky-eol "$default_eol" ||
		fail "set copy-mode-sticky-eol failed"
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
[ "$($TMUX show -gwv copy-mode-sticky-eol)" = on ] ||
	fail "copy-mode-sticky-eol did not default to on"

for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"

	# The default remains sticky. Turning it off preserves the entry column.
	for direction in up down; do
		if [ "$direction" = up ]; then
			row=2
			end=20
		else
			row=4
			end=16
		fi
		[ "$mode" = emacs ] || end=$((end - 1))
		enter_with_default on
		check_cursor 8,3
		x "cursor-$direction"
		check_cursor "$end,$row"
		x cancel

		for entry in '' -e -H; do
			enter_with_default off $entry
			check_cursor 8,3
			x "cursor-$direction"
			check_cursor "8,$row"
			x cancel
		done
	done

	# Short lines retain the existing emacs and vi movement rules. Emacs
	# restores the preferred column; vi resumes from its last-character clamp.
	enter_with_default off
	x cursor-up
	check_cursor 8,2
	x cursor-up
	if [ "$mode" = emacs ]; then
		check_cursor 3,1
		restored=8
	else
		check_cursor 2,1
		restored=2
	fi
	x cursor-up
	check_cursor "$restored,0"
	x cancel

	enter_with_default off
	x cursor-down
	check_cursor 8,4
	x cursor-down
	if [ "$mode" = emacs ]; then
		check_cursor 2,5
		restored=8
	else
		check_cursor 1,5
		restored=1
	fi
	x cursor-down
	check_cursor "$restored,6"
	x cancel

	# Horizontal movement before the first vertical move replaces the initial
	# preference, rather than leaving the entry column permanently fixed.
	enter_with_default off
	x cursor-left
	check_cursor 7,3
	x cursor-up
	check_cursor 7,2
	x cancel
done

# Commands change the current visit, not the configured default.
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	if [ "$mode" = emacs ]; then
		end=20
		short=3
		restored=8
		expected=DEFGH
	else
		end=19
		short=2
		restored=2
		expected=DEFGHI
	fi

	enter_with_default on
	x sticky-eol-off
	x cursor-up
	check_cursor 8,2
	[ "$($TMUX show -gwv copy-mode-sticky-eol)" = on ] ||
		fail "sticky-eol-off changed the default"
	x cursor-up
	check_cursor "$short,1"
	x sticky-eol-off
	x cursor-up
	check_cursor "$restored,0"
	x cancel
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor "$end,2"
	x cancel

	enter_with_default off
	x sticky-eol-on
	x sticky-eol-on
	x cursor-up
	check_cursor "$end,2"
	[ "$($TMUX show -gwv copy-mode-sticky-eol)" = off ] ||
		fail "sticky-eol-on changed the default"
	x cancel
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor 8,2
	x cancel

	enter_with_default on
	x sticky-eol-toggle
	x cursor-up
	check_cursor 8,2
	x cancel
	enter_with_default off
	x sticky-eol-toggle
	x cursor-up
	check_cursor "$end,2"
	x cancel
	enter_with_default on
	x sticky-eol-toggle
	x sticky-eol-toggle
	x cursor-up
	check_cursor "$end,2"
	x cancel

	# Option changes apply only to the next visit, even after copy-mode again.
	enter_with_default on
	$TMUX set-window-option -g copy-mode-sticky-eol off ||
	    fail "set copy-mode-sticky-eol failed"
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor "$end,2"
	x cancel
	$TMUX copy-mode || fail "copy-mode failed"
	x cursor-up
	check_cursor 8,2
	x cancel

	# Toggling must not move the cursor or reshape an existing selection.
	enter_with_default off
	x history-top
	$TMUX send-keys -N3 -X cursor-right || fail "cursor-right failed"
	x begin-selection
	$TMUX send-keys -N5 -X cursor-right || fail "cursor-right failed"
	selection_format='#{selection_start_x},#{selection_start_y}'
	selection_format="$selection_format,#{selection_end_x},#{selection_end_y}"
	selection=$($TMUX display-message -p "$selection_format")
	for command in sticky-eol-on sticky-eol-off sticky-eol-toggle; do
		x "$command"
		check_cursor 8,0
		[ "$($TMUX display-message -p "$selection_format")" = "$selection" ] ||
			fail "$command changed the selection"
		x copy-selection-no-clear
		[ "$($TMUX show-buffer)" = "$expected" ] ||
			fail "$command changed the copied text"
	done
	x rectangle-on
	for command in sticky-eol-on sticky-eol-off sticky-eol-toggle; do
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
enter_with_default off
x cursor-left
x cursor-up
x cursor-up
check_cursor 3,1
enter_with_default off
check_cursor 3,1
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
				short=3
				last=0
			else
				row=4
				end=16
				short=2
				last=6
			fi
			[ "$mode" = emacs ] || end=$((end - 1))
			enter_with_default on
			x "cursor-$direction"
			check_cursor "$end,$row"
			x cancel

			enter_with_default off
			check_cursor 8,3
			x "cursor-$direction"
			check_cursor "8,$row"
			x "cursor-$direction"
			x "cursor-$direction"
			if [ "$mode" = emacs ]; then
				check_cursor "8,$last"
			else
				check_cursor "$((short - 1)),$last"
			fi
			x cancel
		done

		# Returning to the entry mark must not reenable EOL following.
		enter_with_default off
		x cursor-up
		check_cursor 8,2
		x jump-to-mark
		check_cursor 8,3
		enter_with_default on
		x cursor-up
		check_cursor 8,2
		x cancel

		# Changing the default does not change an already active visit.
		enter_with_default on
		enter_with_default off
		x cursor-up
		end=20
		[ "$mode" = emacs ] || end=19
		check_cursor "$end,2"
		x cancel
	done
done

# An empty entry line has a numeric column zero with EOL following off.
$TMUX respawn-pane -k \
    "printf '\033[H\033[2JABCDEFGHIJKLMNOPQRST\033[2;1H'; exec cat" ||
    fail "respawn-pane failed"
wait_cursor 0,1
for mode in emacs vi; do
	$TMUX set-window-option -g mode-keys "$mode" ||
	    fail "set mode-keys failed"
	enter_with_default off
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
	enter_with_default off
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
	enter_with_default off -u
	check_cursor 8,0
	x cancel
	enter_with_default off -d
	check_cursor 8,9
	x cancel

	# The same policy applies to page and half-page commands after entry.
	for command in page-up halfpage-up; do
		enter_with_default off
		x "$command"
		x page-up
		check_cursor 8,0
		x cancel
	done
	for command in page-down halfpage-down; do
		enter_with_default off
		x "$command"
		check_cursor 8,9
		x cancel
	done

	enter_with_default on -u
	end=20
	[ "$mode" = emacs ] || end=19
	check_cursor "$end,0"
	x cancel
	enter_with_default on -d
	end=16
	[ "$mode" = emacs ] || end=15
	check_cursor "$end,9"
	x cancel
done

exit 0
