#!/bin/sh

# Mouse events distinguish status areas and retain the drag's origin.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

# Stop both isolated servers.
cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
}
trap cleanup 0
trap 'exit 1' 1 2 15

# Report the layout and failed check.
fail()
{
	echo "$position/$side: $*" >&2
	exit 1
}

# Send a one-based SGR mouse report through the attached client.
mouse()
{
	seq=$(printf '\033[<%s;%s;%s%s' "$1" "$2" "$3" "$4")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || fail 'send mouse failed'
	sleep 0.05
}

# Check the current status row and dispatched binding.
check()
{
	got=$($TMUX show -gv @status)
	[ "$got" = "$1" ] || fail "got '$got', expected '$1'"
	if [ "$#" -eq 2 ]; then
		got=$($TMUX show -gv @location)
		[ "$got" = "$2" ] || fail "binding '$got', expected '$2'"
	fi
}

# Check formats recorded by a click at a terminal position.
click()
{
	$TMUX set -g @status unset || exit 1
	mouse 0 "$1" "$2" M
	mouse 0 "$1" "$2" m
	check "$3" "$4"
}

$TMUX new-session -d -s inner -x 60 -y 20 'sleep 180' || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status 2 || exit 1
$TMUX set -g 'status-format[0]' '#[range=window|0]ITEM#[norange]' || exit 1
$TMUX set -g 'status-format[1]' '' || exit 1
$TMUX set -g side-status-format '#[range=window|0]ITEM#[norange]' || exit 1
$TMUX bind p set -g @prefix-kept yes || exit 1
format='<#{mouse_status_line}>'
for location in Pane Status StatusLeft StatusRight StatusDefault \
    SideStatus SideStatusLeft SideStatusRight SideStatusDefault \
    SideStatusBorder Control0 Control9 SideStatusControl0 SideStatusControl9; do
	for event in MouseDown SecondClick TripleClick MouseDrag \
	    MouseDragEnd; do
		$TMUX bind -n "${event}1$location" \
		    "set -gF @status '$format'; \
		    set -g @location '$location'" || exit 1
	done
done
$TMUX2 new-session -d -s outer -x 60 -y 20 'sleep 180' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

# Mouse formats are empty without a mouse event.
got=$($TMUX display -p '<#{mouse_status_line}>')
[ "$got" = '<>' ] || fail "format outside a mouse binding: '$got'"

for position in top bottom; do
	$TMUX set -g status-position "$position" || exit 1
	item_row=19 background_row=20 offset=0
	if [ "$position" = top ]; then
		item_row=1 background_row=2 offset=2
	fi
	for side in off left right; do
		$TMUX set -g side-status "$side" || exit 1
		sleep 0.3
		click 2 "$item_row" '<0>' Status
		click 30 "$background_row" '<1>' StatusDefault
		click 30 10 '<>' Pane
		[ "$side" = off ] && continue

		column=2 border=14
		[ "$side" = right ] && column=49 border=47
		click "$column" "$((offset + 1))" '<0>' SideStatus
		click "$column" "$((offset + 2))" '<1>' SideStatusDefault
		click "$border" 10 "<$((9 - offset))>" SideStatusBorder

		# Unbound side motion must not consume a pending prefix.
		for button in 35 51; do
			$TMUX set -gu @prefix-kept || exit 1
			$TMUX2 send-keys -t outer:0.0 C-b || exit 1
			mouse "$button" "$column" 10 M
			$TMUX2 send-keys -t outer:0.0 p || exit 1
			sleep 0.1
			[ "$($TMUX show -qgv @prefix-kept)" = yes ] ||
			    fail 'side motion consumed the prefix'
		done

		# The first motion leaves horizontal status for the side area.
		# The binding stays horizontal while the row follows the pointer.
		mouse 0 30 "$background_row" M
		mouse 32 "$column" 10 M
		check "<$((9 - offset))>" StatusDefault
		mouse 32 30 10 M
		check '<>' StatusDefault
		mouse 0 30 10 m
		check '<>'
		click 30 10 '<>' Pane

		# The reverse drag stays a side event across horizontal status.
		mouse 0 "$column" 10 M
		mouse 32 30 "$background_row" M
		check '<1>' SideStatusDefault
		mouse 32 30 10 M
		check '<>' SideStatusDefault
		mouse 0 30 10 m
		check '<>'
		click 30 10 '<>' Pane

		# Horizontal controls retain ControlN; side ranges have their own keys.
		for range in left right 'control|0' 'control|9'; do
			case "$range" in
			left) suffix=StatusLeft ;;
			right) suffix=StatusRight ;;
			'control|0') suffix=Control0 ;;
			'control|9') suffix=Control9 ;;
			esac
			$TMUX set -g 'status-format[0]' \
			    "#[range=$range]ITEM#[norange]" || exit 1
			$TMUX set -g side-status-format \
			    "#[range=$range]ITEM#[norange]" || exit 1
			sleep 0.1
			click 2 "$item_row" '<0>' "$suffix"
			case "$suffix" in
			Control*) suffix=SideStatus$suffix ;;
			*) suffix=Side$suffix ;;
			esac
			click "$column" "$((offset + 1))" '<0>' "$suffix"
		done
		$TMUX set -g 'status-format[0]' \
		    '#[range=window|0]ITEM#[norange]' || exit 1
		$TMUX set -g side-status-format \
		    '#[range=window|0]ITEM#[norange]' || exit 1
	done
done

exit 0
