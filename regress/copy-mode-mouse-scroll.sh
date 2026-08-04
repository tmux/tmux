#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX1="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

$TMUX1 kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null

cleanup()
{
	rm -f "$MOUSE"
	$TMUX1 kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

MOUSE=$(mktemp) || exit 1

# Start an attached client for the inner server inside an outer pane. This
# lets raw terminal mouse reports reach the inner client.
$TMUX2 new -d -x80 -y24 \
	"awk 'BEGIN { for (i = 0; i < 320000; i++) printf \"x\"; printf \"\\n\" }'; cat" ||
	exit 1
$TMUX1 new -d -x80 -y24 "$TMUX2 attach" || exit 1
sleep 1

$TMUX2 copy-mode || exit 1

# Send fifty real SGR wheel reports, each of which scrolls five rows.
i=0
while [ "$i" -lt 50 ]; do
	printf '\033[<64;40;10M' >>"$MOUSE"
	i=$((i + 1))
done
$TMUX1 load-buffer -b mouse "$MOUSE" || exit 1
$TMUX1 paste-buffer -S -b mouse || exit 1

# Disabled debug logging must not expand mouse_word for every wheel report.
end=$(( $(date +%s) + 10 ))
while :; do
	position=$($TMUX2 display-message -p '#{scroll_position}') || exit 1
	[ "$position" -eq 250 ] && break
	[ "$(date +%s)" -ge "$end" ] && exit 1
	sleep 1
done

exit 0
