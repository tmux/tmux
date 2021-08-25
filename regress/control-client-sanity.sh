#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP" 0 1 15

$TMUX -f/dev/null new -d -x200 -y200 || exit 1
$TMUX -f/dev/null splitw || exit 1
sleep 1
cat <<EOF|$TMUX -C a >$TMP
refresh-client -C 200x200
selectp -t%0
splitw
neww
splitw
selectp -t%0
killp -t%1
swapp -t%2 -s%3
neww
splitw
splitw
selectl tiled
killw
EOF
sleep 1
$TMUX has || exit 1
$TMUX lsp -aF '#{pane_id} #{window_layout}' >$TMP || exit 1
cat <<EOF|cmp -s $TMP - || exit 1
%0 f5ab,200x200,0,0[200x50,0,0,0,200x149,0,51,3]
%3 f5ab,200x200,0,0[200x50,0,0,0,200x149,0,51,3]
%2 dcbd,200x200,0,0[200x100,0,0,2,200x99,0,101,4]
%4 dcbd,200x200,0,0[200x100,0,0,2,200x99,0,101,4]
EOF
$TMUX kill-server 2>/dev/null

exit 0
