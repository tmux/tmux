#!/bin/sh

# A synchronized update on the base screen must not defer cursor movement on
# the copy-mode screen displayed in its place.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
CLIENT_BYTES=$DIR/client-bytes
CONTROL=$DIR/control
EMITTER=$DIR/emitter.pl

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

wait_for_client()
{
	i=0
	while [ "$i" -lt 50 ]; do
		$INNER list-clients -F '#{client_termfeatures}' 2>/dev/null |
		    grep -q 'sync' && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "sync-capable client did not attach"
}

wait_for_sync()
{
	i=0
	while [ "$i" -lt 50 ]; do
		[ "$($INNER display-message -p \
		    '#{synchronized_output_flag}' 2>/dev/null)" = 1 ] &&
		    return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "application did not enter synchronized output"
}

wait_for_cursor_position()
{
	position=$(printf '\033[%d;%dH' "$2" "$1")
	i=0
	while [ "$i" -lt 20 ]; do
		grep -Fq "$position" "$CLIENT_BYTES" 2>/dev/null &&
		    return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "copy-mode cursor position did not reach client"
}

cat >"$EMITTER" <<'PERL'
use strict;
use warnings;

my $control = $ENV{CONTROL};
(my $dir = $control) =~ s{/[^/]+$}{};

# Leave the application cursor away from the left edge so copy mode can move
# it left.
syswrite STDOUT, "copy cursor";
open my $ready, '>', "$dir/ready" or die "$dir/ready: $!\n";
close $ready;
while (!-e $control) {
	select undef, undef, undef, 0.01;
}

# Keep a real application synchronized update open while the separately
# displayed copy-mode screen moves its cursor. Repeating DECSET also keeps the
# one-second safety timer from ending the update during the assertion.
for (1 .. 200) {
	syswrite STDOUT, "\e[?2026h";
	select undef, undef, undef, 0.05;
}
PERL

$INNER new-session -d -s inner -x 80 -y 24 \
    "CONTROL='$CONTROL' perl '$EMITTER'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:sync' || exit 1

$OUTER new-session -d -s outer -x 80 -y 24 \
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
wait_for_client

i=0
while [ "$i" -lt 50 ] && [ ! -e "$DIR/ready" ]; do
	sleep 0.1
	i=$((i + 1))
done
[ -e "$DIR/ready" ] || fail "application emitter did not become ready"

$INNER copy-mode -t inner:0.0 || exit 1
before=$($INNER display-message -p -t inner:0.0 '#{copy_cursor_x}') ||
    exit 1
[ "$before" -gt 0 ] || fail "copy-mode cursor started at left edge"

: >"$CONTROL"
wait_for_sync
sleep 0.5

$OUTER pipe-pane -O -t outer:0.0 "cat >'$CLIENT_BYTES'" || exit 1
sleep 0.1
$INNER send-keys -t inner:0.0 -X cursor-left || exit 1
after=$($INNER display-message -p -t inner:0.0 '#{copy_cursor_x}') ||
    exit 1
after_y=$($INNER display-message -p -t inner:0.0 '#{copy_cursor_y}') ||
    exit 1
[ "$after" -eq $((before - 1)) ] ||
    fail "copy-mode cursor did not move logically: $before to $after"
[ "$($INNER display-message -p '#{synchronized_output_flag}')" = 1 ] ||
    fail "application synchronized output ended unexpectedly"

wait_for_cursor_position $((after + 1)) $((after_y + 1))
$OUTER pipe-pane -t outer:0.0 || exit 1
