#!/bin/sh

. ./input-common.inc

start_pane erasechars 8 3 'ABCDEFGH\r\033[3C\033[2X'
check_capture erasechars 'ABC  FGH'
check_raw_matches erasechars \
	'C 0,3 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,4 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,5 data=\(1,1,F\) flags=NONE\[0\]'

start_pane deletechars 8 3 'ABCDEFGH\r\033[3C\033[3P'
check_capture deletechars 'ABCGH'
check_raw_matches deletechars \
	'C 0,3 data=\(1,1,G\) flags=NONE\[0\]' \
	'C 0,4 data=\(1,1,H\) flags=NONE\[0\]' \
	'C 0,5 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\]'

start_pane insertchars 8 3 'ABCDEF\r\033[3C\033[2@xy'
check_capture insertchars 'ABCxyDEF'
check_raw_matches insertchars \
	'C 0,3 data=\(1,1,x\) flags=NONE\[0\]' \
	'C 0,4 data=\(1,1,y\) flags=NONE\[0\]' \
	'C 0,5 data=\(1,1,D\) flags=NONE\[0\]'

start_pane eraseline 8 3 'ABCDEFGH\r\033[4C\033[K'
check_capture eraseline 'ABCD'
check_raw_matches eraseline \
	'C 0,4 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\]' \
	'C 0,7 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\]'

start_pane erasescreen 8 3 '1111111\033[2;1H2222222\033[H\033[JZ'
check_capture erasescreen 'Z'
check_raw_matches erasescreen \
	'^G 8x3 \(0/0\)$' \
	'C [0-9]+,0 data=\(1,1,Z\) flags=NONE\[0\]' \
	'C [0-9]+,1 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\]'

start_pane tabs 12 3 'A\tB\033[2g\r\033[IC'
check_raw_matches tabs \
	'L 0 \(0\) flags=EXTENDED\[[0-9a-f]+\]' \
	'C 0,1 data=\(7,7,       \) flags=TAB\[[0-9a-f]+\]' \
	'C 0,2 data=\(1,1,!\) flags=PADDING\[[0-9a-f]+\]' \
	'C 0,8 data=\(1,1,B\) flags=NONE\[0\]' \
	'C 0,0 data=\(1,1,C\) flags=NONE\[0\]'

exit $exit_status
