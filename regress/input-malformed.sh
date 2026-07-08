#!/bin/sh

. ./input-common.inc

start_cmd csi-param-discard 8 3 \
    "perl -e 'print qq{\e[}, q{1} x 80, qq{\030OK}'; sleep 2"
check_capture csi-param-discard 'OK'

start_cmd csi-interm-discard 8 3 \
    "perl -e 'print qq{\e[    \030OK}'; sleep 2"
check_capture csi-interm-discard 'OK'

start_cmd osc-discard 8 3 \
    "perl -e 'print qq{\e]2;}, q{x} x 1100000, qq{\e\\\\OK}'; sleep 2"
check_capture osc-discard 'OK'

start_cmd apc-discard 8 3 \
    "perl -e 'print qq{\e_}, q{x} x 1100000, qq{\e\\\\OK}'; sleep 2"
check_capture apc-discard 'OK'

start_pane unknown-csi 8 3 '\033[?9999zOK'
check_capture unknown-csi 'OK'

start_pane unknown-osc 8 3 '\033]999;bad\aOK'
check_capture unknown-osc 'OK'

start_pane malformed-osc 8 3 '\033]8;id=a:id=b;http://bad\aX\033]8;id=no-separator\aY\033]9;4;5;200\a\033]9;4;z\a\033]10;notacolour\a\033]11;notacolour\a\033]12;notacolour\a\033]4;999;red\a\033]104;999\a\033]52bad\a\033]52;c;@@@\aOK'
check_capture malformed-osc 'XYOK'
check_raw_matches malformed-osc \
    'C 0,0 data=\(1,1,X\).* link=NONE linkid=NONE' \
    'C 0,1 data=\(1,1,Y\).* link=NONE linkid=NONE' \
    'C 0,2 data=\(1,1,O\).* link=NONE linkid=NONE' \
    'C 0,3 data=\(1,1,K\).* link=NONE linkid=NONE'

start_pane malformed-dcs 8 3 '\033P$qBAD\033\\OK'
check_capture malformed-dcs 'OK^[P0$r
^[\'

start_pane malformed-utf8 8 3 '\360\200\200\200A\355\240\200B'
check_capture malformed-utf8 '�A�B'

exit $exit_status
