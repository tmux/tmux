#!/bin/sh

# Human-readable command help and terminal-aware output.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

fail()
{
	echo "$*"
	$TMUX kill-server 2>/dev/null
	exit 1
}

contains()
{
	printf '%s\n' "$1" | grep -q "$2" || fail "missing '$2'"
}

not_contains()
{
	printf '%s\n' "$1" | grep -q "$2" && fail "unexpected '$2'"
	return 0
}

global_help=$($TEST_TMUX -h) || fail "tmux -h failed"
contains "$global_help" '^Common commands$'
contains "$global_help" 'tmux help <command>'

$TMUX new-session -d -x 80 -y 24 -s visual-test || exit 1
$TMUX set-buffer -b sample 'a useful buffer preview' || exit 1

# These compatibility checks keep the interactive presentation changes from
# altering output used by scripts. Pipes retain the traditional defaults.
legacy=$($TMUX list-commands) || fail "legacy list-commands failed"
contains "$legacy" '^attach-session (attach) \['
not_contains "$legacy" '^Sessions & Server$'

legacy=$($TMUX list-sessions) || fail "legacy list-sessions failed"
contains "$legacy" '^visual-test: 1 windows'
not_contains "$legacy" '^NAME  *WINDOWS'

# Explicit formats always win and remain exact.
formatted=$($TMUX list-sessions -F '#{session_name}|#{session_windows}') ||
	fail "formatted list-sessions failed"
[ "$formatted" = 'visual-test|1' ] ||
	fail "custom session format changed: $formatted"

formatted=$($TMUX list-commands -F \
	'#{command_list_name}|#{command_list_alias}' help) ||
	fail "formatted list-commands failed"
[ "$formatted" = 'help|' ] ||
	fail "custom command format changed: $formatted"

# Detailed help resolves canonical names and aliases and includes examples.
help=$($TMUX help split-window) || fail "help split-window failed"
contains "$help" '^NAME$'
contains "$help" '^USAGE$'
contains "$help" '^EXAMPLES$'
contains "$help" 'tmux split-window -h'

alias_help=$($TMUX help splitw) || fail "help alias failed"
[ "$alias_help" = "$help" ] || fail "alias help differs from canonical help"

# Every registered command must have a usable help entry.
commands=$($TMUX list-commands -F '#{command_list_name}') ||
	fail "could not enumerate commands"
[ "$(printf '%s\n' "$commands" | wc -l | tr -d ' ')" = 93 ] ||
	fail "unexpected command count"
# Require every future command to ship with help and at least one example.
for command in $commands; do
	command_help=$($TMUX help "$command") ||
		fail "missing help for $command"
	printf '%s\n' "$command_help" | grep -q '^EXAMPLES$' ||
		fail "missing examples for $command"
done

# A pseudo-terminal selects readable tables. NO_COLOR only removes colour.
catalog=$(NO_COLOR=1 script -q /dev/null $TMUX list-commands) ||
	fail "interactive command catalog failed"
contains "$catalog" 'Sessions & Server'
contains "$catalog" 'COMMAND  *ALIAS  *DESCRIPTION'

sessions=$(NO_COLOR=1 script -q /dev/null $TMUX list-sessions) ||
	fail "interactive list-sessions failed"
contains "$sessions" 'Sessions'
contains "$sessions" 'NAME  *WINDOWS  *STATE'

windows=$(NO_COLOR=1 script -q /dev/null $TMUX list-windows) ||
	fail "interactive list-windows failed"
contains "$windows" 'Windows'
contains "$windows" 'INDEX  *NAME  *PANES'

panes=$(NO_COLOR=1 script -q /dev/null $TMUX list-panes) ||
	fail "interactive list-panes failed"
contains "$panes" 'Panes'
contains "$panes" 'TARGET  *SIZE  *HISTORY'

keys=$(NO_COLOR=1 script -q /dev/null $TMUX list-keys -T prefix) ||
	fail "interactive list-keys failed"
contains "$keys" 'Key bindings'
contains "$keys" 'KEY  *TABLE  *FLAGS  *ACTION'

buffers=$(NO_COLOR=1 script -q /dev/null $TMUX list-buffers) ||
	fail "interactive list-buffers failed"
contains "$buffers" 'Buffers'
contains "$buffers" 'NAME  *SIZE  *PREVIEW'

options=$(NO_COLOR=1 script -q /dev/null $TMUX show-options -g) ||
	fail "interactive show-options failed"
contains "$options" 'Options'
contains "$options" 'OPTION  *VALUE  *SOURCE'

escape=$(printf '\033')
case "$sessions" in
	*"$escape"*) fail "NO_COLOR output contains ANSI escapes" ;;
esac
coloured=$(env -u NO_COLOR script -q /dev/null $TMUX list-sessions) ||
	fail "coloured list-sessions failed"
case "$coloured" in
	*"$escape"*) ;;
	*) fail "terminal output has no ANSI styling" ;;
esac

$TMUX kill-server 2>/dev/null
exit 0
