#!/bin/sh

. ./input-common.inc

start_pane attrs 16 3 '\033[1mB\033[2mD\033[3mI\033[4mU\033[5mK\033[7mR\033[8mH\033[9mS\033[53mO'
check_raw_matches attrs \
	'C 0,0 data=\(1,1,B\) flags=NONE\[0\] attr=BRIGHT\[[0-9a-f]+\]' \
	'C 0,1 data=\(1,1,D\) flags=NONE\[0\] attr=BRIGHT,DIM\[[0-9a-f]+\]' \
	'C 0,2 data=\(1,1,I\) flags=NONE\[0\] attr=BRIGHT,DIM,ITALICS\[[0-9a-f]+\]' \
	'C 0,3 data=\(1,1,U\) flags=NONE\[0\] attr=BRIGHT,DIM,UNDERSCORE,ITALICS\[[0-9a-f]+\]' \
	'C 0,5 data=\(1,1,R\) flags=NONE\[0\] attr=BRIGHT,DIM,UNDERSCORE,BLINK,REVERSE,ITALICS\[[0-9a-f]+\]' \
	'C 0,7 data=\(1,1,S\) flags=NONE\[0\] attr=BRIGHT,DIM,UNDERSCORE,BLINK,REVERSE,HIDDEN,ITALICS,STRIKETHROUGH\[[0-9a-f]+\]' \
	'C 0,8 data=\(1,1,O\) flags=NONE\[0\] attr=BRIGHT,DIM,UNDERSCORE,BLINK,REVERSE,HIDDEN,ITALICS,STRIKETHROUGH,OVERLINE\[[0-9a-f]+\]'

start_pane colours 12 3 '\033[38;5;196;48;5;17mX\033[58;5;45mY'
check_raw_matches colours \
	'C 0,0 data=\(1,1,X\) flags=FG256,BG256\[[0-9a-f]+\] attr=NONE\[0\] fg=colour196\[10000c4\] bg=colour17\[1000011\]' \
	'C 0,1 data=\(1,1,Y\) flags=FG256,BG256\[[0-9a-f]+\] attr=NONE\[0\] fg=colour196\[10000c4\] bg=colour17\[1000011\] us=colour45\[100002d\]'

start_pane bce 8 3 '\033[44mA\033[K'
check_raw_matches bce \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\] attr=NONE\[0\].* bg=blue\[4\]' \
	'C 0,1 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\].* bg=blue\[4\]' \
	'C 0,7 data=\(1,1, \) flags=CLEARED\[[0-9a-f]+\] attr=NONE\[0\].* bg=blue\[4\]'

start_pane underlines 12 3 '\033[4:2m2\033[4:3m3\033[4:4m4\033[4:5m5'
check_raw_matches underlines \
	'C 0,0 data=\(1,1,2\) flags=NONE\[0\] attr=UNDERSCORE_2\[[0-9a-f]+\]' \
	'C 0,1 data=\(1,1,3\) flags=NONE\[0\] attr=UNDERSCORE_3\[[0-9a-f]+\]' \
	'C 0,2 data=\(1,1,4\) flags=NONE\[0\] attr=UNDERSCORE_4\[[0-9a-f]+\]' \
	'C 0,3 data=\(1,1,5\) flags=NONE\[0\] attr=UNDERSCORE_5\[[0-9a-f]+\]'

start_pane hyperlink 12 3 '\033]8;id=id1;https://example.com/a\033\\A\033]8;;\033\\B'
check_raw_matches hyperlink \
	'L 0 \(0\) flags=EXTENDED,HYPERLINK\[[0-9a-f]+\]' \
	'C 0,0 data=\(1,1,A\) flags=NONE\[0\].* link=https://example.com/a linkid=id1' \
	'C 0,1 data=\(1,1,B\) flags=NONE\[0\].* link=NONE linkid=NONE'

exit $exit_status
