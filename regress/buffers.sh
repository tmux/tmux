#!/bin/sh

# Tests of paste buffer command semantics, as implemented in cmd-set-buffer.c
# (set-buffer and delete-buffer), cmd-paste-buffer.c, cmd-load-buffer.c,
# cmd-save-buffer.c (save-buffer and show-buffer), cmd-list-buffers.c and
# paste.c.
#
# This exercises:
# - set-buffer creating automatic buffers (buffer0, buffer1, ... with the
#   most recent first), -b creating/replacing a named buffer, -a appending,
#   -n renaming and the error paths (no data, unknown buffer);
# - show-buffer for the top and for named buffers;
# - delete-buffer for the top and named buffers, and when nothing exists;
# - list-buffers -F custom formats and -f filters;
# - paste-buffer into a pane: newline-to-CR translation by default, -r raw,
#   -s custom separator, -d delete-after-paste, unknown buffer error;
# - the buffer-limit option evicting the oldest automatic buffers but not
#   named buffers;
# - load-buffer/save-buffer round trips including control characters and
#   UTF-8, save-buffer -a appending and errors for missing files/buffers.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
TMP2=$(mktemp)
trap 'rm -f "$TMP" "$TMP2"; $TMUX kill-server 2>/dev/null' 0 1 15

# check_ok $cmd...
#
# Run a command and require that it succeeds.
check_ok()
{
	if ! $TMUX "$@"; then
		echo "Command failed (expected success): $*"
		exit 1
	fi
}

# check_fail $expected_error $cmd...
#
# Run a command and require that it fails with the given error message.
check_fail()
{
	exp="$1"
	shift
	out=$($TMUX "$@" 2>&1)
	if [ $? -eq 0 ]; then
		echo "Command succeeded (expected failure): $*"
		exit 1
	fi
	if [ "$out" != "$exp" ]; then
		echo "Wrong error for: $*"
		echo "Expected: '$exp'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_buffers $expected
#
# Compare the buffer list (as "name=content ...", most recent first) with
# $expected.
check_buffers()
{
	out=$(echo $($TMUX list-buffers -F \
	    '#{buffer_name}=#{buffer_sample}'))
	if [ "$out" != "$1" ]; then
		echo "Buffer list wrong."
		echo "Expected: '$1'"
		echo "But got:  '$out'"
		exit 1
	fi
}

# check_show $args $expected
#
# Compare show-buffer output with $expected.
check_show()
{
	out=$($TMUX show-buffer $1 2>&1)
	if [ "$out" != "$2" ]; then
		echo "show-buffer $1 wrong."
		echo "Expected: '$2'"
		echo "But got:  '$out'"
		exit 1
	fi
}

assert_alive()
{
	if [ "$($TMUX display-message -p alive 2>&1)" != "alive" ]; then
		echo "Server died: $1"
		exit 1
	fi
}

check_ok new-session -d -s B -x 80 -y 24

# ---------------------------------------------------------------------------
# set-buffer, show-buffer, delete-buffer, list-buffers.

# Automatic buffers stack with the most recent first.
check_ok set-buffer one
check_ok set-buffer two
check_buffers 'buffer1=two buffer0=one'
check_show '' 'two'
check_show '-b buffer0' 'one'

# -a only appends to a buffer named with -b: without -b it creates a new
# automatic buffer. Empty data is silently ignored.
check_ok set-buffer -a '!'
check_buffers 'buffer2=! buffer1=two buffer0=one'
check_ok delete-buffer
check_ok set-buffer ''
check_buffers 'buffer1=two buffer0=one'

# -b names a buffer explicitly; setting it again replaces the content;
# -a appends to it.
check_ok set-buffer -b named abc
check_buffers 'named=abc buffer1=two buffer0=one'
check_ok set-buffer -b named xyz
check_ok set-buffer -a -b named 123
check_buffers 'named=xyz123 buffer1=two buffer0=one'
check_show '-b named' 'xyz123'

# -n renames; renaming to a bad source is an error, as is no data at all.
check_ok set-buffer -b named -n other
check_buffers 'other=xyz123 buffer1=two buffer0=one'
check_fail 'unknown buffer: nosuch' set-buffer -b nosuch -n foo
check_fail 'no data specified' set-buffer -b other
check_fail 'no buffer nosuch' show-buffer -b nosuch

# list-buffers -f filters.
out=$($TMUX list-buffers -f '#{==:#{buffer_name},other}' -F '#{buffer_name}')
if [ "$out" != "other" ]; then
	echo "list-buffers -f wrong: '$out'"
	exit 1
fi

# delete-buffer -b removes one buffer; without -b the most recent automatic
# buffer goes - named buffers are not candidates for the top, for
# show-buffer and delete-buffer alike.
check_ok delete-buffer -b buffer1
check_buffers 'other=xyz123 buffer0=one'
check_fail 'unknown buffer: buffer1' delete-buffer -b buffer1
check_ok delete-buffer
check_buffers 'other=xyz123'
check_fail 'no buffers' show-buffer
check_fail 'no buffer' delete-buffer
check_ok delete-buffer -b other
check_fail 'no buffers' show-buffer

# ---------------------------------------------------------------------------
# buffer-limit.

# Only automatic buffers count against buffer-limit and the oldest are
# evicted; named buffers survive. (Automatic buffer numbers keep counting
# up over the life of the server, so compare content only.)
check_ok set-option -g buffer-limit 3
check_ok set-buffer -b keepme precious
check_ok set-buffer a1
check_ok set-buffer a2
check_ok set-buffer a3
check_ok set-buffer a4
out=$(echo $($TMUX list-buffers -F '#{buffer_sample}'))
if [ "$out" != 'a4 a3 a2 precious' ]; then
	echo "buffer-limit eviction wrong: '$out'"
	exit 1
fi
check_ok set-option -g buffer-limit 50
check_ok delete-buffer -b keepme
check_ok delete-buffer; check_ok delete-buffer; check_ok delete-buffer

# ---------------------------------------------------------------------------
# paste-buffer.

# Paste into a raw, echo-free pane running cat -v so control characters are
# visible; a fresh window per paste keeps assertions simple.

# paste_line $bufdata $pasteargs $expected
#
# Set a buffer, paste it into a fresh cat -v pane and compare the first
# screen line with $expected.
paste_line()
{
	$TMUX kill-window -t B:9 2>/dev/null
	check_ok new-window -d -t B:9 'stty raw -echo && exec cat -v'
	i=0
	while [ "$($TMUX display-message -p -t B:9.0 \
	    '#{pane_current_command}')" != "cat" ]; do
		i=$((i + 1))
		[ $i -gt 50 ] && { echo "cat did not start."; exit 1; }
		sleep 0.1
	done
	check_ok set-buffer -b paste "$1"
	check_ok paste-buffer $2 -b paste -t B:9.0
	i=0
	while out=$($TMUX capture-pane -p -t B:9.0 | sed -n 1p) && \
	    [ "$out" != "$3" ]; do
		i=$((i + 1))
		if [ $i -gt 50 ]; then
			echo "Paste of '$1' ($2) wrong."
			echo "Expected: '$3'"
			echo "But got:  '$out'"
			exit 1
		fi
		sleep 0.1
	done
}

# By default linefeeds are replaced with carriage returns (shown as ^M by
# cat -v); -r pastes raw and -s sets an explicit separator.
paste_line 'one
two' '' 'one^Mtwo'
paste_line 'one
two' '-r' 'one'
paste_line 'one
two' '-s |' 'one|two'
paste_line 'one
two' '-s XX' 'oneXXtwo'

# -d deletes the buffer after pasting.
paste_line 'gone' '-d' 'gone'
check_fail 'no buffer paste' show-buffer -b paste

# Unknown buffer is an error.
check_fail 'no buffer nosuch' paste-buffer -b nosuch -t B:9.0
check_ok kill-window -t B:9

# ---------------------------------------------------------------------------
# load-buffer and save-buffer.

# Round trip a file with control characters and UTF-8 through load-buffer
# and save-buffer.
printf 'line1\tx\033[31m\001\002\303\251\n' >"$TMP"
check_ok load-buffer -b file "$TMP"
check_ok save-buffer -b file "$TMP2"
if ! cmp -s "$TMP" "$TMP2"; then
	echo "load-buffer/save-buffer round trip differs."
	exit 1
fi

# save-buffer -a appends.
check_ok save-buffer -a -b file "$TMP2"
cat "$TMP" "$TMP" >"$TMP".x
if ! cmp -s "$TMP".x "$TMP2"; then
	rm -f "$TMP".x
	echo "save-buffer -a did not append."
	exit 1
fi
rm -f "$TMP".x

# show-buffer prints the loaded content (text form).
check_ok delete-buffer -b file

# load-buffer of a missing file and save-buffer of a missing buffer or to a
# bad path are errors.
check_fail "No such file or directory: $TMP.nosuch" \
	load-buffer -b x "$TMP.nosuch"
check_fail 'no buffer nosuch' save-buffer -b nosuch "$TMP2"
check_ok set-buffer -b sb data
out=$($TMUX save-buffer -b sb /nonexistent/dir/file 2>&1)
if [ $? -eq 0 ]; then
	echo "save-buffer to bad path succeeded."
	exit 1
fi

# save-buffer - writes to stdout and load-buffer - reads from stdin.
out=$($TMUX save-buffer -b sb -)
if [ "$out" != "data" ]; then
	echo "save-buffer - wrong: '$out'"
	exit 1
fi
check_ok delete-buffer -b sb
printf 'from stdin' | $TMUX load-buffer -b stdinbuf -
check_show '-b stdinbuf' 'from stdin'
check_ok delete-buffer -b stdinbuf

assert_alive

$TMUX kill-server 2>/dev/null
exit 0
