#!/bin/sh

# Test per-client keyboard protocol detection and pane-side encoding.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
KCONF=$(mktemp)
LCONF=$(mktemp)
OUT=$(mktemp)
TMP=$(mktemp)
SOCKETS="testKmc$$ testKka$$ testKua$$ testKrt$$ testKru$$ testKlq$$ testKlu$$"

printf '%s\n' 'set -g extended-keys on' >"$KCONF"
printf '%s\n' 'set -g extended-keys off' >"$LCONF"

cleanup()
{
	rm -f "$KCONF" "$LCONF" "$OUT" "$TMP"
	for socket in $SOCKETS; do
		$TEST_TMUX -L"$socket" kill-server 2>/dev/null
	done
}
trap cleanup 0 1 15

wait_for_mode()
{
	tmux=$1
	wanted=$2
	i=0
	while [ "$($tmux display-message -pt: '#{pane_key_mode}')" != "$wanted" ] &&
	    [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ "$($tmux display-message -pt: '#{pane_key_mode}')" = "$wanted" ]
}

wait_for_output()
{
	i=0
	while [ ! -s "$1" ] && [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ -s "$1" ]
}

client_mode()
{
	tmux=$1
	session=$2

	if [ -z "$session" ]; then
		$tmux list-clients -F '#{client_key_mode}'
	else
		$tmux list-clients -F '#{client_session}:#{client_key_mode}' |
		    sed -n "s/^$session://p"
	fi
}

wait_for_client_mode()
{
	tmux=$1
	session=$2
	wanted=$3
	i=0
	while [ "$(client_mode "$tmux" "$session")" != "$wanted" ] &&
	    [ "$i" -lt 50 ]; do
		sleep 0.1
		i=$((i + 1))
	done
	[ "$(client_mode "$tmux" "$session")" = "$wanted" ]
}

# Each client must be negotiated independently. A tmux with extended-keys off
# stands in for a terminal without Kitty keys, since it does not answer the
# query.
M="$TEST_TMUX -LtestKmc$$ -f$KCONF"
KA="$TEST_TMUX -LtestKka$$ -f$KCONF"
UA="$TEST_TMUX -LtestKua$$ -f$LCONF"
$M new-session -d -x80 -y24 -s kitty || exit 1
$M new-session -d -x80 -y24 -s csiu || exit 1
$KA new-session -d -x80 -y24 "$M attach-session -t kitty" || exit 1
$UA new-session -d -x80 -y24 "$M attach-session -t csiu" || exit 1
wait_for_mode "$KA" 'Kitty 1' || exit 1

# Each client must report the protocol its own terminal negotiated.
wait_for_client_mode "$M" kitty 'Kitty 1' || exit 1
wait_for_client_mode "$M" csiu 'Ext' || exit 1

uc=$($M list-clients -F '#{client_name} #{client_session}' |
    awk '$2 == "csiu" { print $1 }')
[ -n "$uc" ] || exit 1
$M command-prompt -t"$uc" -k 'display-message -pl "%%"' >"$TMP" &
pid=$!
sleep 0.2
$UA send-keys Escape '[97;5u'
wait "$pid"
[ "$(tr -d '[:space:]' <"$TMP")" = 'C-a' ] || exit 1

# A Kitty-capable terminal must still be reported when it is not in use.
$M set-option -g extended-keys off
wait_for_client_mode "$M" kitty 'VT10x (Kitty)' || exit 1
wait_for_client_mode "$M" csiu 'VT10x' || exit 1
$M set-option -g extended-keys on
wait_for_client_mode "$M" kitty 'Kitty 1' || exit 1

$KA kill-server 2>/dev/null
$UA kill-server 2>/dev/null
$M kill-server 2>/dev/null

# Standard extended input must be translated for a Kitty-requesting pane.
: >"$OUT"
R="$TEST_TMUX -LtestKrt$$ -f$KCONF"
RU="$TEST_TMUX -LtestKru$$ -f$LCONF"
$R new-session -d -x80 -y24 \
    "stty raw -echo; printf '\033[>1u'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5" || exit 1
$RU new-session -d -x80 -y24 "$R attach-session" || exit 1
wait_for_client_mode "$R" '' 'Ext' || exit 1
$RU send-keys C-a
wait_for_output "$OUT" || exit 1
[ "$(tr -d ' \n' <"$OUT")" = '1b5b39373b3575' ] || exit 1
$RU kill-server 2>/dev/null
$R kill-server 2>/dev/null

# An application is told the Kitty flags it asked for even when a client that
# only supports VT10x is attached, and keys from that client are encoded as it
# asked.
: >"$OUT"
: >"$TMP"
printf '%s\n' "set -as terminal-overrides ',*:Eneks@:Dseks@'" >>"$KCONF"
LQ="$TEST_TMUX -LtestKlq$$ -f$KCONF"
LU="$TEST_TMUX -LtestKlu$$ -f$LCONF"
$LQ new-session -d -x80 -y24 \
    "stty raw -echo; sleep 2; printf '\033[>1u\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$TMP'; sleep 5" || exit 1
$LU new-session -d -x80 -y24 "$LQ attach-session" || exit 1
wait_for_output "$OUT" || exit 1
[ "$(tr -d ' \n' <"$OUT")" = '1b5b3f3175' ] || exit 1
wait_for_client_mode "$LQ" '' 'VT10x' || exit 1
$LU send-keys C-a
wait_for_output "$TMP" || exit 1
[ "$(tr -d ' \n' <"$TMP")" = '1b5b39373b3575' ] || exit 1

exit 0
