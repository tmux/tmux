#!/bin/sh

# Keys which arrive in the same batch of terminal input as the key which opens
# a prompt, a pane prompt or a menu must still reach it. A key which ends a
# numeric prompt is not part of it: the command the prompt queues for it runs
# before it, and the keys keep the order they arrived in. A key which the
# prompt does not use at all waits for the prompt to close rather than running
# before the commands which are already queued.
#
# The client is attached from an outer server so that one send-keys is one
# batch of input. Keys sent with send-keys -K are injected into the client
# rather than typed at it, and must reach a prompt in the same way.

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

pane_line()
{
	$INNER capture-pane -p -S 0 -E 0 -t inner
}

expect_name()
{
	wait_until output_is "$1" window_name ||
		fail "window name is '$(window_name)', expected '$1'"
}

$INNER new-session -d -s inner -x80 -y24 'exec sleep 100' || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t inner" || exit 1
wait_for_clients 1 $INNER || fail "the client did not attach"
client=$($INNER list-clients -F '#{client_name}') || exit 1
[ -n "$client" ] || fail "the client has no name"

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

# The key which ends a numeric prompt is not used by it, so the command the
# prompt queues for it has to run before that key and the keys have to keep
# their order. Record the count when a key runs: it is the prompt's count only
# if its command ran first, so start it at a value the prompt cannot produce.
# set-option expands a format only with -F.
$INNER set-option -g @order '' || exit 1
$INNER set-option -g @count none || exit 1
$INNER bind-key -n C-k command-prompt -N 'set-option -g @count "%%"' ||
	exit 1
$INNER bind-key -n l 'set-option -gF @order "#{@order}[#{@count}]l"' ||
	exit 1
$INNER bind-key -n h 'set-option -gF @order "#{@order}h"' || exit 1
$OUTER send-keys C-k 3 l h || exit 1
wait_until output_is '[3]lh' $INNER display-message -p '#{@order}' ||
	fail "keys ran as '$($INNER display-message -p '#{@order}')'"

# A command the binding queues behind the prompt runs before the binding of a
# key which arrives after it, so a key which has to run while the prompt is
# waiting does not overtake the commands already queued: it runs where it is.
# The command records the count, so it reads it only if the command the prompt
# queues for the key ran first.
$INNER set-option -g @order '' || exit 1
$INNER set-option -g @count none || exit 1
b='set-option -gF @order "#{@order}b#{@count}"'
$INNER bind-key -n C-w "command-prompt -N 'set-option -g @count %%' ; $b" ||
	exit 1
$OUTER send-keys C-w 3 l h || exit 1
wait_until output_is 'b3[3]lh' $INNER display-message -p '#{@order}' ||
	fail "keys ran as '$($INNER display-message -p '#{@order}')'"

# A key which ends a numeric prompt is not used by it, so it keeps the place
# where it was queued: the command the prompt queues for it runs first, then
# the command the binding queued behind the prompt, and only then the key,
# which goes to the pane in the order it arrived with the keys after it.
$INNER set-option -g @count none || exit 1
mark='send-keys -l B'
$INNER bind-key -n C-e "command-prompt -N 'set-option -g @count %%' ; $mark" ||
	exit 1
$OUTER send-keys C-e 3 x y || exit 1
wait_until output_is 'Bxy' pane_line ||
	fail "the pane shows '$(pane_line)', expected 'Bxy'"

# A key sent with send-keys -K is a key like any other: a prompt which is open
# takes it instead of letting it go to the pane. Open the prompt with -b,
# because an injection which opens a prompt waits for the answer and only the
# attached client can answer it.
$INNER set-option -g @count none || exit 1
$INNER bind-key -n C-t command-prompt -b -N 'set-option -g @count "%%"' ||
	exit 1
$INNER send-keys -K -c "$client" C-t || exit 1
$INNER send-keys -K -c "$client" 3 || exit 1
$INNER send-keys -K -c "$client" Enter || exit 1
wait_until output_is 3 $INNER display-message -p '#{@count}' ||
	fail "count is '$($INNER display-message -p '#{@count}')', expected 3"

# A background prompt does not wait for the queue, so a mouse key which it does
# not use must not be put back in the queue by it: that loops forever and the
# server stops answering.
$INNER set-option -g mouse on || exit 1
$INNER bind-key -n C-y command-prompt -b 'set-option -g @bg 1' || exit 1
$OUTER send-keys C-y || exit 1
$OUTER send-keys -l "$(printf '\033[<0;10;5M')" || exit 1

# A server which is stuck does not answer and the client then waits forever, so
# bound each check with timeout(1) where it exists: wait_until cannot do this,
# because the command it runs is the one which blocks.
alive()
(
	if command -v timeout >/dev/null 2>&1; then
		[ "$(timeout 1 $INNER display-message -p ok 2>/dev/null)" = ok ]
	else
		[ "$($INNER display-message -p ok 2>/dev/null)" = ok ]
	fi
)
i=0
while ! alive; do
	i=$((i + 1))
	[ "$i" -lt 10 ] || fail "server stopped answering after a mouse key"
done

# Close the background prompt, so that a prompt can be opened below.
$OUTER send-keys Enter || exit 1

# A key which the prompt does not use, while the prompt is still waiting, must
# not run before the commands which are already queued: it waits for the
# prompt to close and then runs where it was queued. The prompt's own command
# records itself first, then the command the binding queued behind the prompt,
# then the key, so the order shows when the key ran.
# The key is a mouse release on the pane: the prompt is on the status line and
# does not use it, and a release is not part of a click sequence, so it does
# not turn into a double click.
$INNER set-option -g @order '' || exit 1
$INNER bind-key -n MouseUp1Pane 'set-option -gF @order "#{@order}r"' || exit 1
$INNER bind-key -n C-r "command-prompt 'set-option -gF @order \"#{@order}a\"' ; set-option -gF @order '#{@order}b'" ||
	exit 1
$OUTER send-keys C-r || exit 1
$OUTER send-keys -l "$(printf '\033[<0;10;5m')" || exit 1
$OUTER send-keys Enter || exit 1
wait_until output_is 'abr' $INNER display-message -p '#{@order}' ||
	fail "keys ran as '$($INNER display-message -p '#{@order}')'"

# The key must not run while the prompt is waiting either: it runs after the
# prompt has closed. The prompt changes the binding of the key itself, so the
# order shows when the key ran: a key run while the prompt is waiting uses the
# old binding and records 'r', a key run after the prompt has closed uses the
# new binding and records 'q'.
$INNER set-option -g @order '' || exit 1
$INNER bind-key -n MouseUp1Pane 'set-option -gF @order "#{@order}r"' || exit 1
rebind='bind-key -n MouseUp1Pane "set-option -gF @order \"#{@order}q\""'
$INNER bind-key -n C-s "command-prompt '$rebind'" || exit 1
$OUTER send-keys C-s || exit 1
$OUTER send-keys -l "$(printf '\033[<0;10;5m')" || exit 1
$OUTER send-keys Enter || exit 1
wait_until output_is 'q' $INNER display-message -p '#{@order}' ||
	fail "keys ran as '$($INNER display-message -p '#{@order}')'"

exit 0
