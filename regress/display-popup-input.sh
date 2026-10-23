#!/bin/sh

# Input while a display-popup is open. Input after an explicit window or
# session switch is a separate command-queue scheduling issue.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
. ./regress-common.inc
FAILED=0

cleanup()
{
	stop_servers
}

trap cleanup 0 1 15

bad()
{
	echo "FAIL: $*" >&2
	FAILED=1
}

fmt()
{
	$TMUX display-message -p -t "$1" "$2"
}

screen()
{
	$TMUX2 capture-pane -p -t "$OUTER"
}

click()
{
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' "$1" "$2" "$1" "$2")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

click_status()
{
	line=$(screen | grep -n -- "$1" | tail -1)
	row=${line%%:*}
	text=${line#*:}
	before=${text%%"$1"*}
	click $((${#before} + 3)) "$row"
}

popup_shown()
{
	case "$(screen)" in
	*POPUP-HERE*) return 0 ;;
	esac
	return 1
}

setup()
{
	cleanup
	$TMUX new-session -d -s s -x 80 -y 24 -n zero 'cat' || exit 1
	$TMUX set -g mouse on
	$TMUX set -g status-right ''
	$TMUX split-window -h -t s:zero 'cat'
	$TMUX select-pane -t s:zero.0
	$TMUX new-window -d -t s:1 -n one 'cat'
	$TMUX bind P display-popup -w 30 -h 6 -E \
	    "sh -c 'printf POPUP-%s HERE; exec cat'"
	$TMUX2 new-session -d -x 80 -y 25 "$TMUX attach -t s" || exit 1
	$TMUX2 set -g status off
	sleep 1
	OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
	CLIENT=$($TMUX list-clients -F '#{client_name}' | head -1)
	LEFT=$(fmt s:zero.0 '#{pane_id}')
}

# Status click while a popup opened by a key binding is open.
setup
$TMUX2 send-keys -t "$OUTER" C-b P
sleep 1
popup_shown || bad "1a: popup did not open"
click_status "1:one"
[ "$(fmt s: '#{window_index}')" = 0 ] ||
	bad "1a: status click switched windows while popup was open"
$TMUX2 send-keys -t "$OUTER" C-c
sleep 1
[ "$(fmt s: '#{window_index}')" = 0 ] ||
	bad "1a: queued status click switched windows after popup closed"

# Click on another pane while a popup opened by a key binding is open.
setup
$TMUX2 send-keys -t "$OUTER" C-b P
sleep 1
popup_shown || bad "1b: popup did not open"
click 75 3
$TMUX2 send-keys -t "$OUTER" C-c
sleep 1
[ "$(fmt s:zero '#{pane_id}')" = "$LEFT" ] ||
	bad "1b: queued click selected another pane after popup closed"

# Wheel over the status line while a popup opened by a key binding is open.
setup
$TMUX2 send-keys -t "$OUTER" C-b P
sleep 1
popup_shown || bad "1c: popup did not open"
line=$(screen | grep -n -- "0:zero" | tail -1)
seq=$(printf '\033[<65;%s;%sM' 9 "${line%%:*}")
$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
sleep 1
[ "$(fmt s: '#{window_index}')" = 0 ] ||
	bad "1c: status wheel switched windows while popup was open"
$TMUX2 send-keys -t "$OUTER" C-c
sleep 1
[ "$(fmt s: '#{window_index}')" = 0 ] ||
	bad "1c: queued status wheel switched windows after popup closed"

# Status click while a popup opened by another client is open.
setup
$TMUX display-popup -c "$CLIENT" -w 30 -h 6 -E \
    "sh -c 'printf POPUP-%s HERE; exec cat'" &
sleep 1
popup_shown || bad "2: popup did not open"
click_status "1:one"
[ "$(fmt s: '#{window_index}')" = 0 ] ||
	bad "2: status click switched windows while popup was open"
popup_shown || bad "2: popup was left behind in window 0"

# Click inside a popup whose program has enabled SGR mouse reporting.
PROG=$(mktemp)
trap 'cleanup; rm -f "$PROG"' 0 1 15
printf '%s\n' "printf '\033[?1000h\033[?1006hPOPUP-%s' HERE" \
    'exec cat' >"$PROG"
setup
$TMUX bind M display-popup -w 30 -h 6 -E "sh $PROG"
$TMUX2 send-keys -t "$OUTER" C-b M
sleep 1
popup_shown || bad "4: popup did not open"
click 40 12
case "$(screen)" in
*'[<0;'*) ;;
*) bad "4: click inside popup did not reach its program" ;;
esac
$TMUX2 send-keys -t "$OUTER" C-c
sleep 1

# Control: a popup opened by another client already receives this click.
setup
$TMUX display-popup -c "$CLIENT" -w 30 -h 6 -E "sh $PROG" &
sleep 1
popup_shown || bad "4b: popup did not open"
click 40 12
case "$(screen)" in
*'[<0;'*) ;;
*) bad "4b: click inside externally opened popup did not reach program" ;;
esac

exit $FAILED
