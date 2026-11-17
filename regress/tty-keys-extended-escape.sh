#!/bin/sh

# An Escape followed by an extended key must remain a separate key. The
# sequence's leading Escape must not be consumed as the second half of
# M-Escape.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 15

$INNER new-session -d -s inner -x80 -y10 "stty raw -echo; exec cat -v" ||
    exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g assume-paste-time 0 || exit 1
$INNER set-option -s escape-time 1000 || exit 1
$INNER set-option -s extended-keys on || exit 1
$INNER unbind-key -a -T root || exit 1
$OUTER new-session -d -s outer -x80 -y10 "$INNER attach -t inner" || exit 1
$OUTER set-option -g assume-paste-time 0 || exit 1

i=0
while [ -z "$($INNER list-clients -F '#{client_name}')" ]; do
	[ "$i" -lt 100 ] || fail 'inner client did not attach'
	sleep 0.05
	i=$((i + 1))
done

bind_key()
{
	$INNER bind-key -n "$1" send-keys -l "$2" || exit 1
}
bind_key Escape E
bind_key M-Escape A
bind_key C-a C
bind_key M-C-a M
bind_key M-Up U
bind_key M-DC D

expected_output=

send_raw()
{
	# Hex arguments avoid command separators in a partial sequence.
	bytes=$(printf '%s' "$1" | od -An -v -tx1)
	$OUTER send-keys -H $bytes || exit 1
}

assert_events()
{
	name=$1
	expected_output=$expected_output$2
	send_raw "$3"
	if [ "$#" -eq 4 ]; then
		sleep 0.05
		send_raw "$4"
	fi
	i=0
	while [ "$i" -lt 100 ]; do
		actual=$($INNER capture-pane -p | tr -d '\n')
		[ "$actual" = "$expected_output" ] && return 0
		sleep 0.05
		i=$((i + 1))
	done
	fail "$name: got '$actual', expected '$expected_output'"
}

assert_events 'CSI u' C "$(printf '\033[97;5u')"
assert_events 'Escape and CSI u' EC "$(printf '\033\033[97;5u')"
assert_events 'Escape and xterm' EC "$(printf '\033\033[27;5;97~')"
assert_events 'Escape and CSI u with Meta' EM "$(printf '\033\033[97;7u')"
assert_events 'split CSI u' EC "$(printf '\033\033[97;')" '5u'
assert_events 'split xterm' EC "$(printf '\033\033[27;5')" ';97~'
assert_events 'CSI u split after two Escapes' EC "$(printf '\033\033')" '[97;5u'
assert_events 'xterm split after two Escapes' EC "$(printf '\033\033')" '[27;5;97~'
assert_events 'M-Escape' A "$(printf '\033\033')"
assert_events 'M-Up' U "$(printf '\033\033[A')"
assert_events 'M-DC' D "$(printf '\033\033[3~')"

exit 0
