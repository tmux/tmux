#!/bin/sh

# Tests of the window and pane options saved by save-state -w and restored by
# load-state -w.
#
# This covers:
# - show-options -wH and show-options -pH for each pane being the same after a
#   load, with user options, an empty value, array options, an array set with
#   no items, a window hook and a pane hook, and options inherited rather than
#   set staying inherited;
# - options with side effects ending up with the saved value and having that
#   effect: automatic-rename both set and inherited, remain-on-exit,
#   pane-border-status and pane-border-style, synchronize-panes, and
#   pane-scrollbars, which only takes effect through options_push_changes,
#   and window-size manual, which keeps the saved size;
# - save, load and save again giving the same JSON apart from the pane ids;
# - a file without options leaving automatic-rename off for the named window;
# - an unknown option, an option in the wrong scope, a bad value and an index
#   for an option that is not an array failing with the option's name and
#   creating nothing.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"; $TMUX kill-server 2>/dev/null' 0 1 15

fail()
{
	echo "$1"
	exit 1
}

# Print a format for a target.
show()
{
	$TMUX display -p -t "$1" "$2"
}

# Print a state file without the pane ids.
strip()
{
	sed 's/"I":"%[0-9]*"//g' "$1"
}

# Check the options of two windows and their panes are the same.
same_options()
{
	[ "$($TMUX show -wH -t "$2")" = "$($TMUX show -wH -t "$1")" ] ||
	    fail "window options differ: $($TMUX show -wH -t "$2")"
	for i in 0 1 2; do
		[ "$($TMUX show -pH -t "$2.$i")" = \
		    "$($TMUX show -pH -t "$1.$i")" ] ||
		    fail "pane $i options differ: $($TMUX show -pH -t "$2.$i")"
	done
}

# Load a file that should be rejected and check nothing was created.
check_fail()
{
	before=$($TMUX list-panes -a -F '#{pane_id}')
	printf '%s' "$1" >$TMP/bad.json
	if out=$($TMUX load-state -w -t S: $TMP/bad.json 2>&1); then
		fail "invalid file was accepted: $1"
	fi
	case "$out" in
	*"$2"*) ;;
	*) fail "unexpected message for $1: $out" ;;
	esac
	[ "$($TMUX list-panes -a -F '#{pane_id}')" = "$before" ] ||
	    fail "invalid file created panes: $1"
}

$TMUX new-session -d -s S -x 80 -y 24 || fail "new-session failed"
$TMUX split-window -h -t S:0 || fail "split-window failed"
$TMUX split-window -v -t S:0 || fail "split-window failed"

# Window options.
$TMUX rename-window -t S:0 named || fail "rename-window failed"
$TMUX set -w -t S:0 @user 'a "b" \c' || fail "set @user failed"
$TMUX set -w -t S:0 @empty '' || fail "set @empty failed"
$TMUX set -w -t S:0 automatic-rename-format '' ||
    fail "set automatic-rename-format failed"
$TMUX set -w -t S:0 pane-border-status top || fail "set border status failed"
$TMUX set -w -t S:0 pane-border-style fg=red || fail "set border style failed"
$TMUX set -w -t S:0 synchronize-panes on || fail "set synchronize failed"
$TMUX set -w -t S:0 pane-scrollbars on || fail "set pane-scrollbars failed"
$TMUX set -w -t S:0 'pane-colours[3]' red || fail "set pane-colours failed"
$TMUX set -w -t S:0 'pane-colours[5]' '#102030' ||
    fail "set pane-colours failed"
$TMUX set-hook -w -t S:0 window-renamed 'set -w @renamed 1' ||
    fail "set-hook failed"

# Pane options, including an array with no items, which hides the global one.
$TMUX set -g 'pane-colours[2]' green || fail "set -g pane-colours failed"
$TMUX set -p -t S:0.0 'pane-colours[0]' red || fail "set pane-colours failed"
$TMUX set -pu -t S:0.0 'pane-colours[0]' || fail "set -u pane-colours failed"
$TMUX set -p -t S:0.1 @pane 'x' || fail "set @pane failed"
$TMUX set -p -t S:0.1 remain-on-exit on || fail "set remain-on-exit failed"
$TMUX set -p -t S:0.1 'pane-colours[1]' blue || fail "set pane-colours failed"
$TMUX set -p -t S:0.2 window-style bg=green || fail "set window-style failed"
$TMUX set-hook -p -t S:0.2 pane-focus-in 'set -p @focused 1' ||
    fail "set-hook -p failed"

$TMUX save-state -w -t S:0 $TMP/a.json || fail "save-state failed"
$TMUX load-state -w -d -t S:1 $TMP/a.json || fail "load-state failed"
same_options S:0 S:1
$TMUX save-state -w -t S:1 $TMP/b.json || fail "save-state of copy failed"
[ "$(strip $TMP/b.json)" = "$(strip $TMP/a.json)" ] ||
    fail "saving the copy gives different JSON"
grep -qF '{"name":"@empty"}' $TMP/a.json || fail "empty value not left out"
[ "$(show S:1.0 '#{pane-colours}')" = "" ] || fail "empty array not kept"
grep -qF '{"name":"pane-colours","index":"5","value":"#102030"}' $TMP/a.json ||
    fail "array item not saved by index"

# The options have their effects.
[ "$(show S:1 '#{automatic-rename}')" = 0 ] || fail "automatic-rename is on"
for i in 0 1 2; do
	[ "$(show S:1.$i '#{pane_height}')" = "$(show S:0.$i '#{pane_height}')" ] ||
	    fail "pane $i height differs with pane-border-status"
	[ "$(show S:1.$i '#{pane_width}')" = "$(show S:0.$i '#{pane_width}')" ] ||
	    fail "pane $i width differs with pane-scrollbars"
	[ "$(show S:1.$i '#{pane_synchronized}')" = 1 ] ||
	    fail "pane $i is not synchronized"
done
[ "$(show S:1.1 '#{remain-on-exit}')" = on ] || fail "remain-on-exit is off"
[ "$(show S:1.0 '#{pane-border-style}')" = fg=red ] ||
    fail "pane-border-style not inherited from the window"
$TMUX respawn-pane -t S:1.1 'exit 0' || fail "respawn-pane failed"
n=0
while [ "$(show S:1.1 '#{pane_dead}')" != 1 ]; do
	n=$((n + 1))
	[ $n -lt 50 ] || fail "pane with remain-on-exit did not stay dead"
	sleep 0.1
done
$TMUX rename-window -t S:1 other || fail "rename-window failed"
n=0
while [ "$(show S:1 '#{@renamed}')" != 1 ]; do
	n=$((n + 1))
	[ $n -lt 50 ] || fail "window hook did not run"
	sleep 0.1
done
$TMUX kill-window -t S:1

# A window with automatic-rename inherited keeps it inherited.
$TMUX new-window -d -t S:2 || fail "new-window failed"
$TMUX save-state -w -t S:2 $TMP/c.json || fail "save-state failed"
$TMUX load-state -w -d -t S:3 $TMP/c.json || fail "load-state failed"
[ "$($TMUX show -wH -t S:3)" = "" ] ||
    fail "options set on copy: $($TMUX show -wH -t S:3)"
[ "$(show S:3 '#{automatic-rename}')" = 1 ] || fail "automatic-rename is off"

# A window sized with resize-window keeps its size and window-size manual.
$TMUX new-window -d -t S:5 '' || fail "new-window failed"
$TMUX split-window -d -h -t S:5 '' || fail "split-window failed"
$TMUX resize-window -t S:5 -x 50 -y 15 || fail "resize-window failed"
$TMUX save-state -w -t S:5 $TMP/m.json || fail "save-state failed"
$TMUX new-session -d -s M -x 100 -y 30 || fail "new-session M failed"
$TMUX load-state -w -d -t M: $TMP/m.json || fail "load-state into M failed"
[ "$(show M:1 '#{window_width}x#{window_height} #{window-size}')" = \
    "50x15 manual" ] ||
    fail "manual size lost: $(show M:1 '#{window_width}x#{window_height}')"
$TMUX kill-session -t M
$TMUX kill-window -t S:5

# A file without options leaves automatic-rename off for the named window.
L1='{"V":2,"L":{"t":"p","w":80,"h":24,"x":0,"y":0,"a":true,"i":0}}'
printf '{"version":1,"window":{"name":"n","layout":%s,"panes":[{}]}}' "$L1" \
    >$TMP/n.json
$TMUX load-state -w -d -t S:4 $TMP/n.json || fail "load-state failed"
[ "$($TMUX show -wH -t S:4)" = "automatic-rename off" ] ||
    fail "options without a list: $($TMUX show -wH -t S:4)"

# Invalid options.
W='{"version":1,"window":{"layout":'"$L1"',"options":'
P='[]'
check_fail "$W"'{},"panes":[{}]}}' '"options" expected an array'
check_fail "$W"'[5],"panes":[{}]}}' 'invalid JSON'
check_fail "$W"'[{"value":"1"}],"panes":[{}]}}' 'invalid option: '
check_fail "$W"'[{"name":"nosuch"}],"panes":[{}]}}' 'invalid option: nosuch'
check_fail "$W"'[{"name":"status","value":"on"}],"panes":[{}]}}' \
    'invalid option: status'
check_fail "$W"'[{"name":"window-style","value":"bg=nope"}],"panes":[{}]}}' \
    'window-style: '
check_fail "$W"'[{"name":"remain-on-exit","value":"maybe"}],"panes":[{}]}}' \
    'remain-on-exit: '
check_fail "$W"'[{"name":"pane-border-status","index":"0","value":"top"}],"panes":[{}]}}' \
    'not an array: pane-border-status'
check_fail "$W"'[{"name":"@user","index":"0","value":"x"}],"panes":[{}]}}' \
    'not an array: @user'
check_fail "$W"'[{"name":"pane-colours","index":"3","value":"nope"}],"panes":[{}]}}' \
    'pane-colours: bad colour: nope'
check_fail "$W"'[{"name":"window-renamed","index":"0","value":"nosuchcmd"}],"panes":[{}]}}' \
    'window-renamed: '
check_fail "$W$P"',"panes":[{"options":[{"name":"automatic-rename","value":"on"}]}]}}' \
    'invalid option: automatic-rename'
check_fail "$W$P"',"panes":[{"options":[{"name":"remain-on-exit","value":"no"}]}]}}' \
    'remain-on-exit: '

exit 0
