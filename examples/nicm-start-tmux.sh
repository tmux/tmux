#!/bin/sh -x

[ ! -z "$TMUX" ] && exit

SOCKET=/tmp/tmux-1000-main
SESSION=natasha-main

TMUX="tmux -S $SOCKET"

if ! $TMUX -s $SESSION has 2>/dev/null; then
    $TMUX new -d -s $SESSION -nyelena 'exec ssh yelena'		# 0

    $TMUX set default-command "$SHELL -l"

    $TMUX -s $SESSION neww -d -ntodo 'exec emacs ~/TODO'	# 1
    $TMUX -s $SESSION neww -d -nncmpc				# 2
    $TMUX -s $SESSION neww -d					# 3
    $TMUX -s $SESSION neww -d					# 4
    $TMUX -s $SESSION neww -d					# 5
    $TMUX -s $SESSION neww -d					# 6
    $TMUX -s $SESSION neww -d					# 7
    $TMUX -s $SESSION neww -d					# 8
    $TMUX -s $SESSION neww -d					# 9

    $TMUX set prefix ^A
    $TMUX unbind ^B
    $TMUX bind ^A send-prefix

    $TMUX set bell-action none
fi

$TMUX -s $SESSION attach
