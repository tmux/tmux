#!/bin/sh

. ./input-common.inc

start_pane bs 8 3 'abc\bd'
check_capture bs 'abd'
check_raw_matches bs \
	'C 0,0 data=\(1,1,a\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1,b\) flags=NONE\[0\]' \
	'C 0,2 data=\(1,1,d\) flags=NONE\[0\]'

start_pane nel 8 3 'A\033EB'
check_capture nel 'A
B'
check_raw_matches nel \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 1,0 data=\(1,1,B\) flags=NONE\[0\]'

start_pane tabstops 16 3 '\033H1\t2\033[3g\r\t3'
check_raw_matches tabstops \
	'C 0,0 data=\(1,1,1\) flags=NONE\[0\]' \
	'C 0,8 data=\(1,1,2\) flags=NONE\[0\]' \
	'C 0,15 data=\(1,1,3\) flags=NONE\[0\]'

start_pane decaln 6 3 '\033#8'
check_raw_matches decaln \
	'C 0,0 data=\(1,1,E\) flags=NONE\[0\]' \
	'C 1,5 data=\(1,1,E\) flags=NONE\[0\]' \
	'C 2,5 data=\(1,1,E\) flags=NONE\[0\]'

start_pane charset 8 3 '\033(0qxl\033(BZ'
check_raw_matches charset \
	'C 0,0 data=\(1,1,q\) flags=NONE\[0\] attr=CHARSET\[[0-9a-f]+\]' \
	'C 0,1 data=\(1,1,x\) flags=NONE\[0\] attr=CHARSET\[[0-9a-f]+\]' \
	'C 0,2 data=\(1,1,l\) flags=NONE\[0\] attr=CHARSET\[[0-9a-f]+\]' \
	'C 0,3 data=\(1,1,Z\) flags=NONE\[0\] attr=NONE\[0\]'

start_pane g1charset 8 3 '\033)0\016q\017Z'
check_raw_matches g1charset \
	'C 0,0 data=\(1,1,q\) flags=NONE\[0\] attr=CHARSET\[[0-9a-f]+\]' \
	'C 0,1 data=\(1,1,Z\) flags=NONE\[0\] attr=NONE\[0\]'

start_pane csisave 8 3 '\033[3;3HS\033[s\033[1;1HA\033[uR'
check_raw_matches csisave \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 2,2 data=\(1,1,S\) flags=NONE\[0\]' \
	'C 2,3 data=\(1,1,R\) flags=NONE\[0\]'

start_pane alternate 8 3 'main\033[?1049halt\033[?1049lback'
check_capture alternate 'mainback'
check_raw_matches alternate \
	'C 0,0 data=\(1,1,m\) flags=NONE\[0\]' \
	'C 0,4 data=\(1,1,b\) flags=NONE\[0\]' \
	'C 0,7 data=\(1,1,k\) flags=NONE\[0\]'

start_pane sync 8 3 '\033P=1signored\033\\A\033P=2s\033\\B'
check_raw_matches sync \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1,B\) flags=NONE\[0\]'

start_pane private 8 3 '\033[?25lA\033[?25hB\033[?1000hC\033[?1000lD'
check_raw_matches private \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1,B\) flags=NONE\[0\]' \
	'C 0,2 data=\(1,1,C\) flags=NONE\[0\]' \
	'C 0,3 data=\(1,1,D\) flags=NONE\[0\]'

start_pane ris 8 3 'A\033cB'
check_capture ris 'B'
check_raw_matches ris \
	'C 0,0 data=\(1,1,B\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\]'

start_pane keypad 8 3 '\033=A\033>B'
check_raw_matches keypad \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1,B\) flags=NONE\[0\]'

start_pane cursorstyle 8 3 '\033[5 qA\033[0 qB'
check_raw_matches cursorstyle \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 0,1 data=\(1,1,B\) flags=NONE\[0\]'

exit $exit_status
