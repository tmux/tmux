#!/bin/sh

# Keys which arrive in the same batch of terminal input as the key which opens
# a prompt, a pane prompt or a menu must still reach it. A key which ends a
# numeric prompt is not part of it, so it must run after the command the prompt
# runs, in the order the binding asked for.
#
# The client is attached from an outer server so that one send-keys is one
# batch of input.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export PATH TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
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
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

window_name()
{
	$INNER display-message -p '#{window_name}'
}

expect_name()
{
	wait_until output_is "$1" window_name ||
		fail "window name is '$(window_name)', expected '$1'"
}

$INNER new-session -d -s inner -x80 -y24 'exec sleep 100' || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t inner" || exit 1
wait_for_clients 1 $INNER

$INNER set-option -g automatic-rename off || exit 1

# Keys typed into a command prompt opened by a key in the same batch.
$INNER rename-window -t inner original || exit 1
$OUTER send-keys C-b , n a m e Enter || exit 1
expect_name originalname

# The same for confirm-before.
$INNER bind-key -n C-y confirm-before 'rename-window confirmed' || exit 1
$INNER rename-window -t inner original || exit 1
$OUTER send-keys C-y y || exit 1
expect_name confirmed

# The same for a menu.
$INNER bind-key -n C-y display-menu -T menu A a 'rename-window picked' ||
	exit 1
$INNER rename-window -t inner original || exit 1
$OUTER send-keys C-y a || exit 1
expect_name picked

# The same for a pane prompt.
$INNER bind-key -n C-p command-prompt -P 'rename-window "p%%"' || exit 1
$INNER rename-window -t inner original || exit 1
$OUTER send-keys C-p a b c Enter || exit 1
expect_name pabc

# A key which ends a numeric prompt is not used by it and is not part of its
# input, so it runs after the command the prompt runs.
$INNER bind-key -n C-n command-prompt -N 'rename-window "n%%"' || exit 1
$INNER bind-key -n l 'rename-window L' || exit 1
$INNER rename-window -t inner original || exit 1
$OUTER send-keys C-n 3 l || exit 1
expect_name L

# The same for a numeric pane prompt.
$INNER bind-key -n C-g command-prompt -P -N 'rename-window "g%%"' || exit 1
$INNER rename-window -t inner original || exit 1
$OUTER send-keys C-g 3 l || exit 1
expect_name L

# A command after the prompt in the same binding must still run after it.
$INNER bind-key -n C-o "command-prompt 'rename-window A' ; rename-window B" ||
	exit 1
$INNER rename-window -t inner start || exit 1
$OUTER send-keys C-o x Enter || exit 1
expect_name B

# A key which arrives after the keys which answer a prompt must not be taken as
# part of the prompt's input.
$INNER bind-key -n C-a command-prompt 'rename-window A' || exit 1
$INNER bind-key -n z 'rename-window Y' || exit 1
$INNER rename-window -t inner start || exit 1
$OUTER send-keys C-a a b c Enter z || exit 1
expect_name Y

exit 0
