#!/bin/sh

# Tests for display-panes as panes-mode.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	exit 1
}

wait_format()
{
	target=$1
	format=$2
	want=$3

	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t "$target" "$format" 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "$target: expected '$want' for $format, got '$got'"
}

wait_option()
{
	option=$1
	want=$2

	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX show -gv "$option" 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "$option: expected '$want', got '$got'"
}

wait_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		c=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "expected $1 clients, have $c"
}

$TMUX new-session -d -s m -x 80 -y 24 'cat' || exit 1
$TMUX split-window -t m:0 'cat' || exit 1
p0=$($TMUX display-message -p -t m:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t m:0.1 '#{pane_id}')
$TMUX set -g display-panes-format 'P#{pane_index}' ||
	fail "set display-panes-format failed"

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t m" || exit 1
wait_clients 1

capture()
{
	$TMUX2 capture-pane -p -t out:0 2>/dev/null
}

wait_capture()
{
	i=0
	while [ "$i" -lt 50 ]; do
		CAPTURED=$(capture)
		printf '%s\n' "$CAPTURED" | grep -F -q "$1" && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for '$1' in capture"
}

# Entry, default zoom, and exit.
$TMUX display-panes -d 0 -t "$p0" || fail "display-panes failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_format "$p0" '#{window_zoomed_flag}' '1'
wait_capture 'P0'
wait_capture 'P1'
$TMUX send-keys -t "$p0" q || fail "exit panes mode failed"
wait_format "$p0" '#{pane_in_mode}' '0'
wait_format "$p0" '#{window_zoomed_flag}' '0'

# -Z starts unzoomed.
$TMUX display-panes -Zd 0 -t "$p0" || fail "display-panes -Z failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_format "$p0" '#{window_zoomed_flag}' '0'
$TMUX send-keys -t "$p0" q || fail "exit unzoomed panes mode failed"
wait_format "$p0" '#{pane_in_mode}' '0'

# Selection keys run the command template and close the mode.
$TMUX set -g @picked none || fail "set @picked failed"
$TMUX display-panes -d 0 -t "$p0" 'set -g @picked %%' ||
	fail "display-panes selection failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
$TMUX send-keys -t "$p0" 1 || fail "select pane failed"
wait_format "$p0" '#{pane_in_mode}' '0'
wait_option @picked "$p1"

# Commands after display-panes run immediately while the mode remains.
$TMUX set -g @after none || fail "set @after failed"
$TMUX display-panes -Nd 500 -t "$p0" \; set -g @after fast ||
	fail "display-panes immediate command failed"
wait_option @after fast
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_format "$p0" '#{pane_in_mode}' '0'

# Existing zoom is restored on exit.
$TMUX select-pane -t "$p0" || fail "select p0 failed"
$TMUX resize-pane -Z -t "$p0" || fail "zoom failed"
wait_format "$p0" '#{window_zoomed_flag}' '1'
$TMUX display-panes -d 0 -t "$p0" || fail "display-panes zoomed failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
$TMUX send-keys -t "$p0" q || fail "exit zoomed panes mode failed"
wait_format "$p0" '#{pane_in_mode}' '0'
wait_format "$p0" '#{window_zoomed_flag}' '1'
$TMUX resize-pane -Z -t "$p0" || fail "unzoom cleanup failed"

# Panes mode can be stacked above another mode and returns to it on exit.
$TMUX copy-mode -t "$p0" || fail "copy-mode failed"
wait_format "$p0" '#{pane_mode}' 'copy-mode'
$TMUX display-panes -d 0 -t "$p0" 'set -g @picked %%' ||
	fail "display-panes over copy-mode failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
$TMUX send-keys -t "$p0" 1 || fail "select from stacked mode failed"
wait_format "$p0" '#{pane_mode}' 'copy-mode'
wait_option @picked "$p1"
$TMUX send-keys -t "$p0" -X cancel || fail "copy-mode cancel failed"
wait_format "$p0" '#{pane_in_mode}' '0'

# Panes mode is not kept underneath another mode.
$TMUX display-panes -d 0 -t "$p0" || fail "display-panes no-stack failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
$TMUX copy-mode -t "$p0" || fail "copy-mode over panes-mode failed"
wait_format "$p0" '#{pane_mode}' 'copy-mode'
$TMUX send-keys -t "$p0" -X cancel || fail "copy-mode cancel failed"
wait_format "$p0" '#{pane_in_mode}' '0'
wait_format "$p0" '#{window_zoomed_flag}' '0'

# -s shows panes from a different source window but enters the mode in the
# target pane.
$TMUX new-window -d -t m: -n src 'cat' || fail "new source window failed"
$TMUX split-window -t m:src 'cat' || fail "split source window failed"
sp0=$($TMUX display-message -p -t m:src.0 '#{pane_id}')
sp1=$($TMUX display-message -p -t m:src.1 '#{pane_id}')
$TMUX respawn-pane -k -t "$sp0" 'printf "SOURCE-PANE\n"; exec cat' ||
	fail "write source marker failed"
$TMUX set -g display-panes-format '' || fail "clear source display format failed"
$TMUX set -w -t m:src display-panes-format 'SOURCE-ONLY' ||
	fail "set source window display format failed"
$TMUX set -g @picked none || fail "reset @picked failed"
$TMUX display-panes -d 0 -s m:src -t "$p0" 'set -g @picked %%' ||
	fail "display-panes source window failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_capture 'SOURCE-PANE'
CAPTURED=$(capture)
printf '%s\n' "$CAPTURED" | grep -F -q 'SOURCE-ONLY' &&
	fail "used source window display-panes-format"
$TMUX send-keys -t "$p0" 1 || fail "select source pane failed"
wait_format "$p0" '#{pane_in_mode}' '0'
wait_option @picked "$sp1"
$TMUX kill-window -t m:src || fail "kill source window failed"
$TMUX set -g display-panes-format 'P#{pane_index}' ||
	fail "reset display-panes-format after source failed"

# pane-border-status rows are drawn as plain preview borders and do not shift
# pane content into the status row.
$TMUX set -w -t m:0 pane-border-status top ||
	fail "set pane-border-status failed"
$TMUX set -g display-panes-format '' || fail "clear display-panes-format failed"
$TMUX respawn-pane -k -t "$p0" 'printf "STATUS-TOP\n"; exec cat' ||
	fail "write status marker failed"
wait_capture 'STATUS-TOP'
$TMUX display-panes -d 0 -t "$p0" || fail "display-panes status failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_capture 'STATUS-TOP'
CAPTURED=$(capture)
first=$(printf '%s\n' "$CAPTURED" | sed -n '1p')
second=$(printf '%s\n' "$CAPTURED" | sed -n '2p')
printf '%s\n' "$first" | grep -F -q 'STATUS-TOP' &&
	fail "pane content drawn in status border row"
printf '%s\n' "$second" | grep -F -q 'STATUS-TOP' ||
	fail "pane content not drawn below status border row"
$TMUX send-keys -t "$p0" q || fail "exit status panes mode failed"
wait_format "$p0" '#{pane_in_mode}' '0'
$TMUX set -w -t m:0 pane-border-status off ||
	fail "reset pane-border-status failed"
$TMUX set -g display-panes-format 'P#{pane_index}' ||
	fail "reset display-panes-format failed"

# Floating panes are always framed in panes-mode, independent of their real
# pane-border-lines setting.
fp=$($TMUX new-pane -dPF '#{pane_id}' -B none -x 30 -y 8 -X 8 -Y 3 \
    -t m:0 'cat') || fail "new borderless floating pane failed"
$TMUX set -g display-panes-format '' || fail "clear display-panes-format failed"
$TMUX display-panes -Zd 0 -t "$fp" || fail "display-panes floating failed"
wait_format "$fp" '#{pane_mode}' 'panes-mode'
wait_capture '┌'
wait_capture '┘'
$TMUX send-keys -t "$fp" q || fail "exit floating panes mode failed"
wait_format "$fp" '#{pane_in_mode}' '0'

$TMUX display-panes -d 0 -t "$fp" || fail "display-panes floating zoomed failed"
wait_format "$fp" '#{pane_mode}' 'panes-mode'
wait_format "$fp" '#{window_zoomed_flag}' '1'
wait_capture '┌'
wait_capture '┘'
$TMUX send-keys -t "$fp" q || fail "exit zoomed floating panes mode failed"
wait_format "$fp" '#{pane_in_mode}' '0'
wait_format "$fp" '#{window_zoomed_flag}' '0'

$TMUX resize-pane -t "$fp" -x 48 -y 14 ||
	fail "resize floating pane larger failed"
$TMUX set -g display-panes-format '#[align=right]FP#{pane_width}x#{pane_height}' ||
	fail "set floating display-panes-format failed"
$TMUX display-panes -Zd 0 -t "$fp" || fail "display-panes floating format failed"
wait_format "$fp" '#{pane_mode}' 'panes-mode'
wait_capture 'FP48x14'
$TMUX resize-pane -t "$fp" -x 30 -y 10 ||
	fail "resize floating pane smaller failed"
wait_capture 'FP30x10'
$TMUX send-keys -t "$fp" q || fail "exit floating format panes mode failed"
wait_format "$fp" '#{pane_in_mode}' '0'

$TMUX kill-pane -t "$fp" || fail "kill floating pane failed"
$TMUX set -g display-panes-format 'P#{pane_index}' ||
	fail "reset display-panes-format after floating failed"

exit 0
