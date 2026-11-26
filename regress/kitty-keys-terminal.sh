#!/bin/sh

# Test the Kitty keyboard protocol against a real terminal rather than against
# another copy of tmux. This drives kitty through its remote control interface
# so that kitty itself encodes the key presses.
#
# This has to be run by hand from this directory. The Makefile runs tests with
# env -i, which removes DISPLAY and HOME, so kitty cannot start and this exits
# immediately without testing anything. It is also skipped unless kitty is
# installed and is new enough to have send-key.

PATH=/bin:/usr/bin

. ./regress-common.inc

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

command -v kitty >/dev/null 2>&1 || exit 101
[ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ] || exit 101
kitty @ --help 2>/dev/null | grep -q 'send-key' || exit 101

DIR=$(mktemp -d)
CONF=$DIR/conf
SOCKET=$DIR/kitty
OUT=$DIR/out
TMUX="$TEST_TMUX -LtestKrt$$ -f$CONF"

printf '%s\n' 'set -g extended-keys on' >"$CONF"

# Stop the test server and remove its temporary directory.
cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

kitty -o allow_remote_control=yes --listen-on "unix:$SOCKET" \
    -o confirm_os_window_close=0 -e \
    $TMUX new-session -s r "sleep 120" >"$DIR/err" 2>&1 &

# The terminfo entry for kitty may be missing, so treat a server which never
# appears as a reason to skip rather than as a failure.
wait_until $TMUX has-session -t r 2>/dev/null || exit 101

# The client must negotiate the protocol with the real terminal.
wait_for_output 'Kitty 1' $TMUX list-clients -F '#{client_key_mode}' || exit 1

# Check a key encoded by the real Kitty terminal.
try_key()
{
	key=$1
	expected=$2

	: >"$OUT"
	$TMUX respawn-pane -k -t r: \
	    "stty raw -echo; printf '\033[>1u'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 20" || return 1
	wait_for_output 'Kitty 1' $TMUX display-message -pt r: \
	    '#{pane_key_mode}' || return 1
	kitty @ --to "unix:$SOCKET" send-key "$key" 2>/dev/null || return 1
	wait_until test -s "$OUT" || return 1
	[ "$(tr -d ' \n' <"$OUT")" = "$expected" ]
}

try_key ctrl+a '1b5b39373b3575' || exit 1
try_key alt+a '1b5b39373b3375' || exit 1
try_key ctrl+shift+a '1b5b39373b3675' || exit 1

# Super cannot be represented by the standard extended keys at all, so this
# only succeeds if the whole chain used the Kitty protocol.
try_key super+a '1b5b39373b3975' || exit 1

exit 0
