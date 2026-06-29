#!/bin/sh

# Name validation policy.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

fail()
{
	echo "$*"
	$TMUX kill-server 2>/dev/null
	exit 1
}

must_fail()
{
	"$@" >/dev/null 2>&1 && fail "unexpected success: $*"
	return 0
}

must_equal()
{
	got=$1
	want=$2

	[ "$got" = "$want" ] || fail "got '$got', expected '$want'"
}

$TMUX new-session -d -x 80 -y 24 || exit 1
$TMUX set-option -qg allow-set-title on || exit 1
$TMUX set-option -qg allow-rename on || exit 1
$TMUX set-option -qg automatic-rename off || exit 1

# Commands reject ':' and '.' for sessions and windows, but allow '#'.
$TMUX rename-session 'session#ok' || fail "session name with # rejected"
must_equal "$($TMUX display-message -p '#{session_name}')" 'session#ok'
must_fail $TMUX rename-session 'session:bad'
must_fail $TMUX rename-session 'session.bad'

$TMUX rename-window 'window#ok' || fail "window name with # rejected"
must_equal "$($TMUX display-message -p '#{window_name}')" 'window#ok'
must_fail $TMUX rename-window 'window:bad'
must_fail $TMUX rename-window 'window.bad'

$TMUX set-option -q @name 'format#ok' || exit 1
$TMUX rename-session '#{@name}' || fail "format in session name not expanded"
must_equal "$($TMUX display-message -p '#{session_name}')" 'format#ok'
$TMUX rename-window '#{@name}' || fail "format in window name not expanded"
must_equal "$($TMUX display-message -p '#{window_name}')" 'format#ok'
must_fail $TMUX rename-session '#{session_name}:bad'
must_fail $TMUX rename-window '#{window_name}.bad'
pid=$($TMUX display-message -p '#{pid}')

created=$($TMUX new-session -dP -F '#{session_id}:#{window_id}' \
	-s 'new-session#ok' -n 'new-window#ok') || \
	fail "new-session name with # rejected"
created_session=${created%:*}
created_window=${created#*:}
must_equal "$($TMUX display-message -pt "$created_session" '#{session_name}')" \
	'new-session#ok'
must_equal "$($TMUX display-message -pt "$created_window" '#{window_name}')" \
	'new-window#ok'
$TMUX kill-session -t "$created_session"

must_fail $TMUX new-session -d -s 'new-session:bad'
must_fail $TMUX new-session -d -s 'new-session.bad'
must_fail $TMUX new-session -d -n 'new-window:bad'
must_fail $TMUX new-session -d -n 'new-window.bad'

created_window=$($TMUX new-window -dP -F '#{window_id}' \
	-n 'created-window#ok') || \
	fail "new-window name with # rejected"
must_equal "$($TMUX display-message -pt "$created_window" '#{window_name}')" \
	'created-window#ok'
must_fail $TMUX new-window -d -n 'created-window:bad'
must_fail $TMUX new-window -d -n 'created-window.bad'

created=$($TMUX new-session -dP -F '#{session_id}:#{window_id}' \
	-s 'new-session-#{pid}' -n 'new-window-#{pid}') || \
	fail "format in new-session name not expanded"
created_session=${created%:*}
created_window=${created#*:}
must_equal "$($TMUX display-message -pt "$created_session" '#{session_name}')" \
	"new-session-$pid"
must_equal "$($TMUX display-message -pt "$created_window" '#{window_name}')" \
	"new-window-$pid"
$TMUX kill-session -t "$created_session"

created_window=$($TMUX new-window -dP -F '#{window_id}' -n '#{@name}') || \
	fail "format in new-window name not expanded"
must_equal "$($TMUX display-message -pt "$created_window" '#{window_name}')" \
	'format#ok'
must_fail $TMUX new-session -d -s 'new-session-#{pid}:bad'
must_fail $TMUX new-session -d -n 'new-window-#{pid}.bad'
must_fail $TMUX new-window -d -n '#{window_name}:bad'

# Invalid UTF-8 is never allowed for command names.
invalid=$(printf '\302')
must_fail $TMUX rename-session "bad${invalid}name"
must_fail $TMUX rename-window "bad${invalid}name"
must_fail $TMUX new-session -d -s "bad${invalid}name"
must_fail $TMUX new-session -d -n "bad${invalid}name"
must_fail $TMUX new-window -d -n "bad${invalid}name"
must_fail $TMUX set-buffer -b "bad${invalid}name" data

# Titles set by commands allow '#', ':' and '.'.
$TMUX select-pane -T 'title#:.ok' || fail "command title rejected"
must_equal "$($TMUX display-message -p '#{pane_title}')" 'title#:.ok'
$TMUX send-keys "printf '\\033]2;title#[fg=red]ok\\007'" Enter || exit 1
sleep 1
must_equal "$($TMUX display-message -p '#{pane_title}')" 'title#[fg=red]ok'
$TMUX send-keys "printf '\\033]2;title#(bad)\\007'" Enter || exit 1
sleep 1
must_equal "$($TMUX display-message -p '#{pane_title}')" 'title_(bad)'

# Buffer names allow '#', ':' and '.'.
$TMUX set-buffer -b 'buffer#:.ok' data || fail "buffer name rejected"
must_equal "$($TMUX list-buffers -F '#{buffer_name}')" 'buffer#:.ok'

# Window names from escape sequences allow '#' except in '#('.
$TMUX send-keys "printf '\\033kescape#:.ok\\033\\\\'" Enter || exit 1
sleep 1
must_equal "$($TMUX display-message -p '#{window_name}')" 'escape#__ok'

# Titles from escape sequences reject only '#'.
$TMUX send-keys "printf '\\033]2;escape#:.ok\\007'" Enter || exit 1
sleep 1
must_equal "$($TMUX display-message -p '#{pane_title}')" 'escape#:.ok'

# Invalid UTF-8 from escape sequences is ignored.
$TMUX rename-window 'before-invalid' || exit 1
$TMUX send-keys "printf '\\033kbad\\302name\\033\\\\'" Enter || exit 1
sleep 1
must_equal "$($TMUX display-message -p '#{window_name}')" 'before-invalid'

$TMUX select-pane -T 'before-invalid-title' || exit 1
$TMUX send-keys "printf '\\033]2;bad\\302title\\007'" Enter || exit 1
sleep 1
must_equal "$($TMUX display-message -p '#{pane_title}')" 'before-invalid-title'

$TMUX kill-server 2>/dev/null
exit 0
