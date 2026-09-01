#!/bin/sh

# With clear-on-attach disabled tmux must preserve the terminal contents by
# scrolling them away, rather than entering and clearing the alternate screen.
# Capture the bytes written by a real attached client for both settings.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER_ON="$TEST_TMUX -LtestIon$$ -f/dev/null"
OUTER_ON="$TEST_TMUX -LtestOon$$ -f/dev/null"
INNER_OFF="$TEST_TMUX -LtestIoff$$ -f/dev/null"
OUTER_OFF="$TEST_TMUX -LtestOoff$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER_ON kill-server 2>/dev/null
	$INNER_ON kill-server 2>/dev/null
	$OUTER_OFF kill-server 2>/dev/null
	$INNER_OFF kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$current_inner list-clients 2>/dev/null | grep -q . && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "inner client did not attach"
}

capture_attach()
{
	setting=$1
	output=$2
	go=$DIR/go-$setting
	if [ "$setting" = on ]; then
		current_inner=$INNER_ON
		current_outer=$OUTER_ON
	else
		current_inner=$INNER_OFF
		current_outer=$OUTER_OFF
	fi

	$current_inner new-session -d -s inner -x40 -y10 \
	    'exec sleep 100' || exit 1
	$current_inner set-option -g status off || exit 1
	$current_inner set-option -g clear-on-attach "$setting" || exit 1
	$current_outer new-session -d -s outer -x40 -y10 \
	    "while [ ! -e '$go' ]; do sleep 0.01; done; exec $current_inner attach-session -t inner" || exit 1
	$current_outer set-option -g status off || exit 1
	$current_outer pipe-pane -O "cat >'$output'" || exit 1
	touch "$go"
	wait_for_client
	sleep 0.5
	$current_outer pipe-pane -O || exit 1
}

on=$DIR/on
off=$DIR/off
capture_attach on "$on"
capture_attach off "$off"

smcup=$(printf '\033[?1049h')
grep -Fq "$smcup" "$on" || fail "clear-on-attach on did not use smcup"
if grep -Fq "$smcup" "$off"; then
	fail "clear-on-attach off used smcup"
fi

# screen has the indn capability (CSI Ps S), which the preserving path uses
# after setting the scrolling region and moving to its last line.
escape=$(printf '\033')
LC_ALL=C grep -Eq "${escape}\\[[0-9]+S" "$off" ||
	fail "clear-on-attach off did not scroll the old contents away"

exit 0
