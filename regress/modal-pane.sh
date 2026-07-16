#!/bin/sh

# Tests for modal floating panes.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	$TMUX2 kill-server >/dev/null 2>&1
}

fail()
{
	echo "$*" >&2
	cleanup
	exit 1
}

must_equal()
{
	got=$1
	want=$2
	[ "$got" = "$want" ] || fail "got '$got', expected '$want'"
}

check_ok()
{
	$TMUX "$@" || fail "command failed: $*"
}

check_fail()
{
	exp="$1"
	shift
	out=$($TMUX "$@" 2>&1)
	if [ $? -eq 0 ]; then
		fail "command succeeded (expected failure): $*"
	fi
	must_equal "$out" "$exp"
}

fmt()
{
	$TMUX display-message -p -t "$1" "$2"
}

click()
{
	col="$1"
	row="$2"
	seq=$(printf '\033[<0;%s;%sM\033[<0;%s;%sm' \
	    "$col" "$row" "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

move_mouse()
{
	col="$1"
	row="$2"
	seq=$(printf '\033[<35;%s;%sM' "$col" "$row")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

ctrl_drag()
{
	scol="$1"
	srow="$2"
	ecol="$3"
	erow="$4"

	seq=$(printf '\033[<16;%s;%sM' "$scol" "$srow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<48;%s;%sM' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<16;%s;%sm' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

drag()
{
	scol="$1"
	srow="$2"
	ecol="$3"
	erow="$4"

	seq=$(printf '\033[<0;%s;%sM' "$scol" "$srow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<32;%s;%sM' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 0.2
	seq=$(printf '\033[<0;%s;%sm' "$ecol" "$erow")
	$TMUX2 send-keys -t "$OUTER" -l "$seq" 2>/dev/null
	sleep 1
}

cleanup

check_ok new-session -d -s modal -x 80 -y 24 'cat'
sleep 1
p0=$(fmt modal:0 '#{pane_id}')
check_ok split-window -h -t "$p0" 'cat'
sleep 1
p1=$(fmt modal:0 '#{pane_id}')

check_ok select-pane -t "$p0"
check_ok resize-pane -Z -t "$p0"
must_equal "$(fmt modal:0 '#{window_zoomed_flag}')" 1

modal=$($TMUX new-pane -OPF '#{pane_id}' -t "$p1" \
    -x 20 -y 5 -X 20 -Y 10 'cat') ||
	fail "new-pane -O failed"
sleep 1
must_equal "$(fmt "$modal" '#{pane_floating_flag}:#{pane_modal_flag}:#{pane_active}')" 1:1:1
case "$(fmt "$modal" '#{pane_flags}')" in
*O*) ;;
*) fail "modal pane flags do not include O" ;;
esac
case "$(fmt modal:0 '#{window_flags}')" in
*O*) ;;
*) fail "modal window flags do not include O" ;;
esac
must_equal "$(fmt modal:0 '#{window_modal_pane}')" "$modal"
must_equal "$(fmt modal:0 '#{window_zoomed_flag}')" 0

check_fail "window already has a modal pane" \
	new-pane -O -x 10 -y 4 'cat'
check_fail "modal pane must be floating" \
	new-pane -O -L 'cat'
check_fail "pane is modal" \
	break-pane -s "$modal"
check_fail "pane is modal" \
	join-pane -s "$modal" -t "$p0"
check_fail "pane is modal" \
	join-pane -s "$p0" -t "$modal"
check_fail "pane is modal" \
	swap-pane -s "$modal" -t "$p0"
check_fail "pane is modal" \
	swap-pane -s "$p0" -t "$modal"

check_ok select-pane -t "$p1"
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"
check_ok last-pane -t modal:0
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"

under=$($TMUX split-window -PF '#{pane_id}' -t "$p0" 'cat') ||
	fail "split-window under modal failed"
sleep 1
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"
must_equal "$(fmt "$under" '#{pane_active}')" 0

float=$($TMUX new-pane -PF '#{pane_id}' -x 10 -y 4 -X 5 -Y 3 'cat') ||
	fail "new floating pane under modal failed"
sleep 1
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"
must_equal "$(fmt "$float" '#{pane_active}')" 0

check_ok new-window -d -t modal: -n other 'cat'
check_ok select-window -t modal:other
other=$(fmt modal:other '#{pane_id}')
must_equal "$(fmt modal:other '#{pane_id}')" "$other"
check_ok select-window -t modal:0
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"

check_ok send-keys -t modal:0 'modal-key' Enter
sleep 1
case "$($TMUX capture-pane -pt "$modal")" in
*modal-key*) ;;
*) fail "keyboard input did not reach modal pane" ;;
esac
case "$($TMUX capture-pane -pt "$p0")" in
*modal-key*) fail "keyboard input reached pane below modal" ;;
esac

$TMUX set -g mouse on
$TMUX set -g focus-follows-mouse on
$TMUX set -g @modal-mouse ''
$TMUX bind -n MouseDown1Pane run-shell \
    "$TMUX set -g @modal-mouse '#{mouse_pane}'"
$TMUX bind x set -g @modal-prefix yes

$TMUX2 new-session -d -x 80 -y 24 "$TMUX attach -t modal" ||
	fail "outer session failed"
sleep 1
OUTER=$($TMUX2 list-panes -F '#{pane_id}' | head -1)
[ -n "$OUTER" ] || fail "no outer pane"

click 1 1
must_equal "$($TMUX show -gv @modal-mouse)" ''
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"

panes=$(fmt modal:0 '#{window_panes}')
ctrl_drag 1 1 8 3
must_equal "$(fmt modal:0 '#{window_panes}')" "$panes"
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"

move_mouse 1 1
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"

left=$(fmt "$modal" '#{pane_left}')
top=$(fmt "$modal" '#{pane_top}')
click $((left + 1)) $((top + 1))
must_equal "$($TMUX show -gv @modal-mouse)" "$modal"
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"

width=$(fmt "$modal" '#{pane_width}')
right=$((left + width + 1))
drag "$right" $((top + 1)) $((right + 5)) $((top + 1))
new_width=$(fmt "$modal" '#{pane_width}')
[ "$new_width" -gt "$width" ] ||
	fail "modal pane did not grow after right-border drag"

$TMUX2 send-keys -t "$OUTER" C-b x
sleep 1
must_equal "$($TMUX show -gv @modal-prefix)" yes

$TMUX set-buffer -b modal-edit-test 'test'
check_ok choose-buffer -t "$modal"
panes=$(fmt modal:0 '#{window_panes}')
$TMUX2 send-keys -t "$OUTER" e
sleep 1
must_equal "$(fmt modal:0 '#{window_panes}')" "$panes"
must_equal "$(fmt modal:0 '#{window_modal_pane}')" "$modal"
$TMUX2 send-keys -t "$OUTER" q
sleep 1

check_ok kill-pane -t "$modal"
sleep 1
must_equal "$(fmt modal:0 '#{window_modal_pane}')" ''
case "$(fmt modal:0 '#{window_flags}')" in
*O*) fail "modal window flag remained after modal pane closed" ;;
esac
must_equal "$(fmt modal:0 '#{pane_id}')" "$p0"
must_equal "$(fmt modal:0 '#{window_zoomed_flag}')" 1
check_ok resize-pane -Z -t "$p0"
must_equal "$(fmt modal:0 '#{window_zoomed_flag}')" 0

$TMUX set -g editor 'sh -c "sleep 10" sh'
check_ok resize-pane -Z -t "$p0"
check_ok choose-buffer -t "$p0"
panes=$(fmt modal:0 '#{window_panes}')
$TMUX2 send-keys -t "$OUTER" e
sleep 1
editor=$(fmt modal:0 '#{window_modal_pane}')
[ -n "$editor" ] || fail "buffer editor did not open as modal pane"
must_equal "$(fmt modal:0 '#{window_panes}')" $((panes + 1))
must_equal "$(fmt "$editor" '#{pane_modal_flag}:#{pane_active}')" 1:1
check_ok kill-pane -t "$editor"
sleep 1
must_equal "$(fmt modal:0 '#{window_modal_pane}')" ''
must_equal "$(fmt modal:0 '#{pane_id}')" "$p0"
must_equal "$(fmt modal:0 '#{window_zoomed_flag}')" 1
check_ok resize-pane -Z -t "$p0"
must_equal "$(fmt modal:0 '#{window_zoomed_flag}')" 0

detached=$($TMUX new-pane -OdPF '#{pane_id}' -x 20 -y 5 -X 20 -Y 10 \
    'cat') || fail "new detached modal failed"
sleep 1
must_equal "$(fmt "$detached" '#{pane_modal_flag}:#{pane_active}')" 1:1
must_equal "$(fmt modal:0 '#{window_modal_pane}')" "$detached"
check_ok kill-pane -t "$detached"
sleep 1
must_equal "$(fmt modal:0 '#{window_modal_pane}')" ''
must_equal "$(fmt modal:0 '#{pane_id}')" "$p0"

$TMUX set -g @modal-custom old
check_ok customize-mode -t "$p0" \
	-f '#{==:#{option_name},@modal-custom}'
panes=$(fmt modal:0 '#{window_panes}')
$TMUX2 send-keys -t "$OUTER" j Right j e
sleep 1
editor=$(fmt modal:0 '#{window_modal_pane}')
[ -n "$editor" ] || fail "customize editor did not open as modal pane"
must_equal "$(fmt modal:0 '#{window_panes}')" $((panes + 1))
must_equal "$(fmt "$editor" '#{pane_modal_flag}:#{pane_active}')" 1:1
check_ok kill-pane -t "$editor"
sleep 1
must_equal "$(fmt modal:0 '#{window_modal_pane}')" ''
$TMUX2 send-keys -t "$OUTER" q
sleep 1

modal=$($TMUX new-pane -OkPF '#{pane_id}' -x 20 -y 5 -X 20 -Y 10 'printf done') ||
	fail "new retained modal failed"
sleep 2
must_equal "$(fmt "$modal" '#{pane_dead}:#{pane_modal_flag}:#{pane_active}')" 1:1:1
check_ok respawn-pane -k -t "$modal" 'cat'
sleep 1
must_equal "$(fmt "$modal" '#{pane_dead}:#{pane_modal_flag}:#{pane_active}')" 0:1:1
check_ok select-pane -t "$p1"
must_equal "$(fmt modal:0 '#{pane_id}')" "$modal"
check_ok kill-pane -t "$modal"
sleep 1
must_equal "$(fmt modal:0 '#{window_modal_pane}')" ''
must_equal "$(fmt modal:0 '#{pane_id}')" "$p0"

cleanup
exit 0
