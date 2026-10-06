#!/bin/sh

# Exercise list-commands for the full table, a single command, aliases, custom
# and empty formats, and lookup errors.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

# Keep a server alive across commands. Otherwise a STARTSERVER-only command
# may exit between invocations and leave a short socket-cleanup race.
$TMUX new-session -d -s list-commands 'exec sleep 100' || exit 1

all=$($TMUX list-commands) || exit 1
printf '%s\n' "$all" | grep -q '^attach-session (attach)' || exit 1
printf '%s\n' "$all" | grep -q '^list-commands (lscm)' || exit 1

one=$($TMUX list-commands -F \
    '#{command_list_name}|#{command_list_alias}|#{command_list_usage}' \
    list-commands) || exit 1
case "$one" in
'list-commands|lscm|'*) ;;
*) exit 1 ;;
esac

# Aliases resolve to their command, while an empty expansion prints no line.
[ "$($TMUX list-commands -F '#{command_list_name}' lscm)" = list-commands ] ||
	exit 1
[ -z "$($TMUX list-commands -F '' list-commands)" ] || exit 1

$TMUX list-commands tmux-no-command-$$ >/dev/null 2>&1 && exit 1
$TMUX list-commands list >/dev/null 2>&1 && exit 1
exit 0
