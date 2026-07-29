#!/bin/sh

# Short command help from list-commands.

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

plain=$($TMUX list-commands split-window) ||
	fail "list-commands split-window failed"
contains "$plain" '^split-window (splitw) '

help=$($TMUX list-commands -h split-window) ||
	fail "list-commands -h split-window failed"
[ "$help" = "split-window Create a new pane by splitting a window." ] ||
	fail "unexpected short help: $help"

alias_help=$($TMUX list-commands -h splitw) ||
	fail "list-commands -h splitw failed"
[ "$alias_help" = "$help" ] || fail "alias help differs from command help"

all_help=$($TMUX list-commands -h) ||
	fail "list-commands -h failed"
contains "$all_help" '^attach-session[[:space:]][[:space:]]*Attach to an existing session\.$'
contains "$all_help" '^bind-key[[:space:]][[:space:]]*Bind a key to one or more commands\.$'

verbose=$($TMUX list-commands -hh split-window) ||
	fail "list-commands -hh split-window failed"
contains "$verbose" '^split-window (splitw) '
contains "$verbose" '^    Create a new pane by splitting a window\.$'

separate_verbose=$($TMUX list-commands -h -h split-window) ||
	fail "list-commands -h -h split-window failed"
[ "$separate_verbose" = "$verbose" ] ||
	fail "-hh and -h -h output differs"

description=$($TMUX list-commands -F '#{command_list_description}' \
    split-window) || fail "command description format failed"
[ "$description" = "Create a new pane by splitting a window." ] ||
	fail "unexpected command description: $description"

commands=$($TMUX list-commands -F '#{command_list_name}') ||
	fail "could not enumerate commands"
for command in $commands; do
	description=$($TMUX list-commands -F '#{command_list_description}' \
	    "$command") || fail "could not get description for $command"
	[ -n "$description" ] || fail "missing description for $command"
done

$TMUX kill-server 2>/dev/null
exit 0
