#!/bin/sh

. ./input-common.inc

start_pane alternate 10 3 'MAIN\033[?1049hALT\033[?1049lZ\n'
check_capture alternate 'MAINZ'

start_pane osc133 20 8 'xx\033]133;A\007p>\033]133;B\007cmd\nxy\033]133;P;k=s\007more\nzz\033]133;C\007out\033]133;D;7\007\nq\033]133;C\007bad\033]133;D;-1\007\nqq\033]133;C\007big\033]133;D;300\007\nzzz\033]133;C\007ok\033]133;D\007\n'
check_capture osc133 'xxp>cmd
xymore
zzout
qbad
qqbig
zzzok'
check_raw_matches osc133 \
	'L 0 \(0\) flags=START_PROMPT,START_COMMAND\[[0-9a-f]+\].* osc133=2,4,0,0,0' \
	'L 1 \(1\) flags=SECOND_PROMPT\[[0-9a-f]+\].* osc133=2,0,0,0,0' \
	'L 2 \(2\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,2,5,7' \
	'L 3 \(3\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,1,4,255' \
	'L 4 \(4\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,2,5,255' \
	'L 5 \(5\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,3,5,0'

$TMUX kill-server 2>/dev/null
exit $exit_status
