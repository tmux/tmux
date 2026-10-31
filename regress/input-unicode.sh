#!/bin/sh

. ./input-common.inc

start_pane wide 10 3 '\343\201\202B\rX\n'
check_capture wide 'X B'
check_flags wide 'X X B'

start_pane widepad 10 3 'A\343\201\202B\r\033[2CX\n'
check_capture widepad 'A XB'
check_flags widepad 'X A XB'

start_pane wideedge 5 3 'abc\343\201\202Z\n'
check_capture wideedge 'abcあ
Z'
check_cursor wideedge '0,2'
check_joined wideedge 'abcあZ'

start_pane wideeol 5 3 'abcd\343\201\202Z\n'
check_capture wideeol 'abcd
あZ'
check_cursor wideeol '0,2'

start_pane combine 10 3 'e\314\201\n'
check_capture combine 'é'
check_cursor combine '0,1'

start_pane combinewide 10 3 '\343\201\202\314\201X\n'
check_capture combinewide 'あ́X'
check_cursor combinewide '0,1'

start_pane variation 10 3 '\342\234\224\357\270\217X\n'
check_capture variation '✔️X'
check_cursor variation '0,1'

start_pane flag 10 3 '\360\237\207\254\360\237\207\247X\n'
check_capture flag '🇬🇧X'
check_cursor flag '0,1'

# Regional indicators still pair, but a third starts a new cell.
start_pane flagrun 10 3 '\360\237\207\254\360\237\207\247\360\237\207\270\360\237\207\252\360\237\207\246X'
check_capture flagrun '🇬🇧🇸🇪🇦X'
check_cursor flagrun '6,0'
check_raw_matches flagrun \
	'C 0,0 data=\(2,8,🇬🇧\) flags=NONE\[0\]' \
	'C 0,2 data=\(2,8,🇸🇪\) flags=NONE\[0\]' \
	'C 0,4 data=\(1,4,🇦\) flags=NONE\[0\]' \
	'C 0,5 data=\(1,1,X\) flags=NONE\[0\]'

# A skin tone combines only with the emoji before it.
start_pane tonebefore 10 3 '\360\237\217\275\360\237\221\215'
check_capture tonebefore '🏽👍'
check_cursor tonebefore '4,0'
check_raw_matches tonebefore \
	'C 0,0 data=\(2,4,🏽\) flags=NONE\[0\]' \
	'C 0,1 .*flags=PADDING\[[0-9a-f]+\]' \
	'C 0,2 data=\(2,4,👍\) flags=NONE\[0\]' \
	'C 0,3 .*flags=PADDING\[[0-9a-f]+\]'

start_pane toneafter 10 3 '\360\237\221\215\360\237\217\275'
check_capture toneafter '👍🏽'
check_cursor toneafter '2,0'
check_raw_matches toneafter \
	'C 0,0 data=\(2,8,👍🏽\) flags=NONE\[0\]' \
	'C 0,1 .*flags=PADDING\[[0-9a-f]+\]'

# Later emoji must not accumulate in a cell beginning with a skin tone.
start_pane tonebefore-run 20 3 '\360\237\217\275\360\237\221\215\360\237\221\215\360\237\221\215\360\237\221\215\360\237\221\215\360\237\221\215X'
check_capture tonebefore-run '🏽👍👍👍👍👍👍X'
check_cursor tonebefore-run '15,0'
check_raw_matches tonebefore-run \
	'C 0,0 data=\(2,4,🏽\) flags=NONE\[0\]' \
	'C 0,2 data=\(2,4,👍\) flags=NONE\[0\]' \
	'C 0,12 data=\(2,4,👍\) flags=NONE\[0\]' \
	'C 0,14 data=\(1,1,X\) flags=NONE\[0\]'

# Preserve the skin tone and ZWJ sequence from GitHub issue 4726.
start_pane astronaut 20 3 'abc\360\237\221\251\360\237\217\273\342\200\215\360\237\232\200123'
check_capture astronaut 'abc👩🏻‍🚀123'
check_cursor astronaut '8,0'
check_raw_matches astronaut \
	'C 0,3 data=\(2,15,👩🏻‍🚀\) flags=NONE\[0\]' \
	'C 0,4 .*flags=PADDING\[[0-9a-f]+\]' \
	'C 0,5 data=\(1,1,1\) flags=NONE\[0\]'

start_pane combining-left 10 3 '\314\201A\n'
check_capture combining-left 'A'
check_cursor combining-left '0,1'

start_pane combining-overflow 10 3 'u\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\314\245\n'
check_raw_matches combining-overflow '^		C 0,0 data=\(1,31,u'
check_raw_no_matches combining-overflow 'data=\(0,'

$TMUX kill-server 2>/dev/null
exit $exit_status
