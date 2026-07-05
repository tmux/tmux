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

start_pane combining-left 10 3 '\314\201A\n'
check_capture combining-left 'A'
check_cursor combining-left '0,1'

$TMUX kill-server 2>/dev/null
exit $exit_status
