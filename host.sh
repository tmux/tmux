#!/bin/bash

./tmux -S /tmp/tmux.socket new-session -d -s sess
chmod 777 /tmp/tmux.socket
./tmux -S /tmp/tmux.socket attach -t sess
