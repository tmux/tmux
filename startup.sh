#!/bin/bash

# starts new tmux session if not in tmux already
if [ -z "$TMUX" ]; then
	tmux new-session -d
	tmux send-keys "$0 inside" C-m
	tmux attach
	exit
fi

# all the configs
if [ "$1" = "inside" ]; then
	tmux rename-session lay # session name
	tmux set -g status-style bg=black,fg=white # term colors
	tmux set -g window-style bg=black # window colors
	tmux set -g window-active-style bg=black # active window colors
	tmux set -g pane-border-style fg=blue # pane colors
	tmux set -g pane-active-border-style fg=cyan # active pane colors
	tmux set -g status-bg blue # status bar color
	tmux set -g status-fg white # status bar text color
	# select panes / rc
	tmux split-window -h
	tmux send-keys "clear" C-m
	tmux send-keys "ls ~" C-m
	tmux select-pane -L
	tmux send-keys "clear" C-m
	tmux send-keys "fastfetch" C-m
	tmux split-window -v
	tmux send-keys "clear" C-m
	tmux send-keys "htop" C-m
	tmux select-pane -R
fi
