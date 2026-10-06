#!/bin/sh

# Exercise switch-mode's session and window lists with a real attached client:
# fuzzy filtering, movement and selection, the default and custom commands,
# no-match cancellation, resize, zoom restoration, and -k mode teardown.

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

wait_capture()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		captured=$(capture)
		printf '%s\n' "$captured" | grep -Fq "$marker" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "timed out waiting for '$marker'"
}

wait_count()
{
	marker=$1
	want=$2
	i=0
	while [ "$i" -lt 50 ]; do
		captured=$(capture)
		count=$(printf '%s\n' "$captured" | grep -Fc "$marker")
		[ "$count" -eq "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "found $count '$marker' rows, expected $want"
}

wait_format()
{
	target=$1
	format=$2
	want=$3
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER display-message -p -t "$target" "$format" \
		    2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "$target format $format is '$got', expected '$want'"
}

wait_client_session()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER list-clients -F '#{client_session}' 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "client session is '$got', expected '$want'"
}

# Three alphabetically ordered sessions. alpha:main has two panes so -Z can be
# observed; the two pick-* windows make a deterministic two-row window filter.
$INNER new-session -d -s alpha -n main -x60 -y15 'exec sleep 100' || exit 1
$INNER split-window -d -h -t alpha:main 'exec sleep 100' || exit 1
$INNER new-window -d -t alpha: -n pick-one 'exec sleep 100' || exit 1
$INNER new-session -d -s bravo -n pick-two -x60 -y15 \
	'exec sleep 100' || exit 1
$INNER new-session -d -s charlie -n last -x60 -y15 \
	'exec sleep 100' || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER select-window -t alpha:main || exit 1

$OUTER new-session -d -s outer -x60 -y15 "$INNER attach -t alpha" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
wait_client_session alpha

# Session rows are sorted alpha, bravo, charlie. Up from the first row wraps to
# charlie; Enter runs a custom command with the selected session target.
$INNER set-option -g @picked '' || exit 1
$INNER switch-mode -s -t alpha:main.0 -F 'SESSION #{session_name}' \
	"set-option -g @picked '%%'" || exit 1
wait_count 'SESSION ' 3
$INNER send-keys -t alpha:main.0 Up Enter || exit 1
wait_format alpha:main.0 '#{@picked}' '=charlie:'
wait_format alpha:main.0 '#{pane_in_mode}' 0

# Fuzzy filtering keeps only bravo for "brv". The default command switches the
# real client to that session and closes the mode.
$INNER switch-mode -s -t alpha:main.0 -F 'SESSION #{session_name}' || exit 1
wait_count 'SESSION ' 3
$INNER send-keys -t alpha:main.0 -l brv || exit 1
wait_count 'SESSION ' 1
printf '%s\n' "$captured" | grep -Fq 'SESSION bravo' ||
	fail "fuzzy session filter did not select bravo"
$INNER send-keys -t alpha:main.0 Enter || exit 1
wait_client_session bravo
client=$($INNER list-clients -F '#{client_name}')
$INNER switch-client -c "$client" -t alpha || exit 1
wait_client_session alpha

# Window mode sees pick-one then pick-two for the "pick" filter. Down selects
# the second row, and the custom command receives its window target.
$INNER set-option -g @picked '' || exit 1
$INNER switch-mode -w -t alpha:main.0 -F \
	'WINDOW #{session_name}:#{window_name}' \
	"set-option -g @picked '%%'" || exit 1
wait_capture 'WINDOW '
$INNER send-keys -t alpha:main.0 -l pick || exit 1
wait_count 'WINDOW ' 2
$INNER send-keys -t alpha:main.0 Down Enter || exit 1
picked=$($INNER show-option -gv @picked)
case "$picked" in
=bravo:0.) ;;
*) fail "window selection produced '$picked', expected '=bravo:0.'" ;;
esac

# Navigation with an empty result set must be harmless, and Escape cancels
# without running the custom command.
$INNER set-option -g @picked unchanged || exit 1
$INNER switch-mode -s -t alpha:main.0 -F 'NONE #{session_name}' \
	"set-option -g @picked '%%'" || exit 1
wait_capture 'NONE '
$INNER send-keys -t alpha:main.0 -l no-such-session || exit 1
wait_count 'NONE ' 0
$INNER send-keys -t alpha:main.0 Up Down PPage NPage Home End || exit 1
wait_format alpha:main.0 '#{pane_mode}' switch-mode
$INNER send-keys -t alpha:main.0 Escape || exit 1
wait_format alpha:main.0 '#{pane_in_mode}' 0
[ "$($INNER show-option -gv @picked)" = unchanged ] ||
	fail "cancelled switch-mode ran its command"

# -Z temporarily zooms an unzoomed window. Resizing while the mode is open
# rebuilds and redraws it; exiting restores the original unzoomed state.
$INNER switch-mode -Zs -t alpha:main.0 -F 'RESIZE #{session_name}' || exit 1
wait_capture 'RESIZE alpha'
wait_format alpha:main.0 '#{window_zoomed_flag}' 1
$INNER resize-window -t alpha:main -x50 -y12 || exit 1
wait_format alpha:main.0 '#{window_width}x#{window_height}' 50x12
wait_format alpha:main.0 '#{pane_mode}' switch-mode
$INNER send-keys -t alpha:main.0 Escape || exit 1
wait_format alpha:main.0 '#{window_zoomed_flag}' 0

# If the window was already zoomed, leaving -Z mode must keep it zoomed.
$INNER resize-pane -Z -t alpha:main.0 || exit 1
$INNER switch-mode -Zs -t alpha:main.0 -F 'ZOOMED #{session_name}' || exit 1
wait_capture 'ZOOMED alpha'
$INNER send-keys -t alpha:main.0 Escape || exit 1
wait_format alpha:main.0 '#{window_zoomed_flag}' 1
$INNER resize-pane -Z -t alpha:main.0 || exit 1

# -k uses the shared mode teardown flag. Put it on a disposable pane and make
# sure exiting the mode removes that pane.
kill_pane=$($INNER split-window -d -P -F '#{pane_id}' -t alpha:main \
	'exec sleep 100') || exit 1
$INNER switch-mode -ks -t "$kill_pane" -F 'KILL #{session_name}' || exit 1
wait_format "$kill_pane" '#{pane_mode}' switch-mode
$INNER send-keys -t "$kill_pane" Escape || exit 1
i=0
while [ "$i" -lt 50 ]; do
	$INNER list-panes -s -t alpha -F '#{pane_id}' 2>/dev/null | \
	    grep -q -x "$kill_pane" || break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "switch-mode -k did not kill its pane"

exit 0
