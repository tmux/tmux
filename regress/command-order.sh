#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

cat <<EOF >$TMP
new -sfoo -nfoo0; neww -nfoo1; neww -nfoo2
new -sbar -nbar0; neww -nbar1; neww -nbar2
EOF
$TMUX -f$TMP start </dev/null || exit 1
sleep 1
$TMUX lsw -aF '#{session_name},#{window_name}'|sort >$TMP || exit 1
$TMUX kill-server 2>/dev/null
cat <<EOF|cmp -s $TMP - || exit 1
bar,bar0
bar,bar1
bar,bar2
foo,foo0
foo,foo1
foo,foo2
EOF

cat <<EOF >$TMP
new -sfoo -nfoo0
neww -nfoo1
neww -nfoo2
new -sbar -nbar0
neww -nbar1
neww -nbar2
EOF
$TMUX -f$TMP start </dev/null || exit 1
sleep 1
$TMUX lsw -aF '#{session_name},#{window_name}'|sort >$TMP || exit 1
$TMUX kill-server 2>/dev/null
cat <<EOF|cmp -s $TMP - || exit 1
bar,bar0
bar,bar1
bar,bar2
foo,foo0
foo,foo1
foo,foo2
EOF

exit 0
