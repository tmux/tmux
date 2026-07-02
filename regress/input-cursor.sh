#!/bin/sh

. ./input-common.inc

start_pane cursor 10 3 'ABCDE\r\033[2Cxy\033[1D!\033[4GZ\n'
check_capture cursor 'ABxZE'
check_cursor cursor '0,1'

start_pane saverc 10 3 'abc\0337\033[2;5HXY\0338Z\n'
check_capture saverc 'abcZ
    XY'
check_cursor saverc '0,1'

start_pane hvp 10 4 'A\033[3dB\033[5GC\033[2;2fD\n'
check_capture hvp 'A
 D
 B  C'
check_cursor hvp '0,2'

start_pane cursorlines 8 4 'A\033[2BB\033[1FC\033[1AD\n'
check_capture cursorlines 'AD
C
 B'
check_cursor cursorlines '0,1'

start_pane tabs 12 3 'a\tb\n'
check_capture tabs 'a	b'
check_cursor tabs '0,1'

start_pane tabclear 12 3 '\033H\ta\033[3g\r\tb\n'
check_capture tabclear '	a  b'
check_cursor tabclear '0,1'

start_pane cbt 16 3 '0123456789\r\033[10C\033[Zx\n'
check_capture cbt '01234567x9'
check_cursor cbt '0,1'

$TMUX kill-server 2>/dev/null
exit $exit_status
