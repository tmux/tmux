#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

cleanup()
{
	stop_server $TMUX || exit 1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

pending_matches()
{
	$TMUX capture-pane -pP >"$DIR/actual" || return 1
	cmp -s "$DIR/expected" "$DIR/actual"
}

check_pending()
{
	{ cat "$DIR/pending"; printf '\n'; } >"$DIR/expected"
	wait_until pending_matches || fail 'pending input differs'
}

append_pending()
{
	printf '%b' "$1" >"$DIR/chunk"
	cat "$DIR/chunk" >>"$DIR/pending"
	cat "$DIR/chunk" >"$DIR/input"
	check_pending
}

finish_pending()
{
	printf '%b' "$1" >"$DIR/input"
	: >"$DIR/pending"
	check_pending
}

# A FIFO controls application output without completing its escape sequence.
mkfifo "$DIR/input" || exit 1
: >"$DIR/pending"
$TMUX new-session -d -s pending \
    "stty raw -echo; exec 3<>'$DIR/input';
     printf '\033]2;pending-ready\007'; exec cat <&3" || exit 1
wait_for_output pending-ready $TMUX display-message -p '#{pane_title}' || exit 1
check_pending

# Retain a CSI split across reads, then discard it on completion.
append_pending '\033[3'
append_pending '1'
finish_pending 'mX'

# Cross allocation boundaries in one unfinished OSC, over separate reads.
append_pending '\033]999;'
for size in 63 64 65 127 128 129 255 256 257 4095 4096 4097 16384; do
	length=$(wc -c <"$DIR/pending")
	awk -v n="$((size - length))" \
	    'BEGIN { for (i = 0; i < n; i++) printf "x" }' >"$DIR/chunk"
	cat "$DIR/chunk" >>"$DIR/pending"
	cat "$DIR/chunk" >"$DIR/input"
	check_pending
done
finish_pending '\007'

# A new short sequence must not expose bytes from the larger allocation.
append_pending '\033[4'
finish_pending 'm'

# Raw and escaped captures must preserve NUL, UTF-8, high bytes and backslash.
append_pending '\033]999;A\000\001\303\251\377\\END'
$TMUX capture-pane -pPC >"$DIR/actual" || exit 1
printf '%s\n' '\033]999;A\000\001\303\251\377\134END' >"$DIR/expected"
cmp "$DIR/expected" "$DIR/actual" || fail 'escaped pending input differs'
finish_pending '\007'

# A DCS terminator may itself be split across reads.
append_pending '\033P1;2qhello'
append_pending '\033'
finish_pending '\\'

# Cancellation must discard the pending sequence too.
append_pending '\033[123'
finish_pending '\030'

exit 0
