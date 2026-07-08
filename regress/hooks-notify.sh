#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT=$(mktemp -d)
TMUX_TMPDIR="$OUT"
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null || true
	rm -rf "$OUT"
}
trap cleanup EXIT

wait_for()
{
	option=$1
	expected=$2
	i=0

	while [ $i -lt 30 ]; do
		value=$($TMUX show -gqv "$option" 2>/dev/null || true)
		[ "$value" = "$expected" ] && return 0
		i=$((i + 1))
		sleep 0.2
	done
	fail "expected $option to be '$expected' but got '$value'"
}

$TMUX new -d -s main || fail "new-session main failed"

# session-created, session-renamed, session-closed.
$TMUX set -g @s 0 || fail "set @s failed"
$TMUX set-hook -g session-created \
	'set -gF @s "#{hook}:#{hook_session_name}"' ||
	fail "set-hook session-created failed"
$TMUX new -d -s tmp || fail "new-session tmp failed"
wait_for @s 'session-created:tmp'
$TMUX set-hook -gu session-created || fail "unset session-created failed"

$TMUX set-hook -g session-renamed \
	'set -gF @s "#{hook}:#{hook_session_name}"' ||
	fail "set-hook session-renamed failed"
$TMUX rename-session -t tmp tmp2 || fail "rename-session failed"
wait_for @s 'session-renamed:tmp2'
$TMUX set-hook -gu session-renamed || fail "unset session-renamed failed"

$TMUX set-hook -g session-closed \
	'set -gF @s "#{hook}:#{hook_session_name}"' ||
	fail "set-hook session-closed failed"
$TMUX kill-session -t tmp2 || fail "kill-session failed"
wait_for @s 'session-closed:tmp2'
$TMUX set-hook -gu session-closed || fail "unset session-closed failed"

# window-linked and window-unlinked from new-window and kill-window.
$TMUX set -g @w 0 || fail "set @w failed"
$TMUX set-hook -g window-linked \
	'set -gF @w "#{hook}:#{hook_session_name}:#{hook_window_name}"' ||
	fail "set-hook window-linked failed"
$TMUX set-hook -g window-unlinked \
	'set -gF @w "#{hook}:#{hook_session_name}:#{hook_window_name}"' ||
	fail "set-hook window-unlinked failed"
$TMUX neww -d -t main: -n mywin || fail "new-window failed"
wait_for @w 'window-linked:main:mywin'

# window-linked and window-unlinked from link-window and unlink-window.
$TMUX new -d -s other || fail "new-session other failed"
$TMUX set -g @w 0 || fail "reset @w failed"
$TMUX link-window -s main:mywin -t other:5 || fail "link-window failed"
wait_for @w 'window-linked:other:mywin'
$TMUX set -g @w 0 || fail "reset @w failed"
$TMUX unlink-window -t other:5 || fail "unlink-window failed"
wait_for @w 'window-unlinked:other:mywin'
$TMUX set -g @w 0 || fail "reset @w failed"
$TMUX kill-window -t main:mywin || fail "kill-window failed"
wait_for @w 'window-unlinked:main:mywin'
$TMUX set-hook -gu window-linked || fail "unset window-linked failed"
$TMUX set-hook -gu window-unlinked || fail "unset window-unlinked failed"

# window-renamed sees the window id.
window=$($TMUX display -pt main:0 '#{window_id}') ||
	fail "display window_id failed"
$TMUX set -g @r 0 || fail "set @r failed"
$TMUX set-hook -g window-renamed \
	'set -gF @r "#{hook}:#{hook_window}:#{hook_window_name}"' ||
	fail "set-hook window-renamed failed"
$TMUX rename-window -t main:0 renamed || fail "rename-window failed"
wait_for @r "window-renamed:$window:renamed"
$TMUX set-hook -gu window-renamed || fail "unset window-renamed failed"

# window-layout-changed from split-window.
$TMUX set -g @l 0 || fail "set @l failed"
$TMUX set-hook -g window-layout-changed \
	'set -gF @l "#{hook}:#{hook_window}"' ||
	fail "set-hook window-layout-changed failed"
$TMUX splitw -d -t main:0 || fail "split-window failed"
wait_for @l "window-layout-changed:$window"
$TMUX set-hook -gu window-layout-changed ||
	fail "unset window-layout-changed failed"

# window-pane-changed from select-pane.
$TMUX set -g @p 0 || fail "set @p failed"
$TMUX set-hook -g window-pane-changed \
	'set -gF @p "#{hook}:#{hook_window}"' ||
	fail "set-hook window-pane-changed failed"
$TMUX selectp -t main:0.1 || fail "select-pane failed"
wait_for @p "window-pane-changed:$window"
$TMUX set-hook -gu window-pane-changed ||
	fail "unset window-pane-changed failed"

# session-window-changed from select-window.
$TMUX neww -d -t main: -n w2 || fail "new-window w2 failed"
$TMUX set -g @c 0 || fail "set @c failed"
$TMUX set-hook -g session-window-changed \
	'set -gF @c "#{hook}:#{hook_session_name}"' ||
	fail "set-hook session-window-changed failed"
$TMUX selectw -t main:w2 || fail "select-window failed"
wait_for @c 'session-window-changed:main'
$TMUX set-hook -gu session-window-changed ||
	fail "unset session-window-changed failed"

# pane-mode-changed from entering and leaving copy mode.
$TMUX set -g @m 0 || fail "set @m failed"
pane=$($TMUX display -pt main:0.0 '#{pane_id}') ||
	fail "display pane_id failed"
$TMUX set-hook -g pane-mode-changed \
	'set -gF @m "#{hook}:#{hook_pane}:#{pane_in_mode}"' ||
	fail "set-hook pane-mode-changed failed"
$TMUX copy-mode -t main:0.0 || fail "copy-mode failed"
wait_for @m "pane-mode-changed:$pane:1"
$TMUX send-keys -t main:0.0 -X cancel || fail "cancel failed"
wait_for @m "pane-mode-changed:$pane:0"
$TMUX set-hook -gu pane-mode-changed || fail "unset pane-mode-changed failed"

# pane-exited when a pane's command exits with remain-on-exit off.
$TMUX set -g @x 0 || fail "set @x failed"
$TMUX set-hook -g pane-exited 'set -gF @x "#{hook}:#{hook_pane}"' ||
	fail "set-hook pane-exited failed"
pane=$($TMUX splitw -d -t main:0 -P -F '#{pane_id}' 'true') ||
	fail "split-window true failed"
wait_for @x "pane-exited:$pane"
$TMUX set-hook -gu pane-exited || fail "unset pane-exited failed"

# pane-died when a pane's command exits with remain-on-exit on.
$TMUX set -g remain-on-exit on || fail "set remain-on-exit failed"
$TMUX set -g @d 0 || fail "set @d failed"
$TMUX set-hook -g pane-died 'set -gF @d "#{hook}:#{hook_pane}"' ||
	fail "set-hook pane-died failed"
pane=$($TMUX splitw -d -t main:0 -P -F '#{pane_id}' 'true') ||
	fail "split-window remain-on-exit failed"
wait_for @d "pane-died:$pane"
$TMUX set-hook -gu pane-died || fail "unset pane-died failed"
$TMUX killp -t "$pane" || fail "kill-pane failed"
$TMUX set -g remain-on-exit off || fail "reset remain-on-exit failed"

# pane-title-changed when a pane sets its title.
$TMUX set -g @t 0 || fail "set @t failed"
$TMUX set-hook -g pane-title-changed \
	'set -gF @t "#{hook}:#{hook_pane}:#{pane_title}"' ||
	fail "set-hook pane-title-changed failed"
pane=$($TMUX splitw -d -t main:0 -P -F '#{pane_id}' \
	'printf "\033]2;mytitle\007"; sleep 30') ||
	fail "split-window title failed"
wait_for @t "pane-title-changed:$pane:mytitle"
$TMUX set-hook -gu pane-title-changed ||
	fail "unset pane-title-changed failed"
$TMUX killp -t "$pane" || fail "kill-pane title failed"

# client-attached and client-detached using a control client.
$TMUX set -g @a 0 || fail "set @a failed"
$TMUX set-hook -g client-attached 'set -gF @a "#{hook}"' ||
	fail "set-hook client-attached failed"
$TMUX set-hook -g client-detached 'set -gF @a "#{hook}"' ||
	fail "set-hook client-detached failed"
mkfifo "$OUT/fifo" || fail "mkfifo failed"
$TMUX -C attach -t main <"$OUT/fifo" >"$OUT/control.out" 2>&1 &
exec 3>"$OUT/fifo"
wait_for @a 'client-attached'
exec 3>&-
wait_for @a 'client-detached'
$TMUX set-hook -gu client-attached || fail "unset client-attached failed"
$TMUX set-hook -gu client-detached || fail "unset client-detached failed"

exit 0
