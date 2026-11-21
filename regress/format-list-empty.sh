#!/bin/sh

# Text before and after an empty or completely trimmed list is drawn with
# the selected alignment, including mouse ranges in the surviving text.

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

check()
{
	$TMUX set -g 'status-format[0]' "$1" || exit 1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX2 capture-pane -p -t outer:0.0 | head -n 1)
		[ "$got" = "$2" ] && return
		sleep 0.1
		i=$((i + 1))
	done
	echo "format '$1': got '$got', expected '$2'" >&2
	exit 1
}

click()
{
	$TMUX set -g @clicked none || exit 1
	seq=$(printf '\033[<0;%s;1M\033[<0;%s;1m' "$1" "$1")
	$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX show -gv @clicked)
		[ "$got" = "$2" ] && break
		sleep 0.1
		i=$((i + 1))
	done
	[ "$got" = "$2" ] || {
		echo "column $1: range '$got', expected '$2'" >&2
		exit 1
	}
	sleep 0.5
}

$TMUX new-session -d -s inner -x 20 -y 8 'sleep 100' || exit 1
$TMUX set -g status-position top || exit 1
$TMUX set -g mouse on || exit 1
$TMUX bind -n MouseDown1Status \
    set -gF @clicked '#{mouse_status_range}' || exit 1
$TMUX2 new-session -d -s outer -x 20 -y 8 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

for align in left centre right; do
	case "$align" in
	left) padding=0 ;;
	centre) padding=7 ;;
	right) padding=13 ;;
	esac
	want="$(printf '%*s' "$padding" '')prepost"
	format="#[align=$align,range=user|pre]pre#[norange]"
	format=$format'#[list=on]#[nolist]'
	format=$format'#[range=user|post]post#[norange]'
	check "$format" "$want"
	click "$((padding + 1))" pre
	click "$((padding + 4))" post

	# The list has text and a range, but no room remains for it.
	format="#[align=$align,range=user|pre]0123456789#[norange]"
	format=$format'#[list=on]#[range=user|hidden]'
	format=$format'HHHHHHHHHHHHHHHHHHHH#[norange]#[nolist]'
	format=$format'#[range=user|post]abcdefghij#[norange]'
	check "$format" 0123456789abcdefghij
	click 1 pre
	click 11 post
done

exit 0
