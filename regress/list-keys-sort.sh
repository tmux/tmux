#!/bin/sh

# list-keys -O key orders by the whole key, including the modifiers

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestLKS$$ -f/dev/null"
$TMUX kill-server 2>/dev/null
trap "$TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d

keys()
{
	$TMUX list-keys -T prefix -O key | sed \
	    's/^bind-key *//; s/^-r *//; s/^-N "[^"]*" *//; s/^-T prefix *//' |
	    awk '{print $1}'
}

all=$(keys|wc -l)
plain=$(keys|grep -cvE '^(C|M|S)-')
first=$(keys|grep -nE '^(C|M|S)-'|head -1|cut -d: -f1)

# The modifiers sit above the base key, so every modified key sorts after
# every unmodified one.
[ "$all" -gt 0 ] || exit 1
[ "$plain" -gt 0 ] || exit 1
[ "$plain" -lt "$all" ] || exit 1
[ "$first" = "$((plain + 1))" ] || exit 1

exit 0
