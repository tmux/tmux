#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$TMP/outer -f/dev/null"
TMUX2="$TEST_TMUX -S$TMP/inner -f/dev/null"

cleanup()
{
	stop_servers
	rm -rf "$TMP"
}
trap cleanup 0
trap 'exit 1' 1 2 15

mkfifo "$TMP/output" || exit 1
$TMUX2 new -d -s local -x40 -y12 'sleep 100' || exit 1
$TMUX2 new-window -d -t local:1 "exec cat '$TMP/output'" || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g focus-events on || exit 1
$TMUX2 set -as terminal-features ',screen:programstatus' || exit 1
$TMUX2 bind -n F1 'select-window -L ; select-window -t :1' || exit 1
exec 3>"$TMP/output"

# Two real terminal clients share the session; only the first selects window 1.
first=$($TMUX new -dP -F '#{pane_id}' -x40 -y12 "$TMUX2 attach -t local") || exit 1
second=$($TMUX new-window -dP -F '#{pane_id}' "$TMUX2 attach -t local") || exit 1
wait_for_clients 2 $TMUX2 || exit 1
$TMUX send-keys -t "$first" F1 || exit 1
printf '\033[2J\033[HLOCAL_INITIAL' >&3
wait_for_text LOCAL_INITIAL $TMUX capture-pane -pt "$first" || exit 1
wait_for_output 0 $TMUX2 display-message -pt local: '#{window_index}' || exit 1

# Synchronized output uses damage redraws and must update the local window.
printf '\033[?2026h\033[H\033[2KLOCAL_DAMAGE\033[?2026l' >&3
wait_for_text LOCAL_DAMAGE $TMUX capture-pane -pt "$first" || exit 1
$TMUX capture-pane -pet "$first" >"$TMP/before" || exit 1
client=$($TMUX display-message -pt "$first" '#{pane_tty}') || exit 1
$TMUX2 refresh-client -t "$client" || exit 1
sleep 0.2
$TMUX capture-pane -pet "$first" >"$TMP/after" || exit 1
cmp "$TMP/before" "$TMP/after" || exit 1

# Program status follows the client's local window on updates and refreshes.
printf '\033]7501;state=working:app=local\007' >&3
wait_for_output working $TMUX display-message -pt "$first" '#{pane_program_state}' || exit 1
$TMUX2 refresh-client -t "$client" || exit 1
sleep 0.2
wait_for_output working $TMUX display-message -pt "$first" '#{pane_program_state}' || exit 1
wait_for_output '' $TMUX display-message -pt "$second" '#{pane_program_state}' || exit 1

# Losing the local client must send focus out for its displayed window.
$TMUX2 set-hook -g pane-focus-out 'set -g @local-focus-out yes' || exit 1
$TMUX kill-pane -t "$first" || exit 1
wait_for_output yes $TMUX2 show-options -gv @local-focus-out || exit 1

exit 0
