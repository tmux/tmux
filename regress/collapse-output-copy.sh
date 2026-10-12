#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

$TMUX kill-server 2>/dev/null
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 3 15

$TMUX new-session -d -x20 -y10 \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007FIRST-OUTPUT\r\n\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007012345678901234567890123456789\r\nlast\r\n\033]133;D;7\007\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1

expected=$(printf '012345678901234567890123456789\nlast')
for keys in emacs vi; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode -c || exit 1
	$TMUX send -X search-backward-text second || exit 1
	$TMUX set-buffer sentinel || exit 1
	$TMUX send -X copy-output || exit 1
	[ "$($TMUX show-buffer)" = "$expected" ] || exit 1

	$TMUX send -X select-output || exit 1
	[ "$($TMUX display -p '#{selection_present}')" = 1 ] || exit 1
	$TMUX send -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = "$expected" ] || exit 1
	case "$($TMUX capture-pane -Mp)" in
	*FIRST-OUTPUT*) exit 1 ;;
	esac

	# All-output commands include the original source, even while folded.
	$TMUX send -X copy-output -a || exit 1
	all=$($TMUX show-buffer)
	case "$all" in
	*FIRST-OUTPUT*012345678901234567890123456789*last*) ;;
	*) exit 1 ;;
	esac
	$TMUX send -X select-output -a || exit 1
	$TMUX send -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = "$all" ] || exit 1
	$TMUX send -X cancel || exit 1
done

# A prompt after an output end on the same source line moves left when folded.
$TMUX new-window -n shared \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007head\r\ntail\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007SECOND-OUTPUT\r\n\033]133;D;0\007\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1
for keys in emacs vi; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode -c || exit 1
	$TMUX send -X search-backward-text P1 || exit 1
	$TMUX send -X start-of-line || exit 1
	$TMUX send -X copy-output || exit 1
	[ "$($TMUX show-buffer)" = SECOND-OUTPUT ] || exit 1
	$TMUX send -X select-output || exit 1
	$TMUX send -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = SECOND-OUTPUT ] || exit 1
	$TMUX send -X cancel || exit 1
done

exit 0
