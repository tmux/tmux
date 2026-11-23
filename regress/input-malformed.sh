#!/bin/sh

. ./input-common.inc

# Wait for the terminator and following text to reach the parser.
check_discard()
{
	name=$1
	printf 'OK\n' >"$EXP"
	i=0
	while [ "$i" -lt 300 ]; do
		capture_grid "$name" >"$TMP"
		cmp -s "$TMP" "$EXP" && return 0
		sleep 0.05
		i=$((i + 1))
	done
	fail "$name (timed out waiting for discard)"
}

# The missing-terminator timer may expire before the string limit is reached
# on a slow build. Clear any payload printed after the timeout, then check
# that the string did not change the title and normal output has resumed.
test_discard()
{
	name=$1
	prefix=$2
	start_cmd "$name" 8 3 "$INPUT_HOLD"
	$TMUX select-pane -T discard-test || exit 1
	$TMUX respawn-pane -k \
	    "perl -e 'print qq{$prefix}, q{x} x 1100000, qq{\e\\\\\e[H\e[2JOK}'; $INPUT_HOLD" || exit 1
	check_discard "$name"
	$TMUX display-message -p -t "$name:" '#{pane_title}' >"$TMP" || exit 1
	printf 'discard-test\n' >"$EXP"
	cmp "$TMP" "$EXP" || fail "$name title"
}

start_cmd csi-param-discard 8 3 \
    "perl -e 'print qq{\e[}, q{1} x 80, qq{\030OK}'; sleep 2"
check_capture csi-param-discard 'OK'

start_cmd csi-interm-discard 8 3 \
    "perl -e 'print qq{\e[    \030OK}'; sleep 2"
check_capture csi-interm-discard 'OK'

# Accept large values, discard excessive parameters, and keep zeroes valid.
start_pane csi-large 8 3 'X\033[2147483639CY'
check_capture csi-large 'X      Y'
start_pane csi-max 8 3 'X\033[2147483647CY'
check_capture csi-max 'XY'
start_pane csi-overflow 8 3 'X\033[2147483648CY'
check_capture csi-overflow 'XY'
zeroes=$(awk 'BEGIN { for (i = 0; i < 62; i++) printf "0" }')
start_pane csi-zeroes 8 3 "X\033[${zeroes}1CY"
check_capture csi-zeroes 'X Y'

# Apply missing parameters as defaults and discard only the bad colon group.
start_pane csi-missing 8 3 '\033[31;;mX'
check_raw_matches csi-missing 'C 0,0 .*fg=default\[8\]'
start_pane csi-colon-overflow 8 3 '\033[31;2147483648:2;32mX'
check_raw_matches csi-colon-overflow 'C 0,0 .*fg=green\[2\]'

# Check leading missing parameters and reject excessive lists.
start_pane csi-leading-missing 8 3 '\033[;3HX'
check_capture csi-leading-missing '  X'
params=$(awk 'BEGIN { for (i = 0; i < 22; i++) printf "0;"; printf "31" }')
start_pane csi-many 8 3 "\033[${params}mX"
check_raw_matches csi-many 'C 0,0 .*fg=red\[1\]'
start_pane csi-too-many 8 3 "\033[0;${params}mX"
check_raw_matches csi-too-many 'C 0,0 .*fg=default\[8\]'

test_discard osc-discard '\e]2;'
test_discard apc-discard '\e_'

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
