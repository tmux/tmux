#!/bin/sh

. ./input-common.inc

start_pane_hlimit trim 6 3 'one\ntwo\nthree\nfour\nfive\nsix' 2
check_raw_matches trim \
	'^G 6x3 \(2/2\)$' \
	'L 0 \(-\) flags=NONE\[0\]' \
	'L 1 \(-\) flags=NONE\[0\]'

$TMUX clear-history -t trim:
check_raw_matches trim \
	'^G 6x3 \(0/2\)$' \
	'C 0,0 data=\(1,1,f\) flags=NONE\[0\]' \
	'C 2,0 data=\(1,1,s\) flags=NONE\[0\]'

start_pane_hlimit edhistory 6 3 'one\ntwo\nthree\033[H\033[JZ' 5
check_raw_matches edhistory \
	'^G 6x3 \([1-9][0-9]*/5\)$' \
	'L [0-9]+ \(-\) flags=NONE\[0\]' \
	'C [0-9]+,0 data=\(1,1,Z\) flags=NONE\[0\]'

exit $exit_status
