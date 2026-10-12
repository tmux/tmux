#!/bin/sh

# Extended Shift+Tab must remain BackTab when the pane does not request
# modifyOtherKeys=2, including when the input carries implied Meta.

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

$INNER new-session -d -s inner -x80 -y24 \
    'stty raw -echo; exec cat -tv' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g assume-paste-time 0 || exit 1
$INNER set-option -s extended-keys on || exit 1
$INNER set-option -s escape-time 1000 || exit 1
$INNER unbind-key -a -T root || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t inner" ||
    exit 1
$OUTER set-option -g assume-paste-time 0 || exit 1

i=0
while [ -z "$($INNER list-clients -F '#{client_name}')" ]; do
	[ "$i" -lt 100 ] || fail 'inner client did not attach'
	sleep 0.05
	i=$((i + 1))
done

expected_output=

assert_key()
{
	name=$1
	expected_output=$expected_output$2
	bytes=$(printf '%s' "$3" | od -An -v -tx1)
	$OUTER send-keys -H $bytes || exit 1
	i=0
	while [ "$i" -lt 100 ]; do
		actual=$($INNER capture-pane -p | tr -d '\n')
		[ "$actual" = "$expected_output" ] && return 0
		sleep 0.05
		i=$((i + 1))
	done
	fail "$name: got '$actual', expected '$expected_output'"
}

pane_state='#{pane_key_mode}:#{pane_current_command}'
for mode in 0 1 2; do
	for format in xterm csi-u; do
		$INNER new-window \
		    "printf '\033[>4;${mode}m'; stty raw -echo; exec cat -tv" ||
		    exit 1
		key_mode="Ext $mode"
		[ "$mode" -eq 0 ] && key_mode=VT10x
		i=0
		while [ "$($INNER display-message -p "$pane_state")" != \
		    "$key_mode:cat" ]; do
			[ "$i" -lt 100 ] || fail "pane did not enter mode $mode"
			sleep 0.05
			i=$((i + 1))
		done
		$INNER set-option -s extended-keys-format "$format" || exit 1
		expected_output=
		# Every modifier combination containing Shift.
		for modifier in 2 4 6 8; do
			if [ "$mode" -eq 2 ]; then
				if [ "$format" = xterm ]; then
					expected="^[[27;${modifier};9~"
				else
					expected="^[[9;${modifier}u"
				fi
			else
				expected='^[[Z'
			fi
			case_name="mode $mode, $format, modifier $modifier"
			assert_key "$case_name, CSI u input" "$expected" \
			    "$(printf '\033[9;%su' "$modifier")"
			assert_key "$case_name, xterm input" "$expected" \
			    "$(printf '\033[27;%s;9~' "$modifier")"
		done
	done
done

exit 0
