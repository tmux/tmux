#!/bin/sh

. ./input-common.inc

start_pane absolute 8 4 'A\033[3;5HB\033[2GC\033[2D!'
check_capture absolute 'A

!C  B'
check_cursor absolute '1,2'
check_raw_matches absolute \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 2,4 data=\(1,1,B\) flags=NONE\[0\]' \
	'C 2,0 data=\(1,1,!\) flags=NONE\[0\]' \
	'C 2,1 data=\(1,1,C\) flags=NONE\[0\]'

start_pane savecursor 8 4 '\033[4;4HS\0337\033[1;1HA\0338R'
check_capture savecursor 'A


   SR'
check_cursor savecursor '5,3'
check_raw_matches savecursor \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C 3,3 data=\(1,1,S\) flags=NONE\[0\]' \
	'C 3,4 data=\(1,1,R\) flags=NONE\[0\]'

start_pane origin 8 5 '\033[2;4r\033[?6h\033[1;1HO\033[3;1HP\033[?6lQ'
check_raw_matches origin \
	'C 1,0 data=\(1,1,O\) flags=NONE\[0\]' \
	'C 3,0 data=\(1,1,P\) flags=NONE\[0\]' \
	'C 0,0 data=\(1,1,Q\) flags=NONE\[0\]'

exit $exit_status
