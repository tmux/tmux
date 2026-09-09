#!/bin/sh

# Cover prompt word movement in emacs and vi modes, ambiguous inline command
# completion, and the show/clear-prompt-history command surface.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

capture()
{
	$OUTER capture-pane -p -t outer:0.0 2>/dev/null
}

wait_result()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER show-option -gqv @result 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "prompt result is '$got', expected '$want'"
}

bind_prompt()
{
	initial=$1
	$INNER bind-key -n M-r command-prompt -I "$initial" -p '(word)' \
		"set-option -g @result '%%'" || exit 1
}

run_prompt()
{
	want=$1
	shift
	$INNER set-option -g @result sentinel || exit 1
	$OUTER send-keys M-r || exit 1
	sleep 0.2
	$OUTER send-keys "$@" || exit 1
	$OUTER send-keys Enter || exit 1
	wait_result "$want"
}

run_vi_prompt()
{
	want=$1
	shift
	$INNER set-option -g @result sentinel || exit 1
	$OUTER send-keys M-r || exit 1
	sleep 0.2
	# Send Escape separately so the terminal's escape-time handling does not
	# combine it with the first vi command as a Meta key.
	$OUTER send-keys Escape || exit 1
	# The inner client must see Escape as a complete key, not the prefix of a
	# Meta sequence (the default escape-time is 500 milliseconds).
	sleep 0.7
	$OUTER send-keys "$@" || exit 1
	$OUTER send-keys Enter || exit 1
	wait_result "$want"
}

$INNER new-session -d -s prompt -x80 -y24 'exec sleep 100' || exit 1
$INNER set-option -g status on || exit 1
$INNER set-option -g status-position bottom || exit 1
$INNER set-option -g window-size manual || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t prompt" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
sleep 1

# Emacs Meta-f stops after the first word; Meta-b returns to the start of the
# previous word. Inserting a marker makes the cursor position observable.
$INNER set-option -g status-keys emacs || exit 1
bind_prompt 'one two'
run_prompt 'oneX two' Home M-f X
bind_prompt 'one two'
run_prompt 'one Xtwo' M-b X

# Vi translation and the distinct separator-aware and WORD motions.
$INNER set-option -g status-keys vi || exit 1
bind_prompt 'one-two three'
run_vi_prompt 'one-two Xthree' b i X
bind_prompt 'one-two three'
run_vi_prompt 'one-two Xthree' B i X
bind_prompt 'one-two three'
run_vi_prompt 'oneX-two three' 0 w i X
bind_prompt 'one-two three'
run_vi_prompt 'one-two Xthree' 0 W i X
bind_prompt 'one-two three'
run_vi_prompt 'oneX-two three' 0 e a X
bind_prompt 'one-two three'
run_vi_prompt 'one-twoX three' 0 E a X

# "show-" has several command matches and no longer common prefix. Tab keeps
# the input and draws the sorted candidates inline.
$INNER set-option -g status-keys emacs || exit 1
bind_prompt ''
$OUTER send-keys M-r || exit 1
sleep 0.2
$OUTER send-keys -l 'show-' || exit 1
$OUTER send-keys Tab || exit 1
sleep 0.2
captured=$(capture)
printf '%s\n' "$captured" | grep -Fq 'show-buffer' ||
	fail "ambiguous completion list was not drawn"
printf '%s\n' "$captured" | grep -Fq 'show-environment' ||
	fail "ambiguous completion list was incomplete"
$OUTER send-keys Escape || exit 1

# Add entries to both history rings through real prompts.
bind_prompt 'history-command'
run_prompt 'history-command'
$INNER bind-key -n M-s command-prompt -T search -I history-search \
	-p '(search-history)' "set-option -g @result '%%'" || exit 1
$OUTER send-keys M-s || exit 1
$OUTER send-keys Enter || exit 1
wait_result history-search

command_history=$($INNER show-prompt-history -T command) || exit 1
printf '%s\n' "$command_history" | grep -Fq 'history-command' ||
	fail "command history entry missing"
search_history=$($INNER show-prompt-history -T search) || exit 1
printf '%s\n' "$search_history" | grep -Fq 'history-search' ||
	fail "search history entry missing"
all_history=$($INNER show-prompt-history) || exit 1
printf '%s\n' "$all_history" | grep -Fq 'History for command:' || exit 1
printf '%s\n' "$all_history" | grep -Fq 'History for search:' || exit 1

$INNER show-prompt-history -T invalid >/dev/null 2>&1 &&
	fail "invalid show history type succeeded"
$INNER clear-prompt-history -T invalid >/dev/null 2>&1 &&
	fail "invalid clear history type succeeded"
$INNER clear-prompt-history -T command || exit 1
$INNER show-prompt-history -T command | grep -Fq history-command &&
	fail "type-specific history clear failed"
$INNER clear-prompt-history || exit 1
$INNER show-prompt-history | grep -Fq history-search &&
	fail "all-history clear failed"

exit 0
