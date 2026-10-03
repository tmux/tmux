#!/bin/sh

. ./input-common.inc

start_pane alternate 10 3 'MAIN\033[?1049hALT\033[?1049lZ\n'
check_capture alternate 'MAINZ'

start_pane osc133 20 12 'xx\033]133;A\007p>\033]133;B\007cmd\nxy\033]133;P;k=s\007more\nxz\033]133;A;k=s\007more\nxw\033]133;P;k=c\007more\nxv\033]133;A;k=c\007more\nzz\033]133;C\007out\033]133;D;7\007\nq\033]133;C\007bad\033]133;D;-1\007\nqq\033]133;C\007big\033]133;D;300\007\nzzz\033]133;C\007ok\033]133;D\007\n'
check_capture osc133 'xxp>cmd
xymore
xzmore
xwmore
xvmore
zzout
qbad
qqbig
zzzok'
check_raw_matches osc133 \
	'L 0 \(0\) flags=START_PROMPT,START_COMMAND\[[0-9a-f]+\].* osc133=2,4,0,0,0' \
	'L 1 \(1\) flags=SECOND_PROMPT\[[0-9a-f]+\].* osc133=2,0,0,0,0' \
	'L 2 \(2\) flags=SECOND_PROMPT\[[0-9a-f]+\].* osc133=2,0,0,0,0' \
	'L 3 \(3\) flags=SECOND_PROMPT\[[0-9a-f]+\].* osc133=2,0,0,0,0' \
	'L 4 \(4\) flags=SECOND_PROMPT\[[0-9a-f]+\].* osc133=2,0,0,0,0' \
	'L 5 \(5\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,2,5,7' \
	'L 6 \(6\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,1,4,255' \
	'L 7 \(7\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,2,5,255' \
	'L 8 \(8\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,3,5,0'

# Reject malformed statuses and distinguish zero from a missing status.
start_pane osc133-status 20 8 \
	'\033]133;C\007a\033]133;D;1junk\007\n\033]133;C\007b\033]133;D;4294967296\007\n\033]133;C\007c\033]133;D;255\007\n\033]133;C\007d\033]133;D;0;foo=bar\007\n\033]133;C\007e\033]133;D;foo=bar\007\n\033]133;C\007f\033]133;D;\007\n'
check_raw_matches osc133-status \
	'L 0 \(0\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,0,1,255' \
	'L 1 \(1\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,0,1,255' \
	'L 2 \(2\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,0,1,255' \
	'L 3 \(3\) flags=START_OUTPUT,END_OUTPUT,END_OUTPUT_STATUS\[[0-9a-f]+\].* osc133=0,0,0,1,0' \
	'L 4 \(4\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,0,1,0' \
	'L 5 \(5\) flags=START_OUTPUT,END_OUTPUT\[[0-9a-f]+\].* osc133=0,0,0,1,0'

$TMUX kill-server 2>/dev/null
exit $exit_status
