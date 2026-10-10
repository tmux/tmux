#!/bin/sh

# An Escape followed by a terminal reply must remain a separate key. The
# reply's leading Escape must not be consumed as the second half of a Meta
# key, which would send the rest of the reply to the pane as text. A reply
# split before the end of a known key must wait for the rest.

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
bind_key M-] B
bind_key M-O P
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

theme()
{
	$INNER list-clients -F '#{client_theme}'
}

expected_output=
assert_keys()
{
	name=$1
	expected=$expected_output$2
	expected_theme=$3
	send_raw "$4"
	if [ "$#" -eq 5 ]; then
		sleep 0.05
		send_raw "$5"
	fi
	i=0
	while [ "$i" -lt 100 ]; do
		actual=$(capture)
		actual_theme=$(theme)
		if [ "$actual" = "$expected" ] &&
		    [ "$actual_theme" = "$expected_theme" ]; then
			expected_output=$actual
			return 0
		fi
		sleep 0.05
		i=$((i + 1))
	done
	fail "$name: got '$actual' theme=$actual_theme," \
	    "expected '$expected' theme=$expected_theme"
}

assert_keys 'Escape and dark theme report' E dark \
    "$(printf '\033\033[?997;1n')"
assert_keys 'Escape and light theme report' E light \
    "$(printf '\033\033[?997;2n')"
assert_keys 'split theme report' '' dark \
    "$(printf '\033[?997')" ';1n'
assert_keys 'Escape and light theme report, split' E light \
    "$(printf '\033\033[?99')" '7;2n'
assert_keys 'theme report split after two Escapes' E dark \
    "$(printf '\033\033')" '[?997;1n'
assert_keys 'Escape and light theme report again' E light \
    "$(printf '\033\033[?997;2n')"
assert_keys 'Escape and background colour' E light \
    "$(printf '\033\033]11;rgb:ffff/ffff/ffff\033\\')"
assert_keys 'Escape and foreground colour (BEL)' E light \
    "$(printf '\033\033]10;rgb:0000/0000/0000\007')"
assert_keys 'split background colour' E light \
    "$(printf '\033\033]11;rgb:ff')" "$(printf 'ff/ffff/ffff\033\\')"
assert_keys 'Escape and palette colour' E light \
    "$(printf '\033\033]4;1;rgb:ffff/0000/0000\033\\')"
assert_keys 'Escape and clipboard' E light \
    "$(printf '\033\033]52;c;dGVzdA==\007')"
assert_keys 'M-Escape' A light "$(printf '\033\033')"
assert_keys 'M-]' B light "$(printf '\033]')"
assert_keys 'M-O' P light "$(printf '\033O')"
assert_keys 'M-Up' U light "$(printf '\033\033[A')"

exit 0
