#!/bin/sh

. ./input-common.inc

start_pane alternate 10 3 'MAIN\033[?1049hALT\033[?1049lZ\n'
check_capture alternate 'MAINZ'

start_pane osc133 10 4 '\033]133;A\007prompt\n\033]133;C\007output\n'
check_capture osc133 'prompt
output'
check_flags osc133 'P prompt
O output'

$TMUX kill-server 2>/dev/null
exit $exit_status
