#!/bin/sh

# Test the Kitty keyboard protocol against a real terminal rather than against
# another copy of tmux. This drives kitty through its remote control interface
# so that kitty itself encodes the key presses. It is skipped unless kitty, a
# display and a kitty new enough to have send-key are all available.

PATH=/bin:/usr/bin

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

command -v kitty >/dev/null 2>&1 || exit 0
[ -n "$DISPLAY" ] || [ -n "$WAYLAND_DISPLAY" ] || exit 0
kitty @ --help 2>/dev/null | grep -q 'send-key' || exit 0

DIR=$(mktemp -d)
CONF=$DIR/conf
SOCKET=$DIR/kitty
OUT=$DIR/out
TMUX="$TEST_TMUX -LtestKrt$$ -f$CONF"

printf '%s\n' 'set -g extended-keys on' \
    'set -g extended-keys-format kitty' >"$CONF"

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
i=0
while ! $TMUX has-session -t r 2>/dev/null && [ "$i" -lt 150 ]; do
	sleep 0.1
	i=$((i + 1))
done
$TMUX has-session -t r 2>/dev/null || exit 0

# The client must negotiate the protocol with the real terminal.
i=0
while [ "$($TMUX list-clients -F '#{client_key_mode}')" != 'Kitty 1' ] &&
    [ "$i" -lt 100 ]; do
	sleep 0.1
	i=$((i + 1))
done
[ "$($TMUX list-clients -F '#{client_key_mode}')" = 'Kitty 1' ] || exit 1

# kitty encodes the key, tmux parses it and encodes it again for the pane.
try_key()
{
	key=$1
	expected=$2

	: >"$OUT"
	$TMUX respawn-pane -k -t r: \
	    "stty raw -echo; printf '\033[>1u'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 20" || return 1
	sleep 1
	kitty @ --to "unix:$SOCKET" send-key "$key" 2>/dev/null || return 1
	i=0
	while [ ! -s "$OUT" ] && [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ "$(tr -d ' \n' <"$OUT")" = "$expected" ]
}

try_key ctrl+a '1b5b39373b3575' || exit 1
try_key alt+a '1b5b39373b3375' || exit 1
try_key ctrl+shift+a '1b5b39373b3675' || exit 1

# Super cannot be represented by the standard extended keys at all, so this
# only succeeds if the whole chain used the Kitty protocol.
try_key super+a '1b5b39373b3975' || exit 1

exit 0
