#!/bin/sh

. ./input-common.inc

start_pane wrap 5 3 'ABCDEZ'
check_capture wrap 'ABCDE
Z'
check_raw_matches wrap \
	'^G 5x3 \(0/0\)$' \
	'L 0 \(0\) flags=WRAPPED\[[0-9a-f]+\]' \
	'C 0,4 data=\(1,1,E\) flags=NONE\[0\]' \
	'C 1,0 data=\(1,1,Z\) flags=NONE\[0\]'

start_pane nowrap 5 3 '\033[?7lABCDEZ'
check_capture nowrap 'ABCDZ'
check_raw_matches nowrap \
	'^G 5x3 \(0/0\)$' \
	'L 0 \(0\) flags=NONE\[0\]' \
	'C 0,4 data=\(1,1,Z\) flags=NONE\[0\]'

start_pane pending 5 3 'ABCD\r\033[4CZ'
check_capture pending 'ABCDZ'
check_cursor pending '5,0'
check_raw_matches pending \
	'L 0 \(0\) flags=NONE\[0\]' \
	'C 0,4 data=\(1,1,Z\) flags=NONE\[0\]'

exit $exit_status
