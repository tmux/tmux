#!/bin/sh

# A mouse report split over a slow connection must not become keys when the
# rest arrives after escape-time. Short prefixes that are also keys, such as
# M-[, must still be recognized after escape-time.

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

$INNER new-session -d -s inner -x80 -y10 \
    "stty raw -echo; printf '\033[?1003h\033[?1006h'; exec cat -v" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g mouse on || exit 1
$INNER set-option -g assume-paste-time 0 || exit 1
$INNER set-option -s escape-time 10 || exit 1
$INNER unbind-key -a -T root || exit 1
$OUTER new-session -d -s outer -x80 -y10 "$INNER attach -t inner" || exit 1
$OUTER set-option -g assume-paste-time 0 || exit 1

i=0
while [ -z "$($INNER list-clients -F '#{client_name}')" ]; do
	[ "$i" -lt 100 ] || fail 'inner client did not attach'
	sleep 0.05
	i=$((i + 1))
done

# The outer tmux has no client to answer the inner client's colour queries.
# Pending queries also increase the delay, so wait for them to time out.
sleep 6

bind_key()
{
	$INNER bind-key -n "$1" send-keys -l "$2" || exit 1
}
bind_key Escape E
bind_key M-[ B

expected_output=

send_raw()
{
	# Hex arguments avoid command separators in a partial mouse report.
	bytes=$(printf '%s' "$1" | od -An -v -tx1)
	$OUTER send-keys -H $bytes || exit 1
}

assert_events()
{
	name=$1
	expected_output=$expected_output$2
	send_raw "$3"
	if [ "$#" -eq 4 ]; then
		sleep 0.2
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

assert_events 'slow SGR report' '^[[<35;3;3M' "$(printf '\033[<35;')" '3;3M'
assert_events 'slow SGR prefix' '^[[<35;3;3M' "$(printf '\033[<')" '35;3;3M'
assert_events 'slow X10 report' '^[[MC##' "$(printf '\033[MC')" '##'
assert_events 'Escape and slow report' 'E^[[<35;3;3M' \
    "$(printf '\033\033[<35;')" '3;3M'
assert_events 'M-[' B "$(printf '\033[')"
assert_events 'Escape' E "$(printf '\033')"

exit 0
