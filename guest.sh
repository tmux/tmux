#!/bin/bash
export TMUX_SOCKET_FILE=/tmp/tmux.socket

./tmux -S $TMUX_SOCKET_FILE attach
