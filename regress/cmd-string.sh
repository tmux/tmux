#!/bin/sh

# Tests of string escapes in command arguments.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"

tmux_command_output()
{
	rc=0
	command=0
	while read -r line; do
		[ ! -z "$DEBUG" ] && echo "Got $line" 1>&2
		case "$line" in
			?begin*) command=$((command+1)); continue;;
			?end*)   [ $command -eq 2 ] && continue;;
			?error*) rc=1; continue;;
		esac
		if [ $command -eq 2 ]; then
			printf "%s\n" "$line"
		fi
	done
	return $rc
}

tmux_command()
{
	# CONTROL MODE is used since string escapes are not evaluated for
	# normal shell-originating commands.
	printf "%s\n" "$1" | $TMUX -C attach-session | tmux_command_output
	return $?
}

# test_format $format $expected_result
test_format()
{
	fmt="$1"
	exp="$2"

	out=$(tmux_command "display-message -p \"$fmt\"")

	if [ "$out" != "$exp" ]; then
		echo "Format test failed for '$fmt'."
		echo "Expected: '$exp'"
		echo "But got   '$out'"
		exit 1
	fi
}

test_bad_format()
{
	fmt="$1"
	out="$(tmux_command "display-message -p \"$fmt\"")"
	rc=$?
	if [ $rc -eq 0 ]; then
		echo "Format test failed for '$fmt'."
		echo "Expected failure"
		echo "But got '$out' with rc $rc"
		exit 1
	fi
}

$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null new-session -d || exit 1

# Plain unicode -> utf8 substitutions
test_format '\u23F1' "$(printf '\342\217\261')"
test_format '\u23F11' "$(printf '\342\217\261')1"
test_format '\U0001F550' "$(printf '\360\237\225\220')"
test_format '\U0001F5501' "$(printf '\360\237\225\220')1"
# Escaped leading \
test_format '\\u23F1' '\u23F1'
test_format '\\U0001F550' '\U0001F550'
# Command parse failure
test_bad_format '\u000'
test_bad_format '\U0000000'

test_format '\e' "$(printf '\033')"
test_format 'x\tx' "$(printf 'x\tx')"
test_format '\r' "$(printf '\r')"
test_format '\n' "$(printf '\n')"
test_format '\\' '\'

exit 0
