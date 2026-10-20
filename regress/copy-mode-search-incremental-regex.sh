#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestB$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

cleanup()
{
	$TMUX kill-server 2>/dev/null
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

$TMUX new -d -x40 -y10 \
	"printf 'foo1 bar\nfoo2 baz\nplain foo\n'; cat" || exit 1
$TMUX set -g window-size manual || exit 1

check_count()
{
	[ "$($TMUX display-message -p \
	    '#{search_present} #{search_count_current}/#{search_count}')" \
	    = "$1" ] || exit 1
}

for keys in vi emacs; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X history-top || exit 1
	$TMUX send-keys -X start-of-line || exit 1

	# Like the literal incremental search, vi skips a match at the cursor
	# and emacs includes it.
	$TMUX send-keys -X search-forward-incremental-regex -- '=foo[0-9]' ||
		exit 1
	if [ "$keys" = vi ]; then
		check_count "1 2/2"
	else
		check_count "1 1/2"
	fi

	# A term without metacharacters still matches as plain text.
	$TMUX send-keys -X search-forward-incremental-regex -- '=foo' || exit 1
	check_count "1 2/3"

	$TMUX send-keys -X search-forward-incremental-regex -- '-foo[0-9]' ||
		exit 1
	check_count "1 1/2"

	# An invalid expression simply does not match.
	$TMUX send-keys -X search-forward-incremental-regex -- '=foo[' || exit 1
	check_count "0 /"

	# A valid expression searches again and search-again replays it.
	$TMUX send-keys -X search-forward-incremental-regex -- '=foo[0-9]' ||
		exit 1
	check_count "1 2/2"
	$TMUX send-keys -X search-again || exit 1
	check_count "1 1/2"

	$TMUX send-keys -X cancel || exit 1
done

for keys in vi emacs; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode || exit 1
	$TMUX send-keys -X history-top || exit 1
	$TMUX send-keys -X start-of-line || exit 1

	$TMUX send-keys -X search-backward-incremental-regex -- '=foo[0-9]' ||
		exit 1
	[ "$($TMUX display-message -p \
	    '#{search_count_current}/#{search_count} #{search_match}')" \
	    = "2/2 foo2" ] || exit 1
	$TMUX send-keys -X search-backward-incremental-regex -- '-foo[0-9]' ||
		exit 1
	[ "$($TMUX display-message -p \
	    '#{search_count_current}/#{search_count} #{search_match}')" \
	    = "1/2 foo1" ] || exit 1

	# An empty term clears the marks.
	$TMUX send-keys -X search-backward-incremental-regex -- '=' || exit 1
	check_count "0 /"

	$TMUX send-keys -X cancel || exit 1
done

$TMUX kill-server || exit 1
exit 0
