#!/bin/sh

. ./input-common.inc

# Clear wrapping when an erase reaches the right edge.
for bg in '\033[0m' '\033[44m' '\033[48;2;12;34;56m'; do
	start_pane el-wrap 6 3 "${bg}abcdefG\033[1;3H\033[K"
	check_joined el-wrap 'ab
G'

	start_pane ech-wrap 6 3 "${bg}abcdefG\033[1;5H\033[2X"
	check_joined ech-wrap 'abcd
G'

	# Keep wrapping when the right edge is untouched.
	start_pane ech-middle 6 3 "${bg}abcdefG\033[1;3H\033[2X"
	check_joined ech-middle 'ab  efG'

	start_pane el-start 6 3 "${bg}abcdefG\033[1;3H\033[1K"
	check_joined el-start '   defG'
done

exit $exit_status
