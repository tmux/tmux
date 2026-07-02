#!/bin/sh

. ./input-common.inc

start_pane_history history 6 3 'one\ntwo\nthree\nfour\nfive'
check_raw_matches history \
	'^G 6x3 \([1-9][0-9]*/2000\)$' \
	'L [0-9]+ \(-\) flags=NONE\[0\]' \
	'C [0-9]+,0 data=\(1,1,o\) flags=NONE\[0\]'

start_pane_history index 6 4 'A\nB\nC\033[2;3r\033[2;1HX\033D\033DY'
check_raw_matches index \
	'C [0-9]+,0 data=\(1,1,X\) flags=NONE\[0\]' \
	'C [0-9]+,0 data=\(1,1,A\) flags=NONE\[0\]' \
	'C [0-9]+,1 data=\(1,1,Y\) flags=NONE\[0\]'

start_pane reverse 6 4 'A\nB\nC\033[2;3r\033[2;1H\033MY'
check_raw_matches reverse \
	'C 1,0 data=\(1,1,Y\) flags=NONE\[0\]' \
	'C 2,0 data=\(1,1,B\) flags=NONE\[0\]'

start_pane insertline 6 4 'A\nB\nC\033[2;3r\033[2;1H\033[LY'
check_raw_matches insertline \
	'C 1,0 data=\(1,1,Y\) flags=NONE\[0\]' \
	'C 2,0 data=\(1,1,B\) flags=NONE\[0\]'

start_pane deleteline 6 4 'A\nB\nC\033[2;3r\033[2;1H\033[MY'
check_raw_matches deleteline \
	'C 1,0 data=\(1,1,Y\) flags=NONE\[0\]' \
	'C 2,0 data=\(1,1, \) flags=NONE\[0\]'

start_pane region-edge 6 4 'top\033[2;3rmid\033[2;1H\033D\033Mbot'
check_raw_matches region-edge \
	'C 0,0 data=\(1,1,m\) flags=NONE\[0\]' \
	'C 1,0 data=\(1,1,b\) flags=NONE\[0\]' \
	'C 2,0 data=\(1,1, \) flags=NONE\[0\]' \
	'C 3,0 data=\(1,1, \) flags=NONE\[0\]'

exit $exit_status
