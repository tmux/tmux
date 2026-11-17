#!/bin/sh

# Exercise clipped gutter drawing through a real client.

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
TMP=$(mktemp)
exit_status=0
trap 'rm -f "$TMP"; $INNER kill-server 2>/dev/null; $OUTER kill-server 2>/dev/null' 0 1 15

$INNER new-session -d -x 80 -y 12 \
	"printf '\033]133;A\007p> \033]133;B\007failed\r\n\033]133;C\007012345678901234567890123456789012345678901234567890123456789\033]133;D;7\007\r\n\033]133;A\007p> \033]133;B\007'; exec sleep 100" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g copy-mode-position-format '' || exit 1
$INNER set -g copy-mode-exit-status-format \
	'#{?exit_status_present,RC=#{exit_status},}' || exit 1
$OUTER new-session -d -x 80 -y 12 "$INNER attach" || exit 1
$OUTER set -g status off || exit 1
sleep 1

# Reflow must move the exit status presence flag with its numeric value.
for width in 20 100; do
	$INNER resize-window -x "$width" || exit 1
	$INNER copy-mode -U || exit 1
	$INNER send -X history-top || exit 1
	sleep 0.3
	$OUTER capture-pane -p >"$TMP" || exit 1
	if ! grep -q RC=7 "$TMP"; then
		echo "exit status disappeared at width $width"
		exit_status=1
	fi
	$INNER send -X cancel || exit 1
done

# A gutter wider than the pane must not wrap or scroll the displayed rows.
$INNER resize-window -x 20 || exit 1
$INNER set -g copy-mode-exit-status-format \
	'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX' || exit 1
$INNER copy-mode -c || exit 1
$INNER send -X history-top || exit 1
sleep 0.3
$OUTER capture-pane -p >"$TMP" || exit 1
if ! grep -q '^XXXXXXXXXXXXXXX' "$TMP"; then
	echo 'wide gutter scrolled the displayed rows'
	exit_status=1
fi
$INNER display -p '#{copy_cursor_y}' | \
	awk '$1 < 12 { found = 1 } END { exit !found }' || exit 1

# Line numbers must also fit when the gutter leaves no room for them.
$INNER send -X line-numbers-on || exit 1
for width in 3 1; do
	$INNER resize-window -x "$width" || exit 1
	$INNER send -X history-top || exit 1
	sleep 0.3
	$OUTER capture-pane -p >"$TMP" || exit 1
	if [ "$width" = 3 ]; then
		grep -q '^1 p' "$TMP" || exit 1
	else
		grep -q '^p' "$TMP" || exit 1
	fi
done

exit "$exit_status"
