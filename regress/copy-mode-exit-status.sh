#!/bin/sh

# Check OSC 133 exit status indicators in copy mode using a real client.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

fail() {
	echo "$*" >&2
	exit 1
}

capture() {
	$TMUX capture-pane -pS0 -E- >$TMP || exit 1
}

check_grep() {
	grep -Fq "$1" $TMP || fail "missing pattern: $1"
}

check_no_grep() {
	grep -Fq "$1" $TMP && fail "unexpected pattern: $1"
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null; $TMUX2 kill-server 2>/dev/null" 0 1 15

$TMUX2 new-session -d -x80 -y10 \
	"printf '\033]133;A\007p\$ \033]133;B\007one\n\033]133;C\007out1\n\033]133;D;0\007\033]133;A\007p\$ \033]133;B\007two\n\033]133;C\007out2\n\033]133;D;123\007\033]133;A\007p\$ \033]133;B\007silent\n\033]133;C\007\033]133;D;7\007\033]133;A\007p\$ \033]133;B\007three\n\033]133;C\007out3\n\033]133;D\007\033]133;A\007p\$ \033]133;B\007'; exec sleep 100" || \
	exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g copy-mode-position-format '#[align=left]POS' || exit 1
$TMUX2 list-keys -Tcopy-mode O | grep -Fq 'fold-view-toggle' || exit 1
$TMUX2 list-keys -Tcopy-mode-vi O | grep -Fq 'fold-view-toggle' || exit 1

$TMUX new-session -d -x80 -y10 || exit 1
$TMUX set -g status off || exit 1
$TMUX send -l "$TMUX2 attach" || exit 1
$TMUX send Enter || exit 1
sleep 1

$TMUX2 copy-mode -U || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1
capture
check_grep "!- p\$ two"
check_grep "!  p\$ silent"
check_no_grep "!+ p\$ silent"
check_grep "POS"

# Rebuilding with collapsed output must not expand copy-mode formats before
# the viewport has been restored for the shorter backing grid.
$TMUX2 send -X collapse-output -a || exit 1
sleep 1
capture
check_grep "!+ p\$ two"
check_grep "!  p\$ silent"
check_no_grep "!+ p\$ silent"
$TMUX2 send -X line-numbers-on || exit 1
sleep 1
capture
check_grep "  3 !+ p\$ two"
$TMUX2 send -X fold-view-toggle || exit 1
[ "$($TMUX2 display -p '#{copy_fold_view}')" = 0 ] || exit 1
sleep 1
capture
check_grep "out2"
check_no_grep "!+ p\$ two"
check_grep "  2 p\$ two"
$TMUX2 send -X fold-view-toggle || exit 1
[ "$($TMUX2 display -p '#{copy_fold_view}')" = 1 ] || exit 1
sleep 1
capture
check_grep "  3 !- p\$ two"

# A format wider than the standard three-column gutter is not truncated.
$TMUX2 send -X cancel || exit 1
$TMUX2 set -p copy-mode-exit-status-format \
	'#[align=right]#{?exit_status,!#{exit_status}, }' || exit 1
$TMUX2 copy-mode -c || exit 1
$TMUX2 send -X history-top || exit 1
sleep 1
capture
check_grep "!123+ p\$ two"

$TMUX2 send -X search-forward-text two || exit 1
$TMUX2 send -X select-line || exit 1
$TMUX2 send -X copy-selection || exit 1
selection=$($TMUX2 show-buffer)
[ "$selection" = 'p$ two' ] || fail "unexpected selection: $selection"

# The manual's numeric format shows nonzero statuses in red.
$TMUX2 set -p copy-mode-exit-status-format \
	'#{?exit_status,#[fg=red]#{exit_status}, }' || exit 1
wait_for_text '123+ p$ two' $TMUX capture-pane -p || exit 1
capture
check_no_grep '0+ p$ one'
check_no_grep '0+ p$ three'
$TMUX capture-pane -pR >"$TMP" || exit 1
grep -q 'data=(1,1,1).* fg=red\[1\]' "$TMP" || \
	fail 'numeric exit status was not red'

# A reported zero remains distinguishable from an absent exit status.
$TMUX2 set -p copy-mode-exit-status-format 'RC=[#{exit_status}]' || exit 1
wait_for_text 'RC=[0]' $TMUX capture-pane -p || exit 1
wait_for_text 'RC=[]' $TMUX capture-pane -p || exit 1
capture
check_grep 'RC=[123]'
check_grep 'RC=[7]'
check_grep 'RC=[]'

# A command which has not sent D has no exit status either.
$TMUX2 new-window -n running \
	"printf '\033]133;A\007p\$ \033]133;B\007running\r\n\033]133;C\007out\r\n'; exec sleep 100" || exit 1
$TMUX2 set -p -t :running copy-mode-exit-status-format 'RC=[#{exit_status}]' || exit 1
$TMUX2 set -w -t :running copy-mode-position-format '' || exit 1
wait_for_text out $TMUX2 capture-pane -t :running -p || exit 1
$TMUX2 copy-mode -t :running -U || exit 1
wait_for_text 'RC=[]- p$ running' $TMUX capture-pane -p || exit 1

exit 0
