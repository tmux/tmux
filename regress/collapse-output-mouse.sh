#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
trap 'stop_server $OUTER; stop_server $INNER' 0
trap 'exit 1' 1 2 15

shared='\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007FIRST\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007SECOND\033]133;D;0\007\r\n\033]133;A\007P2> \033]133;B\007'
$INNER new-session -d -s test -x80 -y12 \
	"printf '$shared'; exec sleep 300" || exit 1
$INNER set -g status off || exit 1
$INNER set -g mouse on || exit 1
$INNER set -g window-size manual || exit 1
$INNER set -g copy-mode-position-format '' || exit 1
$OUTER new-session -d -x80 -y12 "$INNER attach -t test" || exit 1
$OUTER set -g status off || exit 1
wait_for_clients 1 $INNER || exit 1

# Click a gutter row through the attached client, then check the folded text.
click()
{
	row=$1
	expected=$2

	# Keep consecutive cases outside the double-click timeout.
	sleep 0.4
	sequence=$(printf '\033[<0;1;%sM\033[<0;1;%sm' "$row" "$row")
	$OUTER send -l "$sequence" || exit 1
	wait_for_output "$expected" $INNER capture-pane -t test: -Mp || exit 1
}

# Gutter rows identify their own command, even beside shared-line output.
for keys in emacs vi; do
	$INNER set -w -t test: mode-keys "$keys" || exit 1
	for row in 1 2; do
		$INNER copy-mode -t test: -U || exit 1
		$INNER send -t test: -X search-backward-text FIRST || exit 1
		wait_for_text FIRSTP1 $OUTER capture-pane -p || exit 1
		if [ "$row" = 1 ]; then
			click "$row" 'P0> first
P1> second
SECOND
P2>'
		else
			click "$row" 'P0> first
FIRSTP1> second
P2>'
		fi
		$INNER send -t test: -X cancel || exit 1
	done
done

# A tree row before the first prompt toggles the initial fold too.
initial='initial-control\r\nLEADING\r\n\033]133;A\007P> \033]133;B\007first\r\n\033]133;C\007OUTPUT\r\n\033]133;D;0\007\033]133;A\007NEXT> \033]133;B\007'
$INNER new-window -t test: -n initial \
	"printf '$initial'; exec sleep 300" || exit 1
for keys in emacs vi; do
	$INNER set -w -t test:initial mode-keys "$keys" || exit 1
	$INNER copy-mode -t test:initial -U || exit 1
	wait_for_text LEADING $OUTER capture-pane -p || exit 1
	click 2 'initial-control
P> first
OUTPUT
NEXT>'
	$INNER send -t test:initial -X cancel || exit 1
done

exit 0
