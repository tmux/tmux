#!/bin/sh

# OSC 9;9 sets the pane path like OSC 7, as a file:// URI with this host's
# name, with or without quotes around the path; only an absolute path is
# used. OSC 9;4 is still the progress bar.

. ./input-common.inc

HOST=$(uname -n)

check_path()
{
	name=$1
	want=$2

	got=$($TMUX display-message -p -t "$name:" '#{pane_path}')
	if [ "$got" != "$want" ]; then
		echo "FAIL: $name: pane_path is '$got', not '$want'"
		exit_status=1
	fi
}

start_pane osc-9-9 20 3 '\033]9;9;/tmp/x\033\\X\n'
check_capture osc-9-9 'X'
check_path osc-9-9 "file://$HOST/tmp/x"

start_pane osc-9-9-quoted 20 3 '\033]9;9;"/tmp/y"\007X\n'
check_capture osc-9-9-quoted 'X'
check_path osc-9-9-quoted "file://$HOST/tmp/y"

start_pane osc-9-9-escaped 20 3 '\033]9;9;/tmp/a b\303\251%%\033\\X\n'
check_capture osc-9-9-escaped 'X'
check_path osc-9-9-escaped "file://$HOST/tmp/a%20b%C3%A9%25"

start_pane osc-9-9-relative 20 3 '\033]9;9;tmp/x\033\\X\n'
check_capture osc-9-9-relative 'X'
check_path osc-9-9-relative ''

start_pane osc-9-9-drive 20 3 '\033]9;9;C:/x\033\\X\n'
check_capture osc-9-9-drive 'X'
check_path osc-9-9-drive ''

start_pane osc-7 20 3 '\033]7;file://host/tmp/z\033\\X\n'
check_capture osc-7 'X'
check_path osc-7 file://host/tmp/z

start_pane osc-9-4 20 3 '\033]9;4;1;25\007X\n'
check_capture osc-9-4 'X'
check_path osc-9-4 ''
progress=$($TMUX display-message -p -t osc-9-4: '#{pane_pb_progress}')
if [ "$progress" != 25 ]; then
	echo "FAIL: osc-9-4: pane_pb_progress is '$progress', not 25"
	exit_status=1
fi

exit $exit_status
