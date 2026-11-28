#!/bin/sh

# Exercise clipped gutter drawing through a real client.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
TMP=$(mktemp)
exit_status=0
trap 'rm -f "$TMP"; $INNER kill-server 2>/dev/null; $OUTER kill-server 2>/dev/null' 0 1 15

# Wait for the expected cells to reach the attached client.
capture_matches()
{
	pattern=$1
	shift
	$OUTER capture-pane -p "$@" >"$TMP" || return 1
	grep -q "$pattern" "$TMP"
}

$INNER new-session -d -x 80 -y 12 \
	"printf '\033]133;A\007p> \033]133;B\007failed\r\n\033]133;C\007012345678901234567890123456789012345678901234567890123456789\033]133;D;7\007\r\n\033]133;A\007p> \033]133;B\007'; exec sleep 100" || exit 1
$INNER set -g status off || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g copy-mode-position-format '' || exit 1
$INNER set -g copy-mode-exit-status-format \
	'#{?#{!=:#{exit_status},},RC=#{exit_status},}' || exit 1
wait_for_text 'p> failed' $INNER capture-pane -p || exit 1
$OUTER new-session -d -x 80 -y 12 "$INNER attach" || exit 1
$OUTER set -g status off || exit 1
wait_for_clients 1 $INNER || exit 1

# Reflow must move the exit status presence flag with its numeric value.
for width in 20 100; do
	$INNER resize-window -x "$width" || exit 1
	$INNER set -g copy-mode-exit-status-format \
		"#{?#{!=:#{exit_status},},RC=$width:#{exit_status},}" || exit 1
	$INNER copy-mode -U || exit 1
	$INNER send -X history-top || exit 1
	if ! wait_until capture_matches "RC=$width:7"; then
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
if ! wait_until capture_matches '^XXXXXXXXXXXXXXX'; then
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
	if [ "$width" = 3 ]; then
		wait_until capture_matches '^1 p' || exit 1
	else
		wait_until capture_matches '^p' || exit 1
	fi
done

# Gutter formats have the same pane context as other copy-mode formats.
$INNER send -X cancel || exit 1
$INNER resize-window -x 80 || exit 1
$INNER set -p @gutter_label PANE || exit 1
$INNER set -p copy-mode-exit-status-format \
	'#{@gutter_label}:#{pane_id}:#{exit_status}' || exit 1
$INNER copy-mode -U || exit 1
$INNER send -X history-top || exit 1
pane=$($INNER display -p '#{pane_id}')
if ! wait_until capture_matches "PANE:$pane:7"; then
	echo 'gutter format lost pane context'
	exit_status=1
fi

# A changing gutter width must move the selection with its text.
$INNER send -X cancel || exit 1
$INNER set -g @gutter_width X || exit 1
$INNER set -p copy-mode-exit-status-format '#{@gutter_width}' || exit 1
$INNER set -g copy-mode-selection-style 'fg=white,bg=blue' || exit 1
$INNER copy-mode -U || exit 1
$INNER send -X history-top || exit 1
$INNER send -X search-forward-text 0123 || exit 1
$INNER send -X start-of-line || exit 1
$INNER send -X begin-selection || exit 1
$INNER send -X -N 3 cursor-right || exit 1
$INNER set -g @gutter_width WIDE || exit 1
if ! wait_until capture_matches 'C 1,6 data=(1,1,0).* bg=blue\[4\]' -R; then
	echo 'selection did not follow the wider gutter'
	exit_status=1
fi
$INNER send -X copy-selection || exit 1
[ "$($INNER show-buffer)" = 012 ] || exit 1

# Changing the format itself must also recalculate the gutter width.
$INNER set -p copy-mode-exit-status-format CHANGED || exit 1
if ! wait_until capture_matches 'CHANGED- p>'; then
	echo 'gutter format change did not update its width'
	exit_status=1
fi

exit "$exit_status"
