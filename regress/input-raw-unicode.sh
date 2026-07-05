#!/bin/sh

. ./input-common.inc

start_pane wide 8 3 'A\343\201\202B'
check_capture wide 'AあB'
check_raw_matches wide \
	'L 0 \(0\) flags=EXTENDED\[[0-9a-f]+\]' \
	'C 0,1 data=\(2,3,あ\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,2 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,3 data=\(1,1,B\) flags=NONE\[0\]'

start_pane combining 8 3 'e\314\201x'
check_raw_matches combining \
	'L 0 \(0\) flags=EXTENDED\[[0-9a-f]+\]' \
	'C 0,0 data=\(1,[0-9]+,.*\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,x\) flags=NONE\[0\]'

start_pane emoji 10 3 '\360\237\230\200Z'
check_raw_matches emoji \
	'L 0 \(0\) flags=EXTENDED\[[0-9a-f]+\]' \
	'C 0,0 data=\(2,4,😀\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,2 data=\(1,1,Z\) flags=NONE\[0\]'

start_pane flag 10 3 '\360\237\207\254\360\237\207\247!'
check_raw_matches flag \
	'L 0 \(0\) flags=EXTENDED\[[0-9a-f]+\]' \
	'C 0,0 data=\(2,8,🇬🇧\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,2 data=\(1,1,!\) flags=NONE\[0\]'

start_pane variation 10 3 '*\357\270\217!'
check_raw_matches variation \
	'L 0 \(0\) flags=EXTENDED\[[0-9a-f]+\]' \
	'C 0,0 data=\(2,[0-9]+,.*\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,2 data=\(1,1,!\) flags=NONE\[0\]'

start_pane invalid 10 3 '\377A'
check_raw_matches invalid \
	'C 0,0 data=\(1,3,.*\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,A\) flags=NONE\[0\]'

start_pane trunc2 10 3 '\303A'
check_raw_matches trunc2 \
	'C 0,0 data=\(1,3,.*\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,A\) flags=NONE\[0\]'

start_pane trunc3 10 3 '\342\202A'
check_raw_matches trunc3 \
	'C 0,0 data=\(1,3,.*\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,1 data=\(1,1,A\) flags=NONE\[0\]'

start_pane overwrite-wide-left 10 3 'A\343\201\202B\r\033[1CX'
check_raw_matches overwrite-wide-left \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1,X\) flags=NONE\[0\]' \
	'C 0,2 data=\(1,1, \) flags=NONE\[0\]' \
	'C 0,3 data=\(1,1,B\) flags=NONE\[0\]'

start_pane overwrite-wide-pad 10 3 'A\343\201\202B\r\033[2CX'
check_raw_matches overwrite-wide-pad \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1, \) flags=NONE\[0\]' \
	'C 0,2 data=\(1,1,X\) flags=NONE\[0\]' \
	'C 0,3 data=\(1,1,B\) flags=NONE\[0\]'

start_pane overwrite-wide-with-wide 10 3 'A\343\201\202B\r\033[1C\347\225\214'
check_raw_matches overwrite-wide-with-wide \
	'C 0,1 data=\(2,3,界\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 0,2 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,3 data=\(1,1,B\) flags=NONE\[0\]'

start_pane wide-right-edge 4 3 'ABC\343\201\202Z'
check_raw_matches wide-right-edge \
	'L 0 \(0\) flags=WRAPPED\[[0-9a-f]+\]' \
	'C 1,0 data=\(2,3,あ\) flags=NONE\[0\] attr=NONE\[0\]' \
	'C 1,1 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 1,2 data=\(1,1,Z\) flags=NONE\[0\]'

exit $exit_status
