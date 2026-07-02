#!/bin/sh

. ./input-common.inc

start_pane sgr-basic 20 3 '\033[1;2;3;4;5;7;8;9mA\033[22;23;24;25;27;28;29mB\n'
check_capture sgr-basic 'AB'
$TMUX capture-pane -peN -t sgr-basic: -S 0 -E - >/dev/null || exit 1

start_pane sgr-colour 20 3 '\033[31;42mA\033[38;5;196;48;5;22mB\033[38;2;1;2;3;48;2;4;5;6mC\033[39;49mD\n'
check_capture sgr-colour 'ABCD'
$TMUX capture-pane -peN -t sgr-colour: -S 0 -E - >/dev/null || exit 1

start_pane sgr-underline 20 3 '\033[4:1mA\033[4:2mB\033[4:3mC\033[4:4mD\033[4:5mE\033[4:0mF\n'
check_capture sgr-underline 'ABCDEF'
$TMUX capture-pane -peN -t sgr-underline: -S 0 -E - >/dev/null || exit 1

start_pane sgr-uscolour 20 3 '\033[58;5;45;4mA\033[58:2::10:20:30mB\033[59mC\n'
check_capture sgr-uscolour 'ABC'
$TMUX capture-pane -peN -t sgr-uscolour: -S 0 -E - >/dev/null || exit 1

start_pane sgr-reset 20 3 '\033[90;100mA\033[0mB\033[91;101mC\033[39;49mD\n'
check_capture sgr-reset 'ABCD'
$TMUX capture-pane -peN -t sgr-reset: -S 0 -E - >/dev/null || exit 1

$TMUX kill-server 2>/dev/null
exit $exit_status
