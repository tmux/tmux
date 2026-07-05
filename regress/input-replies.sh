#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"

$TMUX kill-server 2>/dev/null
sleep 0.5

TMP=$(mktemp)
EXP=$(mktemp)
trap 'rm -f "$TMP" "$EXP"; $TMUX kill-server 2>/dev/null' 0 1 15

$TMUX new-session -d -x 80 -y 24 -s replies \; \
    set-window-option -t replies:0 remain-on-exit on || exit 1
$TMUX set-option -s set-clipboard on || exit 1
$TMUX set-option -s get-clipboard buffer || exit 1
printf Hello | $TMUX load-buffer -
sleep 0.5

exit_status=0

fail()
{
	echo "FAIL: $1"
	diff -u "$EXP" "$TMP"
	exit_status=1
}

query()
{
	name=$1
	expected=$2
	seq=$3
	count=$4
	setup=$5

	$TMUX respawn-window -k -t replies:0 \
	    "stty raw -echo min 1 time 20; printf '$setup'; printf '$seq'; dd bs=1 count=$count 2>/dev/null | cat -v >$TMP"
	sleep 0.5
	printf "%s" "$expected" >"$EXP"
	cmp "$TMP" "$EXP" || fail "$name"
}

query_timeout()
{
	name=$1
	expected=$2
	seq=$3
	setup=$4

	$TMUX respawn-window -k -t replies:0 \
	    "stty raw -echo min 0 time 5; printf '$setup'; printf '$seq'; sleep 0.1; dd bs=1 count=128 2>/dev/null | cat -v >$TMP"
	sleep 1
	printf "%s" "$expected" >"$EXP"
	cmp "$TMP" "$EXP" || fail "$name"
}

query "dsr-ok" '^[[0n' '\033[5n' 4 ''
query "dsr-cursor" '^[[1;1R' '\033[6n' 6 ''
query "da-primary" '^[[?1;2c' '\033[c' 7 ''
query "da-secondary" '^[[>84;0;0c' '\033[>c' 10 ''
query "decrqm-irm-reset" '^[[4;2$y' '\033[4$p' 7 ''
query "decrqm-irm-set" '^[[4;1$y' '\033[4$p' 7 '\033[4h'
query "decrqm-cursor-keys-reset" '^[[?1;2$y' '\033[?1$p' 8 ''
query "decrqm-cursor-keys-set" '^[[?1;1$y' '\033[?1$p' 8 '\033[?1h'
query "decrqm-columns" '^[[?3;4$y' '\033[?3$p' 8 ''
query "decrqm-origin-reset" '^[[?6;2$y' '\033[?6$p' 8 ''
query "decrqm-origin-set" '^[[?6;1$y' '\033[?6$p' 8 '\033[?6h'
query "decrqm-wrap-set" '^[[?7;1$y' '\033[?7$p' 8 ''
query "decrqm-wrap-reset" '^[[?7;2$y' '\033[?7$p' 8 '\033[?7l'
query "decrqm-cursor-visible-set" '^[[?25;1$y' '\033[?25$p' 9 ''
query "decrqm-cursor-visible-reset" '^[[?25;2$y' '\033[?25$p' 9 '\033[?25l'
query "decrqm-mouse-standard-set" '^[[?1000;1$y' '\033[?1000$p' 11 '\033[?1000h'
query "decrqm-mouse-button-set" '^[[?1002;1$y' '\033[?1002$p' 11 '\033[?1002h'
query "decrqm-mouse-all-set" '^[[?1003;1$y' '\033[?1003$p' 11 '\033[?1003h'
query "decrqm-focus-set" '^[[?1004;1$y' '\033[?1004$p' 11 '\033[?1004h'
query "decrqm-mouse-utf8-set" '^[[?1005;1$y' '\033[?1005$p' 11 '\033[?1005h'
query "decrqm-mouse-sgr-set" '^[[?1006;1$y' '\033[?1006$p' 11 '\033[?1006h'
query "decrqm-bracket-paste-set" '^[[?2004;1$y' '\033[?2004$p' 11 '\033[?2004h'
query "decrqm-theme-updates-set" '^[[?2031;1$y' '\033[?2031$p' 11 '\033[?2031h'
query "decrqss-cursor-style" '^[P1$r q0 q^[\' '\033P$q q\033\\' 12 ''

query_timeout "osc-10-query" '^[]10;rgb:ffff/0000/0000^G' '\033]10;?\007' '\033]10;red\007'
query_timeout "osc-11-query" '^[]11;rgb:0000/0000/ffff^G' '\033]11;?\007' '\033]11;blue\007'
query_timeout "osc-12-query" '^[]12;rgb:0000/ffff/0000^G' '\033]12;?\007' '\033]12;green\007'
query_timeout "osc-4-query" '^[]4;1;rgb:ffff/0000/0000^G' '\033]4;1;?\007' '\033]4;1;red\007'
query_timeout "osc-104-reset-query" '' '\033]4;1;?\007' '\033]4;1;red\007\033]104;1\007'
query_timeout "osc-52-query" '^[]52;c;SGVsbG8=^G' '\033]52;c;?\007' ''

$TMUX kill-server 2>/dev/null
exit $exit_status
