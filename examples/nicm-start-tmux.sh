#!/bin/sh -x

[ ! -z "$TMUX" ] && exit

SOCKET=/tmp/tmux-1000-main

TMUX="tmux -S $SOCKET"

if ! $TMUX -s $SESSION has 2>/dev/null; then
    # Session 0
    $TMUX new -d -s0 -nyelena 'exec ssh yelena'		# 0

    # This needs to be set before starting shells...
    $TMUX set default-command "exec $SHELL -l"

    $TMUX -s0 neww -d -ntodo 'exec emacs ~/TODO'	# 1
    $TMUX -s0 neww -d -nncmpc				# 2
    $TMUX -s0 neww -d					# 3
    $TMUX -s0 neww -d					# 4
    $TMUX -s0 neww -d					# 5
    $TMUX -s0 neww -d					# 6
    $TMUX -s0 neww -d					# 7
    $TMUX -s0 neww -d					# 8
    $TMUX -s0 neww -d					# 9

    # Other sessions
    for i in 1 2; do
	# Window 0 is linked from session 0
	$TMUX new -d -s$i
	$TMUX -s$i linkw -dki0 0 0

	$TMUX -s$i neww -d
	$TMUX -s$i neww -d
    done

    # Rebind prefix key
    $TMUX set prefix ^A
    $TMUX unbind ^B
    $TMUX bind ^A send-prefix

    # Bind q,w,e to session 0,1,2. We need per-session toolbar colours!
    $TMUX bind q switch 0
    $TMUX bind Q switch 0
    $TMUX bind w switch 1
    $TMUX bind W switch 1
    $TMUX bind e switch 2
    $TMUX bind E switch 2

    # No bells, thanks
    $TMUX set bell-action none
fi

$TMUX -s0 attach -d
