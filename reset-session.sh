#!/bin/bash

rm -rf tmux*.log
killall -u $USER tmux
./recompile.sh
