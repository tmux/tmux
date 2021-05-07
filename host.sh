#!/bin/bash

./tmux -S /tmp/tmux.socket new-session -s sess
./tmux -S /tmp/tmux.socket attach -t sess
