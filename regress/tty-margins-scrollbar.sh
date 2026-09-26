#!/bin/sh

# With left/right margins, a pane narrowed by a scrollbar can scroll without
# retransmitting its existing rows. Both paths must produce the same screen.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(mktemp -d) || exit 1
INNER="$TEST_TMUX -Lmarginsscrollbar-inner-$$ -f/dev/null"
OUTER="$TEST_TMUX -Lmarginsscrollbar-outer-$$ -f/dev/null"

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

cat >"$DIR/emitter.pl" <<'PERL'
use strict;
use warnings;
$| = 1;
print join("\r\n", map { "KEEP0$_" } 1 .. 6);
while (!-e $ENV{TRIGGER}) {
	select undef, undef, undef, 0.01;
}
print "\r\nNEWROW";
sleep 100;
PERL

for margins in on off; do
	$INNER new-session -d -s inner -x 40 -y 6 \
	    "TRIGGER='$DIR/trigger-$margins' perl '$DIR/emitter.pl'" || exit 1
	$INNER set -g status off || exit 1
	$INNER set -g status-interval 0 || exit 1
	$INNER set -g automatic-rename off || exit 1
	$INNER set -g window-size manual || exit 1
	$INNER set -g pane-scrollbars on || exit 1
	if [ "$margins" = on ]; then
		$INNER set -as terminal-features ',screen-256color:margins' || exit 1
	fi

	$OUTER new-session -d -s outer -x 40 -y 6 'sleep 100' || exit 1
	$OUTER set -g status off || exit 1
	$OUTER set -g window-size manual || exit 1
	$OUTER set -g default-terminal screen-256color || exit 1
	$OUTER respawn-pane -k -t outer:0.0 "$INNER attach -t inner" || exit 1
	sleep 0.5
	$OUTER pipe-pane -O -t outer:0.0 "cat >'$DIR/output-$margins'" || exit 1
	$INNER refresh-client || exit 1
	sleep 0.5
	grep -aq KEEP02 "$DIR/output-$margins" || fail "initial rows not captured"
	offset=$(wc -c <"$DIR/output-$margins")
	: >"$DIR/trigger-$margins"
	i=0
	while [ "$i" -lt 50 ]; do
		$OUTER capture-pane -p -t outer:0.0 >"$DIR/screen-$margins" || exit 1
		grep -q NEWROW "$DIR/screen-$margins" && break
		sleep 0.1
		i=$((i + 1))
	done
	[ "$i" -lt 50 ] || fail "$margins: scroll did not reach the terminal"
	sleep 0.2
	tail -c +"$((offset + 1))" "$DIR/output-$margins" >"$DIR/scroll-$margins"
	grep -aq NEWROW "$DIR/scroll-$margins" || fail "scroll output not captured"
	if [ "$margins" = on ]; then
		if grep -aq KEEP02 "$DIR/scroll-$margins"; then
			fail "scroll with margins retransmitted an existing row"
		fi
	else
		grep -aq KEEP02 "$DIR/scroll-$margins" ||
		    fail "scroll without margins did not exercise the redraw fallback"
	fi
	printf 'KEEP02\nKEEP03\nKEEP04\nKEEP05\nKEEP06\nNEWROW\n' >"$DIR/expected"
	cmp -s "$DIR/expected" "$DIR/screen-$margins" ||
	    fail "$margins: terminal did not contain the expected scrolled rows"
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
done

exit 0
