#!/bin/sh

# new session environment

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
OUT=$(mktemp)
SCRIPT=$(mktemp)
trap "rm -f $TMP $OUT $SCRIPT" 0 1 15

cat <<EOF >$SCRIPT
(
echo TERM=\$TERM
echo PWD=\$(pwd)
echo PATH=\$PATH
echo SHELL=\$SHELL
echo TEST=\$TEST
) >$OUT
EOF

cat <<EOF >$TMP
new -- /bin/sh $SCRIPT
EOF

(cd /; env -i TERM=ansi TEST=test1 PATH=1 SHELL=/bin/sh \
	$TMUX -f$TMP start) || exit 1
sleep 1
(cat <<EOF|cmp -s - $OUT) || exit 1
TERM=screen
PWD=/
PATH=1
SHELL=/bin/sh
TEST=test1
EOF

(cd /; env -i TERM=ansi TEST=test2 PATH=2 SHELL=/bin/sh \
	$TMUX -f$TMP new -d -- /bin/sh $SCRIPT) || exit 1
sleep 1
(cat <<EOF|cmp -s - $OUT) || exit 1
TERM=screen
PWD=/
PATH=2
SHELL=/bin/sh
TEST=test2
EOF

(cd /; env -i TERM=ansi TEST=test3 PATH=3 SHELL=/bin/sh \
	$TMUX -f/dev/null new -d source $TMP) || exit 1
sleep 1
(cat <<EOF|cmp -s - $OUT) || exit 1
TERM=screen
PWD=/
PATH=2
SHELL=/bin/sh
TEST=test2
EOF

$TMUX kill-server 2>/dev/null

exit 0
