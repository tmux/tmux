#!/bin/bash

# Copyright (c) 2012 Juan Ignacio Pumarino, jipumarino@gmail.com
# 
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

# Instructions
# ------------
#
# 1. Install this script and give it execute permission somewhere in your PATH.
#    For example:
#
#    $ mkdir -p ~/bin
#    $ wget https://raw.github.com/jipumarino/tmux-zoom/master/tmux-zoom.sh -O ~/bin/tmux-zoom.sh
#    $ chmod +x ~/bin/tmux-zoom.sh
# 
# 2. Add a shortcut in your ~/.tmux.conf file:
#
#    bind C-k run "tmux-zoom.sh"
#
# 3. When using this shortcut, the current tmux pane will open in a new window by itself.
#    Running it again in the zoomed window will return it to its original pane. You can have
#    as many zoomed windows as you want.

current=$(tmux display-message -p '#W-#I-#P')
list=$(tmux list-window)

[[ "$current" =~ ^(.*)-([0-9]+)-([0-9]+) ]]
current_window=${BASH_REMATCH[1]}
current_pane=${BASH_REMATCH[2]}-${BASH_REMATCH[3]}
new_zoom_window=ZOOM-$current_pane

if [[ $current_window =~ ZOOM-([0-9]+)-([0-9+]) ]]; then
  old_zoom_window=ZOOM-${BASH_REMATCH[1]}-${BASH_REMATCH[2]}
  tmux select-window -t ${BASH_REMATCH[1]} \; select-pane -t ${BASH_REMATCH[2]} \; swap-pane -s $old_zoom_window.1 \; kill-window -t $old_zoom_window
elif [[ $list =~ $new_zoom_window ]]; then
  tmux select-window -t $new_zoom_window
else
  tmux new-window -d -n $new_zoom_window \; swap-pane -s $new_zoom_window.1 \; select-window -t $new_zoom_window
fi
