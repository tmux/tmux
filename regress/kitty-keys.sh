#!/bin/sh

# Test pane-side Kitty keyboard protocol state and key encoding. This uses
# synthetic protocol sequences and does not require a particular terminal.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
CONF=$(mktemp)
OUT=$(mktemp)
TMUX="$TEST_TMUX -LtestK$$ -f$CONF"

trap 'rm -f "$CONF" "$OUT"; $TMUX kill-server 2>/dev/null' 0 1 15

printf '%s\n' 'set -g extended-keys on' >"$CONF"

wait_for_output()
{
	i=0
	while [ ! -s "$OUT" ] && [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ -s "$OUT" ]
}

wait_for_mode()
{
	wanted=$1
	i=0
	while [ "$($TMUX display-message -pt: '#{pane_key_mode}')" != "$wanted" ] &&
	    [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ "$($TMUX display-message -pt: '#{pane_key_mode}')" = "$wanted" ]
}

check_output()
{
	expected=$1
	actual=$(tr -d ' \n' <"$OUT")
	if [ "$actual" != "$expected" ]; then
		echo "expected $expected, got $actual"
		exit 1
	fi
}

$TMUX new-session -d -x80 -y24 \
    "stty raw -echo; printf '\033[>1u'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5" || exit 1
wait_for_mode 'Kitty 1'
$TMUX send-keys -t: C-S-a
wait_for_output
check_output 1b5b39373b3675

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_mode 'Kitty 1'
$TMUX send-keys -t: s-a
wait_for_output
check_output 1b5b39373b3975

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>8u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_mode 'Kitty 8'
$TMUX send-keys -t: a
wait_for_output
check_output 1b5b393775

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_output
check_output 1b5b3f3175

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>4;2m\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_output
check_output 1b5b3f3075

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u\033[>8u\033[<u\033[?u\033[>8u\033[<2u\033[?u'; dd bs=1 count=10 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_output
check_output 1b5b3f31751b5b3f3075

: >"$OUT"
# "always" forces modifyOtherKeys mode 1, not Kitty keys.
$TMUX set-option -g extended-keys always
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>0u\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_output
check_output 1b5b3f3075
wait_for_mode 'Ext 1'
$TMUX set-option -g extended-keys on

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u\033[?u\033[?1049h\033[?u\033[>8u\033[?u\033[?1049l\033[?u'; dd bs=1 count=20 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_output
check_output 1b5b3f31751b5b3f30751b5b3f38751b5b3f3175

$TMUX kill-server 2>/dev/null
exit 0
