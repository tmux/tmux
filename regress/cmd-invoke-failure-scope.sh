#!/bin/sh

# Command failure scope.
#
# Failure scope is the active sequence (CMD_PARSE_SEQUENCE): commands joined by
# ';' share one scope, so a failure skips the rest of that sequence; commands on
# separate lines are independent sequences and are not skipped. After a failed
# inner brace the enclosing sequence still resumes.
#
# Each case below creates windows around a deliberately invalid command and the
# resulting window list shows exactly which commands ran.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
CONF=$(mktemp)
trap "rm -f $TMP $CONF" 0 1 15

# kill-server is asynchronous, so wait for the old server to actually exit
# before starting a fresh one (otherwise the next start races a dying server).
fresh_server() {
	$TMUX kill-server 2>/dev/null
	i=0
	while $TMUX list-sessions >/dev/null 2>&1; do
		sleep 0.05
		i=$((i + 1))
		[ $i -gt 100 ] && break
	done
	$TMUX -f/dev/null start \; new-session -d -swork -nbase || exit 1
}

# $1 label, $2 config body, $3 expected comma-separated window list.
run() {
	fresh_server
	printf '%s\n' "$2" >$CONF
	$TMUX source-file $CONF >/dev/null 2>&1
	got=$($TMUX list-windows -t work -F '#{window_name}' | tr '\n' ',')
	if [ "$got" != "$3" ]; then
		echo "$1: got [$got] expected [$3]" >&2
		exit 1
	fi
}

# ';' sequence in a brace body: A runs, the bad command skips B (same sequence),
# the outer sequence resumes so C runs.
run "semicolon body" 'if-shell true { new-window -dn A ; nonexistent_cmd ; new-window -dn B }
new-window -dn C' 'base,A,C,'

# Newlines are independent sequences: the bad command skips only itself, so both
# B and C still run.
run "newline body" 'if-shell true {
  new-window -dn A
  nonexistent_cmd
  new-window -dn B
}
new-window -dn C' 'base,A,B,C,'

# Top-level ';' sequence: failure skips the rest of the line.
run "top-level semicolon" 'new-window -dn A ; nonexistent_cmd ; new-window -dn B' 'base,A,'

# Top-level newlines: independent, both run.
run "top-level newline" 'new-window -dn A
nonexistent_cmd
new-window -dn B' 'base,A,B,'

$TMUX kill-server 2>/dev/null
exit 0
