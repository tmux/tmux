#!/bin/sh

# Damage-only redraws must evaluate both active and inactive border styles
# for each client, even though the cached border cells belong to the pane.
# Exercise client-dependent colours and dim values separately.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lborder-style-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lborder-style-outer-$$ -f/dev/null"
CAPTURE=$DIR/capture

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

wait_for_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		count=$($INNER list-clients 2>/dev/null | wc -l)
		[ "$count" -eq 2 ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "two inner clients did not attach"
}

wait_for_marker()
{
	target=$1
	marker=$2
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t "$target" >"$CAPTURE" || exit 1
		grep -q "$marker" "$CAPTURE" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client in $target did not receive $marker"
}

cat >"$DIR/emitter.pl" <<'PERL'
use strict;
use warnings;

$| = 1;
for my $phase (1 .. 4) {
	while (!-e "$ENV{TRIGGER}-$phase") {
		select undef, undef, undef, 0.01;
	}
	# Change an unused palette entry and acknowledge it in the pane body.
	print "\e]4;200;rgb:11/22/0$phase\a\e[1;1HDAMAGE$phase";
}
sleep 100;
PERL

$INNER new-session -d -s inner -x 60 -y 20 \
    "TRIGGER='$DIR/trigger' perl '$DIR/emitter.pl'" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g automatic-rename off || exit 1
$INNER set -g status-interval 0 || exit 1
$INNER set -g terminal-features '*:RGB' || exit 1
$INNER set -g pane-border-lines simple || exit 1
$INNER set -g pane-border-status top || exit 1
$INNER set -g pane-border-format 'CLIENT=#{client_name}' || exit 1
FLOAT=$($INNER new-pane -d -PF '#{pane_id}' -x 35 -y 6 -X 5 -Y 5 \
    'sleep 100') || exit 1

$OUTER new-session -d -s outer -x 121 -y 20 'sleep 100' || exit 1
$OUTER set -g status off || exit 1
$OUTER set -g window-size manual || exit 1
$OUTER set -g default-terminal screen || exit 1
LEFT=$($OUTER display-message -p -t outer:0.0 '#{pane_id}') || exit 1
RIGHT=$($OUTER split-window -h -PF '#{pane_id}' 'sleep 100') || exit 1
for target in "$LEFT" "$RIGHT"; do
	$OUTER respawn-pane -k -t "$target" "$INNER attach-session -t inner" ||
	    exit 1
done
wait_for_clients

NAME1=$($OUTER display-message -p -t "$LEFT" '#{pane_tty}') || exit 1
NAME2=$($OUTER display-message -p -t "$RIGHT" '#{pane_tty}') || exit 1
# Capture just the floating pane's rows, excluding the acknowledgement in
# the tiled pane. Exercise each cache by changing the floating pane's focus.
phase=1
for mode in colour dim; do
	if [ "$mode" = colour ]; then
		STYLE="fg=#{?#{==:#{client_name},$NAME1},red,blue}"
		ACTIVE_STYLE=$STYLE
	else
		STYLE="fg=#c8c8c8,bg=#646464,dim=#{?#{==:#{client_name},$NAME1},25,50}%"
		ACTIVE_STYLE="fg=#c8c8c8,bg=#646464,dim=#{?#{==:#{client_name},$NAME1},50,75}%"
	fi
	$INNER set -g pane-border-style "$STYLE" || exit 1
	$INNER set -g pane-active-border-style "$ACTIVE_STYLE" || exit 1
	$INNER select-pane -t inner:0.0 || exit 1

	for active in 0 1; do
		if [ "$active" -eq 1 ]; then
			$INNER select-pane -t "$FLOAT" || exit 1
		fi
		$INNER refresh-client -t "$NAME1" || exit 1
		$INNER refresh-client -t "$NAME2" || exit 1
		sleep 0.5
		for target in "$LEFT" "$RIGHT"; do
			$OUTER capture-pane -pe -S 5 -E 10 -t "$target" \
			    >"$DIR/before-$target" || exit 1
			if [ "$mode" = colour ]; then
				if [ "$target" = "$LEFT" ]; then
					FG=$(printf '\033[31m')
				else
					FG=$(printf '\033[34m')
				fi
			else
				if [ "$target" = "$LEFT" ]; then
					value=$((150 - active * 50))
				else
					value=$((100 - active * 50))
				fi
				FG=$(printf '\033[38;2;%d;%d;%dm' "$value" "$value" "$value")
				value=$((value / 2))
				BG=$(printf '\033[48;2;%d;%d;%dm' "$value" "$value" "$value")
				grep -Fq "$BG" "$DIR/before-$target" ||
				    fail "phase $phase: missing $target's dimmed background"
			fi
			grep -Fq "$FG" "$DIR/before-$target" ||
			    fail "phase $phase: missing $target's border foreground"
		done

		: >"$DIR/trigger-$phase"
		wait_for_marker "$LEFT" "DAMAGE$phase"
		wait_for_marker "$RIGHT" "DAMAGE$phase"
		sleep 0.2
		for target in "$LEFT" "$RIGHT"; do
			$OUTER capture-pane -pe -S 5 -E 10 -t "$target" \
			    >"$CAPTURE" || exit 1
			diff -u "$DIR/before-$target" "$CAPTURE" ||
			    fail "phase $phase changed $target's border style"
		done
		phase=$((phase + 1))
	done
done

exit 0
