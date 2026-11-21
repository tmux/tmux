#!/bin/sh

# An Escape followed by a focus report must remain a separate key. The
# report's leading Escape must not be consumed as the second half of M-Escape,
# and the focus change must still reach the client and the pane.

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
    "printf '\033[?1004h'; stty raw -echo; exec cat -v" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g assume-paste-time 0 || exit 1
$INNER set-option -g focus-events on || exit 1
$INNER set-option -s escape-time 1000 || exit 1
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
bind_key M-Up U

send_raw()
{
	# Hex arguments avoid command separators in a partial sequence.
	bytes=$(printf '%s' "$1" | od -An -v -tx1)
	$OUTER send-keys -H $bytes || exit 1
}

capture()
{
	$INNER capture-pane -p | tr -d '\n'
}

focused()
{
	case $($INNER list-clients -F '#{client_flags}') in
	*focused*)
		echo yes
		;;
	*)
		echo no
		;;
	esac
}

# Start unfocused, whatever the state after attaching, once the pane has
# enabled focus reporting.
i=0
while :; do
	send_raw "$(printf '\033[I\033[O')"
	sleep 0.05
	case $(capture) in
	*'^[[O')
		[ "$(focused)" = no ] && break
		;;
	esac
	[ "$i" -lt 100 ] || fail 'pane did not receive focus reports'
	i=$((i + 1))
done
expected_output=$(capture)

# A focus report is forwarded to the pane directly, while the Escape binding
# runs from the command queue, so the pane may see them in either order. The
# second expected output, if not empty, is that other order.
assert_events()
{
	name=$1
	expected=$expected_output$2
	if [ -n "$3" ]; then
		alternative=$expected_output$3
	else
		alternative=$expected
	fi
	expected_focus=$4
	send_raw "$5"
	if [ "$#" -eq 6 ]; then
		sleep 0.05
		send_raw "$6"
	fi
	i=0
	while [ "$i" -lt 100 ]; do
		actual=$(capture)
		actual_focus=$(focused)
		if [ "$actual" = "$expected" ] ||
		    [ "$actual" = "$alternative" ]; then
			if [ "$actual_focus" = "$expected_focus" ]; then
				expected_output=$actual
				return 0
			fi
		fi
		sleep 0.05
		i=$((i + 1))
	done
	fail "$name: got '$actual' focused=$actual_focus," \
	    "expected '$expected' focused=$expected_focus"
}

assert_events 'FocusIn' '^[[I' '' yes "$(printf '\033[I')"
assert_events 'FocusOut' '^[[O' '' no "$(printf '\033[O')"
assert_events 'Escape and FocusIn' 'E^[[I' '^[[IE' yes \
    "$(printf '\033\033[I')"
assert_events 'Escape and FocusOut' 'E^[[O' '^[[OE' no \
    "$(printf '\033\033[O')"
assert_events 'split FocusIn' 'E^[[I' '^[[IE' yes \
    "$(printf '\033\033[')" 'I'
assert_events 'FocusOut split after two Escapes' 'E^[[O' '^[[OE' no \
    "$(printf '\033\033')" '[O'
assert_events 'M-Escape' A '' no "$(printf '\033\033')"
assert_events 'M-Up' U '' no "$(printf '\033\033[A')"

exit 0
