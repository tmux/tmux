#!/bin/sh

. ./regress-common.inc

# Tests for display-panes as panes-mode.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(test_tmpdir) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	stop_server $TMUX || exit 1
	stop_server $TMUX2 || exit 1
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
	wait_for_clients "$1" $TMUX || fail "expected $1 clients"
}

$TMUX new-session -d -s m -x 80 -y 24 'cat' || exit 1
$TMUX split-window -t m:0 'cat' || exit 1
p0=$($TMUX display-message -p -t m:0.0 '#{pane_id}')
p1=$($TMUX display-message -p -t m:0.1 '#{pane_id}')
$TMUX set -g display-panes-format \
    'P#{pane_index}:#{pane_unzoomed_width}x#{pane_unzoomed_height}' ||
	fail "set display-panes-format failed"

$TMUX2 new-session -d -s out -x 80 -y 24 "$TMUX attach -t m" || exit 1
wait_clients 1
p0_size=$($TMUX display-message -p -t "$p0" '#{pane_width}x#{pane_height}')
p1_size=$($TMUX display-message -p -t "$p1" '#{pane_width}x#{pane_height}')

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
wait_format "$p0" '#{pane_unzoomed_width}x#{pane_unzoomed_height}' \
    "$p0_size"
wait_capture "P0:$p0_size"
wait_capture "P1:$p1_size"
$TMUX send-keys -t "$p0" q || fail "exit panes mode failed"
wait_format "$p0" '#{pane_in_mode}' '0'
wait_format "$p0" '#{window_zoomed_flag}' '0'
$TMUX set -g display-panes-format 'P#{pane_index}' ||
	fail "reset display-panes-format failed"

# Unzoomed sizes match the visible pane size when scrollbars reserve space.
$TMUX set -w -t m:0 pane-scrollbars on ||
	fail "set pane-scrollbars failed"
$TMUX set -w -t m:0 pane-scrollbars-style "width=2,pad=1" ||
	fail "set pane-scrollbars-style failed"
p0_size=$($TMUX display-message -p -t "$p0" '#{pane_width}x#{pane_height}')
p1_size=$($TMUX display-message -p -t "$p1" '#{pane_width}x#{pane_height}')
$TMUX set -g display-panes-format \
    'S#{pane_index}:#{pane_unzoomed_width}x#{pane_unzoomed_height}' ||
	fail "set scrollbar display-panes-format failed"
$TMUX display-panes -d 0 -t "$p0" || fail "display-panes scrollbar failed"
wait_format "$p0" '#{window_zoomed_flag}' '1'
wait_format "$p0" '#{pane_unzoomed_width}x#{pane_unzoomed_height}' \
    "$p0_size"
wait_capture "S0:$p0_size"
wait_capture "S1:$p1_size"
$TMUX send-keys -t "$p0" q || fail "exit scrollbar panes mode failed"
wait_format "$p0" '#{pane_in_mode}' '0'
$TMUX set -w -t m:0 pane-scrollbars off ||
	fail "reset pane-scrollbars failed"
$TMUX set -g display-panes-format 'P#{pane_index}' ||
	fail "reset display-panes-format after scrollbars failed"

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
# Record the mode in the same command queue so client startup cannot race
# the 500ms timeout.
$TMUX set -g @after none || fail "set @after failed"
$TMUX display-panes -Nd 500 -t "$p0" \; \
    set -gF -t "$p0" @after '#{pane_mode}' ||
	fail "display-panes immediate command failed"
wait_option @after 'panes-mode'
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
$TMUX respawn-pane -k -t "$p0" \
    'seq 1 200; printf "COPY-READY\n"; exec cat' ||
	fail "write copy-mode history failed"
wait_capture 'COPY-READY'
$TMUX copy-mode -t "$p0" || fail "copy-mode failed"
wait_format "$p0" '#{pane_mode}' 'copy-mode'
$TMUX send-keys -t "$p0" -X page-up || fail "scroll copy-mode failed"
$TMUX send-keys -t "$p0" -X begin-selection || fail "begin selection failed"
$TMUX send-keys -t "$p0" -X cursor-up || fail "extend selection failed"
copy_format='#{scroll_position}:#{copy_cursor_x}:#{copy_cursor_y}:#{selection_present}:#{selection_start_x}:#{selection_start_y}:#{selection_end_x}:#{selection_end_y}'
copy_state=$($TMUX display-message -p -t "$p0" "$copy_format")
wait_format "$p0" '#{selection_present}' '1'
copy_top=$(capture | sed -n '2p')
$TMUX display-panes -d 0 -t "$p0" 'set -g @picked %%' ||
	fail "display-panes over copy-mode failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_capture "$copy_top"
$TMUX send-keys -t "$p0" 1 || fail "select from stacked mode failed"
wait_format "$p0" '#{pane_mode}' 'copy-mode'
wait_format "$p0" "$copy_format" "$copy_state"
wait_option @picked "$p1"

# Returning at the original size must preserve state on timeout too.
$TMUX display-panes -Nd 200 -t "$p0" || fail "timed panes mode failed"
wait_format "$p0" '#{pane_mode}' 'copy-mode'
wait_format "$p0" "$copy_format" "$copy_state"

# Cancellation and drawing a different pane's mode use the same visible view.
for target in "$p0" "$p1"; do
	$TMUX display-panes -d 0 -t "$target" ||
		fail "copy-mode preview failed"
	wait_format "$target" '#{pane_mode}' 'panes-mode'
	wait_capture "$copy_top"
	$TMUX send-keys -t "$target" q || fail "cancel preview failed"
	wait_format "$p0" '#{pane_mode}' 'copy-mode'
	wait_format "$p0" "$copy_format" "$copy_state"
done
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
status_p0_size=$($TMUX display-message -p -t "$p0" \
    '#{pane_width}x#{pane_height}')
status_p1_size=$($TMUX display-message -p -t "$p1" \
    '#{pane_width}x#{pane_height}')
$TMUX set -g display-panes-format \
    '#[align=right]T#{pane_index}:#{pane_unzoomed_width}x#{pane_unzoomed_height}' ||
	fail "set status display-panes-format failed"
$TMUX respawn-pane -k -t "$p0" 'printf "STATUS-TOP\n"; exec cat' ||
	fail "write status marker failed"
wait_capture 'STATUS-TOP'
$TMUX display-panes -d 0 -t "$p0" || fail "display-panes status failed"
wait_format "$p0" '#{pane_mode}' 'panes-mode'
wait_capture 'STATUS-TOP'
wait_capture "T0:$status_p0_size"
wait_capture "T1:$status_p1_size"
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
fp_size=$($TMUX display-message -p -t "$fp" \
    '#{pane_unzoomed_width}x#{pane_unzoomed_height}')
[ "$fp_size" = "30x8" ] ||
	fail "expected floating unzoomed size 30x8, got $fp_size"
$TMUX send-keys -t "$fp" q || fail "exit zoomed floating panes mode failed"
wait_format "$fp" '#{pane_in_mode}' '0'
wait_format "$fp" '#{window_zoomed_flag}' '0'

$TMUX resize-pane -t "$fp" -x 48 -y 14 ||
	fail "resize floating pane larger failed"
$TMUX set -g display-panes-format \
    '#[align=right]FP#{pane_unzoomed_width}x#{pane_unzoomed_height}' ||
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

# Separate borders use different preview geometry. Exercise both axes, title
# positions and scrollbar sides, including entry from an already zoomed pane.
$TMUX set -g status off \; set -g window-size manual || exit 1
for axis in h v; do
	for config in separate:off:off separate:top:left separate:bottom:right \
	    separate-active:off:off separate-active:top:right \
	    separate-active:bottom:left; do
		type=${config%%:*}
		rest=${config#*:}
		status=${rest%%:*}
		side=${rest#*:}
		$TMUX new-window -t m: -n separate 'cat' \; \
		    resize-window -x 80 -y 24 \; \
		    set -w pane-border-type "$type" \; \
		    set -w pane-border-status "$status" \; \
		    set -w pane-border-format 'TITLE#{pane_index}' \; \
		    split-window -d -"$axis" 'cat' || exit 1
		op0=$($TMUX display-message -p -t m:separate.0 '#{pane_id}')
		op1=$($TMUX display-message -p -t m:separate.1 '#{pane_id}')
		if [ "$side" != off ]; then
			$TMUX set -w pane-scrollbars on \; \
			    set -w pane-scrollbars-position "$side" \; \
			    set -w pane-scrollbars-style 'width=2,pad=1' || exit 1
		fi
		$TMUX respawn-pane -k -t "$op0" \
		    'printf "TOP0\nBODY0\n"; exec cat' || exit 1
		$TMUX respawn-pane -k -t "$op1" \
		    'printf "TOP1\nBODY1\n"; exec cat' || exit 1
		wait_capture BODY0
		wait_capture BODY1
		op0_size=$($TMUX display-message -p -t "$op0" \
		    '#{pane_width}x#{pane_height}')
		op1_size=$($TMUX display-message -p -t "$op1" \
		    '#{pane_width}x#{pane_height}')
		$TMUX set -w display-panes-format \
		    'O#{pane_index}:#{pane_unzoomed_width}x#{pane_unzoomed_height}' || exit 1
		for zoom in 0 1; do
			if [ "$zoom" = 1 ]; then
				$TMUX resize-pane -Z -t "$op0" || exit 1
			fi
			op_geometry=$($TMUX list-panes -t m:separate -F \
			    '#{pane_id} #{pane_left} #{pane_top} #{pane_width} #{pane_height}')
			$TMUX display-panes -d 0 -t "$op0" || exit 1
			wait_format "$op0" '#{pane_mode}' panes-mode
			wait_format "$op0" '#{window_zoomed_flag}' 1
			# Labels and content must start immediately inside the preview
			# border, including when a left scrollbar reserved three columns.
			wait_capture "│O0:$op0_size"
			wait_capture "│O1:$op1_size"
			wait_capture '│BODY0'
			wait_capture '│BODY1'
			if [ "$axis" = h ]; then
				wait_capture '││'
			fi
			$TMUX send-keys -t "$op0" q || exit 1
			wait_format "$op0" '#{pane_in_mode}' 0
			wait_format "$op0" '#{window_zoomed_flag}' "$zoom"
			[ "$($TMUX list-panes -t m:separate -F \
			    '#{pane_id} #{pane_left} #{pane_top} #{pane_width} #{pane_height}')" = \
			    "$op_geometry" ] || fail "$config/$axis: overlay changed pane geometry"
		done
		$TMUX kill-window -t m:separate || exit 1
	done
done

# A single separate pane cannot zoom: its preview fills the area inside the
# existing border, and leaving the mode must restore that same geometry.
for type in separate separate-active; do
	$TMUX new-window -t m: -n single 'printf "TOP\nONLY\n"; exec cat' \; \
	    resize-window -x 80 -y 24 \; \
	    set -w pane-border-type "$type" \; \
	    set -w display-panes-format \
	    'O#{pane_index}:#{pane_unzoomed_width}x#{pane_unzoomed_height}' || exit 1
	only=$($TMUX display-message -p -t m:single '#{pane_id}')
	only_size=$($TMUX display-message -p -t "$only" '#{pane_width}x#{pane_height}')
	only_geometry=$($TMUX display-message -p -t "$only" \
	    '#{pane_left} #{pane_top} #{pane_width} #{pane_height}')
	wait_capture ONLY
	$TMUX display-panes -d 0 -t "$only" || exit 1
	wait_format "$only" '#{pane_mode}' panes-mode
	wait_format "$only" '#{window_zoomed_flag}' 0
	wait_capture "│O0:$only_size"
	wait_capture '│ONLY'
	$TMUX send-keys -t "$only" q || exit 1
	wait_format "$only" '#{pane_in_mode}' 0
	[ "$($TMUX display-message -p -t "$only" \
	    '#{pane_left} #{pane_top} #{pane_width} #{pane_height}')" = \
	    "$only_geometry" ] || fail "$type: overlay changed single-pane geometry"
	$TMUX kill-window -t m:single || exit 1
done

exit 0
