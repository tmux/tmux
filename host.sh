#!/bin/bash

rm -f $TMUX_SOCKET_FILE

#gdb --args ./tmux -S $TMUX_SOCKET_FILE new-session -s sess
./tmux -S $TMUX_SOCKET_FILE new-session -s sess

#./tmux -S /tmp/tmux.sock attach -t sess
