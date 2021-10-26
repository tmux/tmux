#!/bin/bash
export TMUX_SOCKET_FILE=/tmp/tmux.socket

rm -f $TMUX_SOCKET_FILE
rm -f tmux-*.log

#gdb --args ./tmux -S $TMUX_SOCKET_FILE new-session -s sess
./tmux -S $TMUX_SOCKET_FILE new-session -s sess

#./tmux -S /tmp/tmux.sock attach -t sess
