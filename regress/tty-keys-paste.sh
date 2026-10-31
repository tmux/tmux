#!/bin/sh

# Paste state must change only when the start or end key completes. A separate
# Escape before paste start uses its binding, pasted keys bypass bindings, and
# the first key after paste end uses its binding again.

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
    "stty raw -echo; printf '\033[?2004h'; exec cat -v" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g assume-paste-time 0 || exit 1
$INNER set-option -s escape-time 20 || exit 1
$INNER unbind-key -a -T root || exit 1
$INNER bind-key -n Escape send-keys -l E || exit 1
$INNER bind-key -n a send-keys -l A || exit 1
$OUTER new-session -d -s outer -x80 -y10 "$INNER attach -t inner" || exit 1
$OUTER set-option -g assume-paste-time 0 || exit 1

i=0
while [ -z "$($INNER list-clients -F '#{client_name}')" ] ||
    [ "$($INNER display-message -p '#{bracket_paste_flag}')" != 1 ]; do
	[ "$i" -lt 100 ] || fail 'client did not attach or enable bracketed paste'
	sleep 0.05
	i=$((i + 1))
done

send_raw()
{
	bytes=$(printf '%s' "$1" | od -An -v -tx1)
	$OUTER send-keys -H $bytes || exit 1
}

assert_output()
{
	i=0
	while [ "$i" -lt 100 ]; do
		actual=$($INNER capture-pane -p | tr -d '\n')
		[ "$actual" = "$1" ] && return 0
		sleep 0.05
		i=$((i + 1))
	done
	fail "got '$actual', expected '$1'"
}

send_raw "$(printf '\033\033[200~a\033[201~a')"
expected='E^[[200~a^[[201~A'
assert_output "$expected"

# A partial paste end gets more time than escape-time. The final key must
# still be outside the paste when the rest arrives after that time.
send_raw "$(printf '\033[200~a\033[20')"
sleep 0.05
send_raw '1~a'
expected=$expected'^[[200~a^[[201~A'
assert_output "$expected"

# Split the start immediately before its final byte, including an extra
# Escape. Neither the Escape binding nor paste mode may be lost.
$INNER set-option -s escape-time 1000 || exit 1
send_raw "$(printf '\033\033[200')"
sleep 0.05
send_raw "$(printf '~a\033[201~a')"
expected=$expected'E^[[200~a^[[201~A'
assert_output "$expected"

exit 0
