#!/bin/sh

# Test pane-side Kitty keyboard protocol state and key encoding. This uses
# synthetic protocol sequences and does not require a particular terminal.

PATH=/bin:/usr/bin
TERM=screen

. ./regress-common.inc

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
CONF=$(mktemp)
OUT=$(mktemp)
TMUX="$TEST_TMUX -LtestK$$ -f$CONF"

trap 'rm -f "$CONF" "$OUT"; $TMUX kill-server 2>/dev/null' 0 1 15

printf '%s\n' 'set -g extended-keys on' >"$CONF"

# Wait until the pane has written its complete capture.
wait_for_capture()
{
	wait_until test -s "$OUT"
}

# Wait for the application's keyboard mode request to take effect.
wait_for_mode()
{
	wait_for_output "$1" $TMUX display-message -pt: '#{pane_key_mode}'
}

# Compare the captured bytes with the expected encoding.
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
wait_for_capture || exit 1
check_output 1b5b39373b3675

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u'; dd bs=1 count=7 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_mode 'Kitty 1'
$TMUX send-keys -t: s-a
wait_for_capture || exit 1
check_output 1b5b39373b3975

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>8u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_mode 'Kitty 8'
$TMUX send-keys -t: a
wait_for_capture || exit 1
check_output 1b5b393775

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_capture || exit 1
check_output 1b5b3f3175

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>4;2m\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_capture || exit 1
check_output 1b5b3f3075

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u\033[>8u\033[<u\033[?u\033[>8u\033[<2u\033[?u'; dd bs=1 count=10 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_capture || exit 1
check_output 1b5b3f31751b5b3f3075

: >"$OUT"
# "always" forces modifyOtherKeys mode 1, not Kitty keys.
$TMUX set-option -g extended-keys always
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>0u\033[?u'; dd bs=1 count=5 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_capture || exit 1
check_output 1b5b3f3075
wait_for_mode 'Ext 1'
$TMUX set-option -g extended-keys on

: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; printf '\033[>1u\033[?u\033[?1049h\033[?u\033[>8u\033[?u\033[?1049l\033[?u'; dd bs=1 count=20 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5"
wait_for_capture || exit 1
check_output 1b5b3f31751b5b3f30751b5b3f38751b5b3f3175

# Capture exactly one key in the requested mode.
check_key()
{
	flags=$1
	key=$2
	expected=$3
	count=$((${#expected} / 2))

	: >"$OUT"
	$TMUX respawn-pane -k -t: \
	    "stty raw -echo; printf '\033[>%su' '$flags'; dd bs=1 count=$count 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5" || exit 1
	wait_for_mode "Kitty $flags" || exit 1
	$TMUX send-keys -t: "$key" || exit 1
	wait_for_capture || exit 1
	check_output "$expected"
}

# Flag 1 preserves unmodified keys and disambiguates every modified form.
check_key 1 Enter 0d
check_key 1 Tab 09
check_key 1 BSpace 7f
check_key 1 S-Enter 1b5b31333b3275
check_key 1 C-Enter 1b5b31333b3575
check_key 1 C-S-Enter 1b5b31333b3675
check_key 1 M-Enter 1b5b31333b3375
check_key 1 S-Tab 1b5b393b3275
check_key 1 BTab 1b5b393b3275
check_key 1 C-Tab 1b5b393b3575
check_key 1 M-Tab 1b5b393b3375
check_key 1 S-BSpace 1b5b3132373b3275
check_key 1 C-BSpace 1b5b3132373b3575
check_key 1 M-BSpace 1b5b3132373b3375

# Uppercase ASCII uses a lowercase code point and the Shift modifier.
check_key 9 A 1b5b39373b3275
check_key 1 C-A 1b5b39373b3675
check_key 1 M-Z 1b5b3132323b3475
check_key 1 C-é 1b5b3233333b3575
check_key 1 C-😀 1b5b3132383531323b3575

# Modified F3 must be distinct from a cursor position report under flag 1.
check_key 1 S-F3 1b5b31333b327e

# Non-text keypad keys stay distinct under flag 1.
check_key 1 KPBegin 1b5b35373432377e
check_key 1 S-KPBegin 1b5b35373432373b327e
check_key 9 KPBegin 1b5b35373432377e

# The modifyOtherKeys output format does not change Kitty encoding.
for format in csi-u xterm kitty; do
	$TMUX set-option -s extended-keys-format "$format" || exit 1
	check_key 1 C-a 1b5b39373b3575
done

# Unencodable key names are dropped without leaving an Escape prefix.
: >"$OUT"
$TMUX respawn-pane -k -t: \
    "stty raw -echo; $TMUX wait-for -S ready; dd bs=1 count=1 2>/dev/null | od -An -v -t x1 >'$OUT'; sleep 5" || exit 1
$TMUX wait-for ready || exit 1
for key in C-Escape C-BSpace C-# C-M-Escape M-C-BSpace; do
	$TMUX send-keys -t: "$key" || exit 1
done
$TMUX send-keys -t: -l x || exit 1
wait_for_capture || exit 1
check_output 78

$TMUX kill-server 2>/dev/null
exit 0
