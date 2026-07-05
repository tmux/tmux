#!/bin/sh

. ./input-common.inc

start_pane dch 10 3 'abcdef\r\033[3C\033[2PXY\n'
check_capture dch 'abcXY'

start_pane ich 10 3 'abcdef\r\033[3C\033[2@XY\n'
check_capture ich 'abcXYdef'

start_pane erase 10 3 'abcdef\r\033[3C\033[KZ\n'
check_capture erase 'abcZ'

start_pane el1 10 3 'abcdef\r\033[3C\033[1KZ\n'
check_capture el1 '   Zef'

start_pane ech 10 3 'abcdef\r\033[3C\033[2XX\n'
check_capture ech 'abcX f'

start_pane ed 10 3 'one\ntwo\033[2;2H\033[JX\n'
check_capture ed 'one
tX'

start_pane ed1 10 3 'one\ntwo\033[2;2H\033[1JX\n'
check_capture ed1 '
 Xo'

start_pane ed2 10 3 'one\ntwo\033[2JZ\n'
check_capture ed2 '
   Z'

start_pane il 8 4 '111\n222\n333\033[2;1H\033[LAAA\n'
check_capture il '111
AAA
222
333'

start_pane dl 8 4 '111\n222\n333\033[2;1H\033[MZZZ\n'
check_capture dl '111
ZZZ'

start_pane irm 10 3 'abcdef\r\033[4h\033[3CXY\033[4lZ\n'
check_capture irm 'abcXYZef'

start_pane rep 10 3 'A\033[4bB\n'
check_capture rep 'AAAAAB'

start_pane decaln 6 3 '\033#8'
check_capture decaln 'EEEEEE
EEEEEE
EEEEEE'

$TMUX kill-server 2>/dev/null
exit $exit_status
