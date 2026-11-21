#!/bin/sh

. ./input-common.inc

# The shortest collected prefix must leave the final cell at the right edge.
start_pane narrow 2 3 '\033[?7lABC'
check_capture narrow 'AC'
check_cursor narrow '1,0'
check_flags narrow '- AC'

# REP may fill the final cell, but subsequent text still overwrites it.
start_pane repeat 5 3 '\033[?7lA\033[9bYZ'
check_capture repeat 'AAAAZ'
check_cursor repeat '4,0'
check_flags repeat '- AAAAZ'

# Style changes must flush collected cells without moving the final cursor.
start_pane styled 5 3 '\033[?7l\033[31mAB\033[32mCDE\033[34mZ'
check_capture styled 'ABCDZ'
check_cursor styled '4,0'
check_raw_matches styled \
	'C 0,0 data=\(1,1,A\).*fg=red\[1\] ' \
	'C 0,2 data=\(1,1,C\).*fg=green\[2\] ' \
	'C 0,4 data=\(1,1,Z\).*fg=blue\[4\] '

# Overwriting a wide character's padding must clear the old leading cell.
start_pane padding 8 3 '\033[?7lA\343\201\202B\r\033[2CX'
check_capture padding 'A XB'
check_cursor padding '3,0'
check_raw_matches padding \
	'C 0,1 data=\(1,1, \) flags=NONE\[0\]' \
	'C 0,2 data=\(1,1,X\) flags=NONE\[0\]'

# Insert mode must shift existing cells even when autowrap is disabled.
start_pane insert 5 3 '\033[?7lABCD\r\033[C\033[4hXY\033[4l'
check_capture insert 'AXYBC'
check_cursor insert '3,0'

# Re-enabling wrap after a collected prefix must retain the wrap boundary.
start_pane toggle 5 3 '\033[?7lABCD\033[?7hEF'
check_capture toggle 'ABCDE
F'
check_cursor toggle '1,1'
check_flags toggle 'W ABCDE
- F'

# A collected run must leave REP using its last character.
start_pane bulk-repeat 12 3 'ABCDE\033[3b'
check_capture bulk-repeat 'ABCDEEEE'
check_cursor bulk-repeat '8,0'

# A run spanning rows must preserve both pending wraps.
start_pane bulk-wrap 5 3 'ABCDEFGHIJK'
check_capture bulk-wrap 'ABCDE
FGHIJ
K'
check_cursor bulk-wrap '1,2'
check_flags bulk-wrap 'W ABCDE
W FGHIJ
- K'

# A following combining character must attach to the end of the run.
start_pane bulk-combine 12 3 'abcde\314\201F'
check_capture bulk-combine 'abcdéF'
check_cursor bulk-combine '6,0'

# ASCII after an incomplete UTF-8 character must first emit a replacement.
start_pane bulk-incomplete 16 3 '\342ABCDEFG'
check_capture bulk-incomplete '�ABCDEFG'
check_cursor bulk-incomplete '8,0'

# A run starting in wide-character padding must clear the leading cell.
start_pane bulk-padding 10 3 'A\343\201\202B\r\033[2CXYZ'
check_capture bulk-padding 'A XYZ'
check_cursor bulk-padding '5,0'

exit $exit_status
