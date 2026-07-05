#!/bin/sh

. ./input-common.inc

start_pane_history reflow 8 4 'abcdefgh\nijklmnop\nqrstuvwx\nyz'
$TMUX resize-window -t reflow: -x 4 -y 4
sleep 0.2
check_raw_matches reflow \
	'^G 4x4 \([0-9]+/2000\)$' \
	'L [0-9]+ \([0-9-]+\) flags=WRAPPED\[[0-9a-f]+\]' \
	'C [0-9]+,0 data=\(1,1,a\) flags=NONE\[0\]' \
	'C [0-9]+,3 data=\(1,1,d\) flags=NONE\[0\]'

$TMUX resize-window -t reflow: -x 12 -y 4
sleep 0.2
check_raw_matches reflow \
	'^G 12x4 \([0-9]+/2000\)$' \
	'C [0-9]+,0 data=\(1,1,a\) flags=NONE\[0\]' \
	'C [0-9]+,7 data=\(1,1,h\) flags=NONE\[0\]'

exit $exit_status
