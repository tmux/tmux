#!/bin/sh

# A format loop nested inside a loop over the same sorted list must not
# change the outer list.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
stop_server $TMUX || exit 1
trap 'stop_server $TMUX' 0
trap 'exit 1' 1 2 15

# Outer window list is small; the inner loop visits a session with more
# windows, so the shared list is reallocated.
$TMUX new -d -s a || exit 1
$TMUX neww -d -t a || exit 1
$TMUX new -d -s b || exit 1
i=1
while [ $i -lt 120 ]; do
	$TMUX neww -d -t b || exit 1
	i=$((i + 1))
done
out=$($TMUX display -t a: -p '#{W:#{S:#{W:x}}}' | wc -c)
if [ $out -ne 245 ]; then
	echo "nested window loop: got $out bytes, expected 245" >&2
	exit 1
fi
out=$($TMUX lsw -t a: -F '#{S:#{W:x}}' | wc -c)
if [ $out -ne 246 ]; then
	echo "list-windows loop: got $out bytes, expected 246" >&2
	exit 1
fi
stop_server $TMUX || exit 1

$TMUX new -d -s mike || exit 1
$TMUX new -d -s alpha || exit 1
$TMUX new -d -s zeta || exit 1
out=$($TMUX display -p '#{S:#{session_name}=#{S/r:#{session_name}} }')
exp='mike=zetaalphamike alpha=zetaalphamike zeta=zetaalphamike '
if [ "$out" != "$exp" ]; then
	echo "nested session loop: got '$out', expected '$exp'" >&2
	exit 1
fi
out=$($TMUX ls -F '#{session_name}=#{S/r:#{session_name}}' | tr '\n' ' ')
exp='alpha=zetaalphamike mike=zetaalphamike zeta=zetaalphamike '
if [ "$out" != "$exp" ]; then
	echo "list-sessions loop: got '$out', expected '$exp'" >&2
	exit 1
fi

exit 0
