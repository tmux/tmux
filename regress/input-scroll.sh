#!/bin/sh

. ./input-common.inc

start_pane wrap 5 3 'abcdeF'
check_capture wrap 'abcde
F'
check_cursor wrap '1,1'
check_flags wrap 'W abcde
- F'
check_joined wrap 'abcdeF'

start_pane wraplast 5 3 'abcd\033[5GZQ'
check_capture wraplast 'abcdZ
Q'
check_cursor wraplast '1,1'

start_pane nowrap 5 3 '\033[?7labcdeF'
check_capture nowrap 'abcdF'
check_cursor nowrap '4,0'

start_pane origin 6 4 '111111\n222222\n333333\n444444\033[2;3r\033[?6h\033[1;1HAA\033[?6l\033[r'
check_capture origin '111111
AA2222
333333
444444'

start_pane scrollup 5 4 '11111\n22222\n33333\n44444\033[2;3r\033[3;1HAAAAA\nBBBBB\033[r'
check_capture scrollup '11111
AAAAA
BBBBB
44444'

start_pane scrolldown 5 4 '11111\n22222\n33333\n44444\033[2;3r\033[2;1H\033[TZZZZZ\033[r'
check_capture scrolldown '11111
ZZZZZ
22222
44444'

start_pane ri 5 4 '11111\n22222\n33333\n44444\033[2;3r\033[2;1H\033MZZZZZ\033[r'
check_capture ri '11111
ZZZZZ
22222
44444'

start_pane nel 5 3 'AA\033EBC\n'
check_capture nel 'AA
BC'

$TMUX kill-server 2>/dev/null
sleep 0.1
$TMUX new-session -d -x 5 -y 3 -s history \; \
    set-option -g history-limit 3 \; \
    respawn-pane -k "printf '01\n02\n03\n04\n05\n06'; sleep 2" || exit 1
sleep 0.3
$TMUX capture-pane -pN -t history: -S - -E - | normalize_capture >"$TMP"
printf "%s\n" '01
02
03
04
05
06' >"$EXP"
cmp "$TMP" "$EXP" || fail "history limit"

$TMUX clear-history -t history:
$TMUX capture-pane -pN -t history: -S - -E - | normalize_capture >"$TMP"
printf "%s\n" '04
05
06' >"$EXP"
cmp "$TMP" "$EXP" || fail "clear-history"

$TMUX kill-server 2>/dev/null
exit $exit_status
