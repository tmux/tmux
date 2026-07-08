#!/bin/sh

. ./input-common.inc

start_pane hyperlink 20 3 '\033]8;id=1;https://example.com\033\\link\033]8;;\033\\ plain\n'
check_capture hyperlink 'link plain'
check_flags hyperlink 'HX link plain'
$TMUX capture-pane -peH -t hyperlink: -S 0 -E - >/dev/null || exit 1

start_pane palette 20 3 '\033]4;1;rgb:11/22/33;2;red\007\033]104;1;2\007X\n'
check_capture palette 'X'

start_pane osc-colours 20 3 '\033]10;rgb:11/22/33\007\033]11;rgb:44/55/66\007\033]12;rgb:77/88/99\007\033]110\007\033]111\007\033]112\007X\n'
check_capture osc-colours 'X'

start_pane progress 20 3 '\033]9;4;1;25\007\033]9;4;0\007\033]9;4;5;200\007X\n'
check_capture progress 'X'

start_pane rename 20 3 '\033krenamed\033\\X\n'
check_capture rename 'X'

start_pane apc-title 20 3 '\033_test-title\033\\X\n'
check_capture apc-title 'X'

$TMUX kill-server 2>/dev/null
sleep 0.1
$TMUX new-session -d -x 20 -y 3 -s osc52 "sleep 2" || exit 1
$TMUX set-option -s set-clipboard on || exit 1
$TMUX respawn-pane -k -t osc52: \
    "printf '\033]52;c;SGVsbG8=\007'; sleep 2" || exit 1
sleep 0.3
$TMUX save-buffer -b buffer0 - >"$TMP"
printf "Hello" >"$EXP"
cmp "$TMP" "$EXP" || fail "osc52"

$TMUX kill-server 2>/dev/null
exit $exit_status
