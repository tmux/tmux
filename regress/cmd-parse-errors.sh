#!/bin/sh

# Malformed input rejection and crash/leak canary.
#
# Each input below is a syntax error. For every one we require that:
#   - parsing reports failure (non-zero exit), and
#   - the server is still alive afterwards (a follow-up command succeeds).
# The second check is the important one: a parser that crashes or corrupts state
# on bad input would take the server down here.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

$TMUX -f/dev/null start \; new-session -d 2>/dev/null || exit 1

n=0
check() {
	n=$((n + 1))
	# Expect a parse failure.
	if $TMUX run-shell -C "$1" >/dev/null 2>&1; then
		echo "case $n: expected failure but succeeded: $1" >&2
		exit 1
	fi
	# Expect the server to still be responsive.
	if ! $TMUX list-sessions >/dev/null 2>&1; then
		echo "case $n: server died after: $1" >&2
		exit 1
	fi
}

check 'if-shell true {'			# unterminated brace body
check 'display-message a }'		# stray close brace
check 'if-shell true { display-message a'	# unclosed nested brace
check 'if-shell true { ; }'		# empty sequence in body
check '%elif 1'				# %elif with no %if
check '%endif'				# %endif with no %if
check '%zzz'				# unknown % directive
check '; display-message a'		# leading separator
input='%if 1
display-message a'
check "$input"				# unterminated %if
input='%if 1
%endif'
check "$input"				# empty %if body

$TMUX kill-server 2>/dev/null
exit 0
