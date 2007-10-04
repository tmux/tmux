#!/bin/sh

tmux attach
if [[ $? = 1 ]]; then
    tmux new-session -d -nmain 'exec irssi'
    tmux -smain set prefix '^H'
    tmux -smain new-window -d -nherrie 'exec sudo herrie -c /home/mxey/etc/herrie/config'
    tmux -smain new-window -d 'exec lynx'
    exec tmux -smain attach
fi
