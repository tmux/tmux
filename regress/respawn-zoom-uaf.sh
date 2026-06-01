#!/bin/sh

# Regression test for respawning the active pane in a zoomed window.
#
# window_zoom() saves each pane's original unzoomed layout in saved_layout_cell
# and gives the active pane a temporary one-pane zoom layout. On the respawn
# path, spawn_pane() reuses the active pane; the buggy zoom handling is:
#
#     if (w->flags & WINDOW_ZOOMED)
#             new_wp->saved_layout_cell = new_wp->layout_cell;
#
# This clobbers the saved_layout_cell already there with the temporary cell.
# On the next unzoom, window_unzoom() frees the temporary layout, restores
# the now-dangling saved_layout_cell, and layout_fix_panes() reads freed
# memory. With ASan, this is reported as a heap use-after-free.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest"
$TMUX kill-server 2>/dev/null
sleep 1

# Two panes so the window can be zoomed; zoom the active pane.
$TMUX -f/dev/null new -d 'sleep 1000' || exit 1
$TMUX split-window -d 'sleep 1000' || exit 1
$TMUX resize-pane -Z || exit 1

# Respawn the active (zoomed) pane in place, then unzoom.
$TMUX respawn-pane -k 'sleep 1000' || exit 1
$TMUX resize-pane -Z 2>/dev/null
sleep 1

# With the bug, unzooming crashes the server; with the fix, it survives and
# has-session succeeds.
$TMUX has-session 2>/dev/null || exit 1

$TMUX kill-server 2>/dev/null

exit 0
