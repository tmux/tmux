#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x40 -y10 \
	"printf 'zero needle\none needle\ntwo needle\n'; cat" || exit 1
$TMUX set -g window-size manual || exit 1

check_count()
{
	[ "$($TMUX display-message -p \
	    '#{search_count_current}/#{search_count}')" = "$1" ] || exit 1
}

for keys in vi emacs; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X history-top || exit 1
	$TMUX send-keys -X start-of-line || exit 1

	$TMUX send-keys -X search-forward-text needle || exit 1
	check_count 1/3
	$TMUX send-keys -X search-again || exit 1
	check_count 2/3
	$TMUX resize-window -x30 -y8 || exit 1
	check_count 2/3
	$TMUX send-keys -X search-again || exit 1
	check_count 3/3
	$TMUX send-keys -X search-again || exit 1
	check_count 1/3
	$TMUX send-keys -X search-reverse || exit 1
	if [ "$keys" = vi ]; then
		reverse_index=3
	else
		reverse_index=1
	fi
	check_count "$reverse_index/3"
	case "$($TMUX display-message -p \
	    '#{E:copy-mode-position-format}')" in
		*"($reverse_index of 3 matches)") ;;
		*) exit 1 ;;
	esac

	$TMUX send-keys -X cancel || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X history-bottom || exit 1
	$TMUX send-keys -X search-backward-text needle || exit 1
	check_count 3/3
	$TMUX send-keys -X cancel || exit 1

	# Moving onto the second match before searching again skips to the third.
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X history-top || exit 1
	$TMUX send-keys -X start-of-line || exit 1
	$TMUX send-keys -X search-forward-text needle || exit 1
	$TMUX send-keys -X cursor-down || exit 1
	$TMUX send-keys -X search-again || exit 1
	check_count 3/3
	$TMUX send-keys -X cancel || exit 1
done

$TMUX copy-mode || exit 1
$TMUX send-keys -X history-top || exit 1
$TMUX send-keys -X search-forward-text absent || exit 1
check_count /0
case "$($TMUX display-message -p '#{E:copy-mode-position-format}')" in
	*'(Phrase not found)') ;;
	*) exit 1 ;;
esac
$TMUX send-keys -X search-forward-text one || exit 1
check_count 1/1
case "$($TMUX display-message -p '#{E:copy-mode-position-format}')" in
	*'(1 of 1 match)') ;;
	*) exit 1 ;;
esac
$TMUX send-keys -X cancel || exit 1

format=$($TMUX show -gv copy-mode-position-format) || exit 1
case "$format" in
	*'#{search_count_current} of '*'#{search_count} matches'*) ;;
	*) exit 1 ;;
esac

exit 0
