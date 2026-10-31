#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TD=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$TD/socket -f/dev/null"
HOLD='while :; do sleep 3600; done'
LIMIT=65536

cleanup()
{
	stop_server $TMUX
	rm -rf "$TD"
}
trap cleanup 0 1 15

fail()
{
	echo "FAIL: $*" >&2
	exit 1
}

value()
{
	$TMUX display-message -p -t "$1" "#{$2}"
}

write_output()
{
	tty=$(value "$1" pane_tty) || exit 1
	cat "$2" >"$tty" || exit 1
	printf '\033]2;%s\007' "$3" >"$tty" || exit 1
	wait_for_output "$3" $TMUX display-message -p -t "$1" \
	    '#{pane_title}' || exit 1
}

# history_bytes includes the visible screen. A history-free pane receiving
# the same output gives its allocation independently of the byte accounting.
check_budget()
{
	total=$(value "$1" history_bytes) || exit 1
	visible=$(value "$VIEW" history_bytes) || exit 1
	retained=$((total - visible))
	[ "$retained" -ge 0 ] && [ "$retained" -le "$2" ] ||
	    fail "$1 retains $retained bytes, budget $2"
}

PLAIN=$(printf '%060d' 0)
i=0
while [ "$i" -lt 1000 ]; do
	printf 'row%04d %s\n' "$i" "$PLAIN"
	i=$((i + 1))
done >"$TD/plain"
{
	printf '\033[38;2;17;34;51m'
	cat "$TD/plain"
	printf '\033[0m'
} >"$TD/colour"

MAIN=$($TMUX new-session -d -P -F '#{pane_id}' -s bounded \
    -x 80 -y 10 "$HOLD") || exit 1
$TMUX set-option -g history-limit 100000 || exit 1
VIEW=$($TMUX new-session -d -P -F '#{pane_id}' -s view \
    -x 80 -y 10 "$HOLD") || exit 1
$TMUX set-option -t view history-limit 0 || exit 1

# The default is disabled; enabling a byte cap trims existing history.
[ "$($TMUX show-options -gv history-byte-limit)" = 0 ] || exit 1
write_output "$MAIN" "$TD/plain" plain-main
write_output "$VIEW" "$TD/plain" plain-view
[ "$(value "$MAIN" history_bytes)" -gt "$LIMIT" ] ||
    fail 'fixture did not exceed the byte budget'
$TMUX set-option -t bounded history-byte-limit "$LIMIT" || exit 1
check_budget "$MAIN" "$LIMIT"
plain_lines=$(value "$MAIN" history_size)
[ "$plain_lines" -gt 0 ] || fail 'all plain history was discarded'
$TMUX capture-pane -p -t "$MAIN" -S - >"$TD/capture" || exit 1
grep -q '^row0999 ' "$TD/capture" || fail 'newest output was lost'
if grep -q '^row0000 ' "$TD/capture"; then
	fail 'oldest output was not discarded'
fi

# Equal line counts with true-colour cells must consume the budget sooner.
write_output "$MAIN" "$TD/colour" colour-main
write_output "$VIEW" "$TD/colour" colour-view
check_budget "$MAIN" "$LIMIT"
colour_lines=$(value "$MAIN" history_size)
[ "$colour_lines" -gt 0 ] && [ "$colour_lines" -lt "$plain_lines" ] ||
    fail "plain retained $plain_lines lines, colour retained $colour_lines"

# New panes inherit the session cap. Copy-mode refresh must survive eviction.
NEW=$($TMUX new-window -d -P -F '#{pane_id}' -t bounded \
    -n inherited "$HOLD") || exit 1
write_output "$NEW" "$TD/colour" inherited
check_budget "$NEW" "$LIMIT"
$TMUX copy-mode -t "$MAIN" || exit 1
$TMUX send-keys -t "$MAIN" -X refresh-on || exit 1
write_output "$MAIN" "$TD/colour" copy-refresh
$TMUX send-keys -t "$MAIN" -X cancel || exit 1
check_budget "$MAIN" "$LIMIT"

# Lowering the budget applies to all existing panes without new output.
LIMIT=4096
$TMUX set-option -t bounded history-byte-limit "$LIMIT" || exit 1
check_budget "$MAIN" "$LIMIT"
check_budget "$NEW" "$LIMIT"

# Resizing invalidates the cached byte count, including after reflow.
$TMUX resize-window -t "$MAIN" -x 20 -y 6 || exit 1
$TMUX resize-window -t "$VIEW" -x 20 -y 6 || exit 1
$TMUX clear-history -t "$VIEW" || exit 1
check_budget "$MAIN" "$LIMIT"
write_output "$MAIN" "$TD/colour" resized-main
write_output "$VIEW" "$TD/colour" resized-view
check_budget "$MAIN" "$LIMIT"
$TMUX clear-history -t "$MAIN" || exit 1
[ "$(value "$MAIN" history_size)" = 0 ] || exit 1
write_output "$MAIN" "$TD/colour" after-clear
check_budget "$MAIN" "$LIMIT"

# Disabling the byte cap restores line-only retention, which still applies.
$TMUX set-option -t bounded history-byte-limit 0 || exit 1
write_output "$MAIN" "$TD/colour" disabled
total=$(value "$MAIN" history_bytes)
visible=$(value "$VIEW" history_bytes)
[ "$((total - visible))" -gt "$LIMIT" ] ||
    fail 'disabled byte cap still discarded history'
$TMUX set-option -t bounded history-limit 7 || exit 1
[ "$(value "$MAIN" history_size)" -le 7 ] ||
    fail 'line limit was not applied'

exit 0
