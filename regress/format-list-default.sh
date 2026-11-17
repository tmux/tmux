#!/bin/sh

# A list with default alignment is drawn like an explicitly left-aligned list.

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

$TMUX new-session -d -s inner -x 20 -y 8 'sleep 100' || exit 1
$TMUX set -g status-position top || exit 1
$TMUX2 new-session -d -s outer -x 20 -y 8 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

check '#[list=on]foo#[nolist]' foo
check 'pre#[list=on]middle#[nolist]post' premiddlepost

# The focus and trimming markers must also match explicit left alignment.
format='#[list=on]#[list=left-marker]<#[list=right-marker]>'
format=$format'#[list=on]0123456789#[list=focus]FOCUS'
format=$format'#[list=on]abcdefghij#[nolist]'
check "#[align=left]$format" '<3456789FOCUSabcdef>'
check "$format" '<3456789FOCUSabcdef>'

# Window ranges in the list still dispatch status bindings.
$TMUX set -g mouse on || exit 1
$TMUX set -g @clicked no || exit 1
$TMUX bind -n MouseDown1Status set -g @clicked yes || exit 1
check '#[list=on]#[range=window|0]foo#[norange]#[nolist]' foo
seq=$(printf '\033[<0;2;1M\033[<0;2;1m')
$TMUX2 send-keys -t outer:0.0 -l "$seq" || exit 1
i=0
while [ "$i" -lt 50 ]; do
	[ "$($TMUX show -gv @clicked)" = yes ] && exit 0
	sleep 0.1
	i=$((i + 1))
done
echo 'list window range did not receive click' >&2
exit 1
