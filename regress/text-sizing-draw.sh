#!/bin/sh

# Text sizing protocol (OSC 66): drawing to the outside terminal. An inner tmux
# draws into a pane of an outer tmux, which also understands OSC 66, so the
# outer pane shows exactly what was drawn.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUTER="$TEST_TMUX -Ltest1$$ -f/dev/null"
INNER="$TEST_TMUX -Ltest2$$"
$OUTER kill-server 2>/dev/null
$INNER kill-server 2>/dev/null

TMP=$(mktemp)
CONF=$(mktemp)
DATA=$(mktemp)
trap "rm -f $TMP $CONF $DATA; $OUTER kill-server 2>/dev/null; $INNER kill-server 2>/dev/null" 0 1 15

# draw features payload expected [inner-command]
draw()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null

	printf 'set -g status off\nset -as terminal-features "*:%s"\n' "$1" >$CONF
	printf "$2" >$DATA
	$OUTER new -d -x${WIDTH:-20} -y${HEIGHT:-4} \
	    "$INNER -f$CONF new 'cat $DATA; sleep 60'" || exit 1
	sleep 2
	if [ -n "$4" ]; then
		$INNER $4 || exit 1
		sleep 1
	fi

	$OUTER capturep -pe -S0 -E1 >$TMP
	printf "$3" | cmp -s - $TMP || {
		echo "$1: output does not match:"
		od -c $TMP
		exit 1
	}
}

# At normal size without the feature.
draw 'RGB' 'ab\033]66;s=2;X\033\\cd' 'abX cd\n\n'

# By the terminal with the feature.
draw 'textsizing' 'ab\033]66;s=2;X\033\\cd' \
    'ab\033]66;w=1:s=2;X\033\\cd\n  \033[C\033[C\n'

# Still by the terminal in copy mode.
draw 'textsizing' 'ab\033]66;s=2;X\033\\cd' \
    'ab\033]66;w=1:s=2;X\033\\cd\n  \033[C\033[C\n' 'copy-mode -H'

# Only widths with the width feature.
draw 'textsizing-width' 'ab\033]66;s=2;X\033\\\033]66;w=2;Y\033\\' \
    'abX \033]66;w=2;Y\033\\\n\n'

# All the text of the widest characters at normal size.
X49=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
WIDTH=60 HEIGHT=8 draw 'RGB' "\033]66;s=7:w=7;$X49\033\\\\Y" "${X49}Y\n\n"

exit 0
