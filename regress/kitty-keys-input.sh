#!/bin/sh

# Test client-side Kitty keyboard protocol support: an outer tmux acts as a
# Kitty-capable terminal for the inner tmux which is the one under test. The
# inner tmux queries the outer one, the outer one replies, and the inner one
# then parses the key sequences sent to it.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
CONF=$(mktemp)
TMP=$(mktemp)

printf '%s\n' 'set -g extended-keys on' \
    'set -g extended-keys-format kitty' >"$CONF"

TMUX="$TEST_TMUX -LtestKIA$$ -f$CONF"
TMUX2="$TEST_TMUX -LtestKIB$$ -f$CONF"

trap 'rm -f "$CONF" "$TMP"; $TMUX kill-server 2>/dev/null; \
    $TMUX2 kill-server 2>/dev/null' 0 1 15

$TMUX2 new-session -d -x80 -y24 || exit 1
$TMUX new-session -d -x80 -y24 "$TMUX2 attach" || exit 1

exit_status=0

wait_for_mode()
{
	i=0
	while [ "$($TMUX display-message -pt: '#{pane_key_mode}')" != "$1" ] &&
	    [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ "$($TMUX display-message -pt: '#{pane_key_mode}')" = "$1" ]
}

assert_key()
{
	keys=$1
	expected=$2

	$TMUX2 command-prompt -k 'display-message -pl "%%"' >"$TMP" &
	sleep 0.15
	$TMUX send-keys $keys
	wait

	actual=$(tr -d '[:space:]' <"$TMP")
	if [ "$actual" = "$expected" ]; then
		[ -n "$VERBOSE" ] && echo "[PASS] $keys -> $actual"
	else
		echo "[FAIL] $keys -> $expected (got '$actual')"
		exit_status=1
	fi
}

# The inner tmux must detect the outer one and push its flags.
if ! wait_for_mode 'Kitty 1'; then
	echo "[FAIL] inner tmux did not enable the Kitty protocol"
	exit 1
fi

# The handshake is done, so stop the outer tmux encoding the raw sequences
# below as Kitty keys itself.
$TMUX set-option -g extended-keys-format csi-u

# Modifiers, including the Kitty-only Super and Hyper.
assert_key 'Escape [97;2u' 'S-a'
assert_key 'Escape [97;3u' 'M-a'
assert_key 'Escape [97;5u' 'C-a'
assert_key 'Escape [97;6u' 'C-S-a'
assert_key 'Escape [97;7u' 'C-M-a'
assert_key 'Escape [97;9u' 's-a'
assert_key 'Escape [97;17u' 'H-a'
assert_key 'Escape [65;2u' 'S-A'

# Keys with a dedicated tmux key code.
assert_key 'Escape [9;5u' 'C-Tab'
assert_key 'Escape [9;2u' 'BTab'
assert_key 'Escape [13;5u' 'C-Enter'
assert_key 'Escape [27u' 'Escape'
assert_key 'Escape [32;5u' 'C-Space'
assert_key 'Escape [127;5u' 'C-BSpace'

# Keys with a CSI final byte other than u.
assert_key 'Escape [1;5A' 'C-Up'
assert_key 'Escape [1;3D' 'M-Left'
assert_key 'Escape [1;5H' 'C-Home'
assert_key 'Escape [1;5P' 'C-F1'

# Functional and non-ASCII keys.
assert_key 'Escape [57399;5u' 'C-KP0'
assert_key 'Escape [233;5u' 'C-é'

# The alternate key and associated text fields are not requested but must be
# ignored if a terminal sends them anyway.
assert_key 'Escape [97:65;2u' 'S-a'
assert_key 'Escape [97:65:97;5u' 'C-a'
assert_key 'Escape [97;5;97u' 'C-a'

# A repeat event is a key press, a release event is dropped. The C-b that
# follows shows that the sequence was consumed and not passed through.
assert_key 'Escape [97;5:1u' 'C-a'
assert_key 'Escape [97;5:2u' 'C-a'
assert_key 'Escape [97;5:3u Escape [98;5u' 'C-b'

# The Meta modifier bit is not representable and is dropped.
assert_key 'Escape [97;33u Escape [98;5u' 'C-b'

# Lock modifiers are ignored rather than dropping the key.
assert_key 'Escape [97;65u' 'a'
assert_key 'Escape [97;129u' 'a'

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

exit $exit_status
