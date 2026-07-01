#!/bin/sh

# command-alias expansion, against a running server and at server start.
#
# An alias replaces a command name with parsed command text; arguments after the
# alias name are appended to the last command of the expansion. This covers a
# single-command alias, a multi-command alias, argument appending, a built-in
# default alias, and aliases that are defined and used as the server starts
# (both from the startup config and from the client command line that starts
# the server).

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

CONF=$(mktemp)
trap "rm -f $CONF" 0 1 15

wait_gone() {
	i=0
	while $TMUX has-session 2>/dev/null; do
		sleep 0.05
		i=$((i + 1))
		[ $i -gt 100 ] && break
	done
}

# --- Against a running server. ---------------------------------------------
$TMUX -f/dev/null start \; new-session -d -swork -nbase 2>/dev/null || exit 1

# Single-command alias.
$TMUX split-window -dt work || exit 1
$TMUX set -s command-alias[200] 'zoomit=resize-pane -Z' || exit 1
$TMUX zoomit -t work:.0 || exit 1
[ "$($TMUX display-message -p -t work '#{window_zoomed_flag}')" = 1 ] || {
	echo "single-command alias did not zoom" >&2; exit 1; }

# Multi-command alias: both commands run.
$TMUX set -s command-alias[201] 'twowin=new-window -dn AA ; new-window -dn BB' || exit 1
$TMUX run-shell -C 'twowin' || exit 1
got=$($TMUX list-windows -t work -F '#{window_name}' | tr '\n' ',')
[ "$got" = "base,AA,BB," ] || { echo "multi-command alias: got [$got]" >&2; exit 1; }

# Arguments after the alias name are appended to the expansion.
$TMUX set -s command-alias[202] 'namewin=new-window -d -n' || exit 1
$TMUX run-shell -C 'namewin CC' || exit 1
$TMUX list-windows -t work -F '#{window_name}' | grep -qx CC || {
	echo "alias argument append did not create window CC" >&2; exit 1; }

# A built-in default alias (splitp -> split-window).
before=$($TMUX list-panes -t work | wc -l)
$TMUX run-shell -C 'splitp -d -t work' || exit 1
after=$($TMUX list-panes -t work | wc -l)
[ "$after" -gt "$before" ] || { echo "built-in alias splitp did not split" >&2; exit 1; }

$TMUX kill-server 2>/dev/null
wait_gone

# --- At server start: alias defined and used in the startup config. --------
cat <<'EOF' >$CONF
set -s command-alias[100] greet='set -g @g started'
set -s command-alias[101] mkwins='new-window -dn SW1 ; new-window -dn SW2'
new-session -d -swork -nbase
greet
mkwins
EOF
$TMUX -f$CONF start 2>/dev/null || exit 1
[ "$($TMUX show -gv @g)" = started ] || { echo "startup-config alias did not run" >&2; exit 1; }
got=$($TMUX list-windows -t work -F '#{window_name}' | tr '\n' ',')
[ "$got" = "base,SW1,SW2," ] || { echo "startup-config multi alias: got [$got]" >&2; exit 1; }

$TMUX kill-server 2>/dev/null
wait_gone

# --- At server start: alias from config, used on the command line that starts
#     the server. ---------------------------------------------------------
cat <<'EOF' >$CONF
set -s command-alias[100] greet='set -g @g argv'
EOF
$TMUX -f$CONF new-session -d -swork \; greet 2>/dev/null || exit 1
[ "$($TMUX show -gv @g)" = argv ] || { echo "argv-start alias did not run" >&2; exit 1; }

$TMUX kill-server 2>/dev/null
exit 0
