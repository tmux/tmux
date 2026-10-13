#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 15

check()
{
	format=$1
	expected=$2
	actual=$($TMUX -u display-message -p -t "$pane" "$format") || exit 1
	if [ "$actual" != "$expected" ]; then
		printf 'format: %s\nexpected: %s\nactual: %s\n' \
		    "$format" "$expected" "$actual"
		exit 1
	fi
}

emit()
{
	printf '\033]133;%s\033\\' "$1" |
	    $TMUX display-message -I -t "$pane" || exit 1
}

check_command()
{
	emit "C;$1"
	check '#{pane_command_line}' "$2"
	check '#{@started}' "$2"
	check '#{pane_command_running}:#{pane_command_status}' '1:'
	emit 'D;7'
	check '#{pane_command_line}' "$2"
	check '#{@finished}' "$2"
	check '#{pane_command_running}:#{pane_command_status}' '0:7'
}

$TMUX new-session -d -s test 'sleep 1000' || exit 1
pane=$($TMUX new-window -d -P -F '#{pane_id}' '') || exit 1
other=$($TMUX new-window -d -P -F '#{pane_id}' '') || exit 1
$TMUX set-hook -g pane-command-started \
    'set -gF @started "#{hook_command_line}"' || exit 1
$TMUX set-hook -g pane-command-finished \
    'set -gF @finished "#{hook_command_line}"' || exit 1

check '#{pane_command_line}:#{pane_command_running}' ':0'
check_command 'cmdline_url=sleep%201' 'sleep 1'

# Prompt markers leave the most recent command available.
emit A
emit B
check '#{pane_command_line}' 'sleep 1'
$TMUX display-message -p -t "$other" '#{pane_command_line}' |
    grep -qx '' || exit 1

# Unknown fields are ignored and decoded semicolons belong to the value.
check_command 'other=1;cmdline_url=echo%20one%3B%20echo%20two;other=2' \
    'echo one; echo two'
check_command 'cmdline_url=echo%20a+b%2Bc%25%3d%3D' 'echo a+b+c%=='
check_command 'cmdline_url=echo%20%C3%A9%E4%B8%AD' 'echo é中'
check_command 'cmdline_url=echo%20%2520' 'echo %20'

# Use the same cleanup as titles for control characters and format jobs.
check_command 'cmdline_url=echo%20%23%28date%29' 'echo _(date)'
check_command 'cmdline_url=echo%20%5C' 'echo \\'

# Multiline commands retain tabs and newlines as visible escapes, including
# in the event payloads used by hooks.
check_command 'cmdline_url=sleep%201%0Aecho%20done' 'sleep 1\necho done'
check_command 'cmdline_url=echo%09hello%0Aworld' 'echo\thello\nworld'

# The name validator also permits these bytes before clean_name escapes them.
name=$(printf 'tab\tnewline\nname')
$TMUX rename-window -t "$other" "$name" || exit 1
actual=$($TMUX -u display-message -p -t "$other" '#{window_name}') || exit 1
[ "$actual" = 'tab\tnewline\nname' ] || exit 1
$TMUX select-pane -t "$pane" -T "$name" || exit 1
check '#{pane_title}' 'tab\tnewline\nname'

# Every new start clears old metadata if it is missing or invalid.
emit C
check '#{pane_command_line}:#{@started}' ':'
for metadata in \
    'cmdline_url=' \
    'other=cmdline_url=stale' \
    'xcmdline_url=stale' \
    'cmdline_url=bad%' \
    'cmdline_url=bad%2' \
    'cmdline_url=bad%GG' \
    'cmdline_url=bad%00tail' \
    'cmdline_url=bad%0Dtail' \
    'cmdline_url=bad%1Btail' \
    'cmdline_url=bad%07tail' \
    'cmdline_url=bad%08tail' \
    'cmdline_url=bad%0Btail' \
    'cmdline_url=bad%0Ctail' \
    'cmdline_url=bad%7Ftail' \
    'cmdline_url=bad%FF' \
    'cmdline_url=bad%C3' \
    'cmdline_url=bad%C0%AF'
do
	check_command 'cmdline_url=previous' previous
	check_command "$metadata" ''
done

# The limit applies to decoded bytes, not the size of the OSC string.
decoded=$(printf '%5000s' '' | tr ' ' x)
check_command "cmdline_url=$decoded" "$decoded"
encoded=$(awk 'BEGIN { for (i = 0; i < 16384; i++) printf "%%78" }')
decoded=$(printf '%16384s' '' | tr ' ' x)
check_command "cmdline_url=$encoded" "$decoded"
check_command "cmdline_url=${encoded}%78" ''

# Complete UTF-8 characters may reach the limit, but overlong lines are
# rejected in full rather than truncated, even through a character.
check_boundary()
{
	prefix=$(printf "%${1}s" '' | tr ' ' x)
	check_command "cmdline_url=$prefix$2" "$prefix$3"
	check_command "cmdline_url=${prefix}x$2" ''
}
check_boundary 16382 '%C3%A9' 'é'
check_boundary 16381 '%E4%B8%AD' '中'
check_boundary 16380 '%F0%9F%98%80' '😀'

# Each hook gets a snapshot, even if multiple commands arrive in one read.
$TMUX set -g @log '' || exit 1
$TMUX set-hook -g pane-command-started \
    'set -gF @log "#{@log}|start:#{hook_command_line}"' || exit 1
$TMUX set-hook -g pane-command-finished \
    'set -gF @log "#{@log}|end:#{hook_command_line}:#{hook_command_status}"' || exit 1
printf '\033]133;C;cmdline_url=first\007\033]133;D;0\007\033]133;C;cmdline_url=second\007\033]133;D;1\007\033]133;C\007\033]133;D;2\007' |
    $TMUX display-message -I -t "$pane" || exit 1
check '#{@log}' '|start:first|end:first:0|start:second|end:second:1|start:|end::2'
check '#{pane_command_line}' ''

exit 0
