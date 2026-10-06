#!/bin/sh

# Exercise find-window's content, name and title filters in every combination,
# glob/regex and case-insensitive suffixes, plus zoom restoration.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

capture()
{
	$OUTER capture-pane -p -t outer:0.0 2>/dev/null
}

wait_mode()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER display-message -p -t find:base.0 '#{pane_mode}' 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "pane mode is '$got', expected '$want'"
}

run_find()
{
	label=$1
	expected=$2
	shift 2
	$INNER find-window -t find:base.0 "$@" || fail "$label failed"
	wait_mode tree-mode
	i=0
	while [ "$i" -lt 50 ]; do
		captured=$(capture)
		printf '%s\n' "$captured" | grep -Fq "$expected" && break
		sleep 0.1
		i=$((i + 1))
	done
	[ "$i" -lt 50 ] || fail "$label did not show '$expected'"
	$INNER send-keys -t find:base.0 q || exit 1
	wait_mode ''
}

$INNER new-session -d -s find -n base -x80 -y24 \
	"sh -c 'printf \"BaseBody\\n\"; exec sleep 100'" || exit 1
$INNER split-window -d -h -t find:base \
	"sh -c 'printf \"OtherBody\\n\"; exec sleep 100'" || exit 1
$INNER new-window -d -t find: -n NameNeedle 'exec sleep 100' || exit 1
$INNER new-window -d -t find: -n body \
	"sh -c 'printf \"BodyNeedle\\n\"; exec sleep 100'" || exit 1
$INNER new-window -d -t find: -n titled 'exec sleep 100' || exit 1
$INNER select-pane -t find:titled.0 -T TitleNeedle || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t find:base" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
sleep 1

# Wait until the body text is in the pane history before searching it.
i=0
while [ "$i" -lt 50 ]; do
	$INNER capture-pane -p -t find:body.0 | grep -Fq BodyNeedle && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "pane content was not ready"

# No flags means C+N+T. Then cover all three pairs and all three singletons.
run_find default NameNeedle NameNeedle
run_find content-name NameNeedle -C -N NameNeedle
run_find content-title titled -C -T TitleNeedle
run_find name-title titled -N -T TitleNeedle
run_find content-only body -C BodyNeedle
run_find name-only NameNeedle -N NameNeedle
run_find title-only titled -T TitleNeedle

# Glob case folding and all regular-expression suffix variants.
run_find insensitive NameNeedle -N -i nameneedle
run_find regex NameNeedle -N -r '^NameNeedle$'
run_find regex-insensitive NameNeedle -N -r -i '^nameneedle$'

# -Z temporarily zooms the target window and restores its prior state on exit.
$INNER find-window -Z -t find:base.0 -N NameNeedle || exit 1
wait_mode tree-mode
[ "$($INNER display-message -p -t find:base '#{window_zoomed_flag}')" = 1 ] ||
	fail "find-window -Z did not zoom"
$INNER send-keys -t find:base.0 q || exit 1
wait_mode ''
[ "$($INNER display-message -p -t find:base '#{window_zoomed_flag}')" = 0 ] ||
	fail "find-window -Z did not restore zoom"

exit 0
