#!/bin/sh

# Status coordinates and session/window targets on either side, with the
# horizontal status off, above or below the window.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0 1 15

click()
{
	expected="$3|$(($1 - 1))|$(($2 - 1))"
	$TMUX set -g @mouse none || exit 1
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' "$1" "$2" "$1" "$2")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX show -gv @mouse)
		[ "$got" = "$expected" ] && break
		sleep 0.1
		i=$((i + 1))
	done
	[ "$got" = "$expected" ] || {
		echo "$side/$status at $1,$2: '$got', expected '$expected'" >&2
		exit 1
	}
	sleep 0.5
}

$TMUX new-session -d -s inner -n zero -x 80 -y 20 'sleep 100' || exit 1
$TMUX new-window -d -n one 'sleep 100' || exit 1
$TMUX new-session -d -s beta -n other -x 100 -y 30 'sleep 100' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g automatic-rename off || exit 1
$TMUX set -g side-status-width 14 || exit 1
[ "$($TMUX display -p '#{mouse_absolute_x}|#{mouse_absolute_y}')" = '|' ] ||
    exit 1
$TMUX set -g 'status-format[0]' \
    '#[range=window|1]window#[norange]' || exit 1
$TMUX set -g 'status-format[1]' 'second' || exit 1
format='#{mouse_status_range}|#{mouse_status_line}|#{mouse_x}|#{mouse_y}'
format=$format'|#{session_name}|#{window_name}'
format=$format'|#{mouse_absolute_x}|#{mouse_absolute_y}'
for key in MouseDown1Status MouseDown1StatusDefault MouseDown1SideStatus \
    MouseDown1SideStatusDefault MouseDown1SideStatusBorder MouseDown1Pane; do
	$TMUX bind -n "$key" set -gF @mouse "$key|$format" || exit 1
done
$TMUX2 new-session -d -s outer -x 80 -y 20 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

for side in left right; do
	$TMUX set -g side-status "$side" || exit 1
	if [ "$side" = left ]; then
		column=3
		separator=14
		pane_column=17
		separator_x=13
	else
		column=69
		separator=67
		pane_column=3
		separator_x=0
	fi
	for status in off top bottom; do
		offset=0
		if [ "$status" = off ]; then
			$TMUX set -g status off || exit 1
		else
			$TMUX set -g status 2 || exit 1
			$TMUX set -g status-position "$status" || exit 1
			[ "$status" = top ] && offset=2
		fi
		sleep 0.5

		# The current session's windows are between the session headers.
		click "$column" "$((offset + 1))" \
		    'MouseDown1SideStatus|session|0|2|0|inner|zero'
		click "$((column + 3))" "$((offset + 3))" \
		    'MouseDown1SideStatus|window|2|5|2|inner|one'
		click "$column" "$((offset + 4))" \
		    'MouseDown1SideStatus|session|3|2|3|beta|other'
		click "$separator" "$((offset + 6))" \
		    "MouseDown1SideStatusBorder||5|$separator_x|5|inner|zero"
		click "$pane_column" "$((offset + 7))" \
		    'MouseDown1Pane|||2|6|inner|zero'

		click "$column" "$((offset + 8))" \
		    'MouseDown1SideStatusDefault||7|2|7|inner|zero'

		# Horizontal status coordinates are relative to its first line,
		# even if the clicked window's pane extends behind the status.
		if [ "$status" != off ]; then
			line=1
			[ "$status" = bottom ] && line=19
			click 3 "$line" \
			    'MouseDown1Status|window|0|2|0|inner|one'
			click 3 "$((line + 1))" \
			    'MouseDown1StatusDefault||1|2|1|inner|zero'
		fi
	done
done

exit 0
