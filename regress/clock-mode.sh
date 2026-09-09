#!/bin/sh

# Exercise clock-mode initialization, all four styles, its one-second timer,
# resize into the compact renderer, arbitrary-key exit, and cleanup.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
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
	rm -rf "$DIR"
}
trap cleanup 0 1 15

capture()
{
	$OUTER capture-pane -p -t outer:0.0 2>/dev/null
}

wait_mode()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER display-message -p -t clock:0 '#{pane_mode}' 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "pane mode is '$got', expected '$want'"
}

wait_hashes()
{
	i=0
	while [ "$i" -lt 50 ]; do
		captured=$(capture)
		printf '%s\n' "$captured" | grep -q '#' && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "large clock did not render"
}

$INNER new-session -d -s clock -x80 -y24 'exec sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -w clock-mode-colour red || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t clock" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
sleep 1

for style in 12 24 12-with-seconds 24-with-seconds; do
	$INNER set-option -w -t clock:0 clock-mode-style "$style" || exit 1
	$INNER clock-mode -t clock:0 || exit 1
	wait_mode clock-mode
	wait_hashes
	# Let at least one timer callback observe a new second.
	[ "$style" != 12-with-seconds ] || sleep 1.2
	$INNER send-keys -t clock:0 x || exit 1
	wait_mode ''
done

# Resizing an active mode below the large-glyph threshold selects the compact
# renderer. A visible time contains a colon and no block-clock hash.
$INNER clock-mode -t clock:0 || exit 1
wait_mode clock-mode
$INNER resize-window -t clock:0 -x20 -y5 || exit 1
i=0
while [ "$i" -lt 50 ]; do
	captured=$(capture)
	printf '%s\n' "$captured" | grep -Eq '[0-9][0-9]?:[0-9][0-9]' && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "compact clock did not render after resize"
printf '%s\n' "$captured" | grep -q '#' &&
	fail "compact clock still used large glyphs"
$INNER send-keys -t clock:0 Enter || exit 1
wait_mode ''

$INNER has-session -t clock || fail "server died during clock-mode tests"
exit 0
