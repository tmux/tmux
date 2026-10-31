#!/bin/sh

# Split formats into lines, preserve headers and footers while scrolling,
# and clip wide or combining characters at the side status boundary.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1
HEIGHT=8

cleanup()
{
	$TMUX2 kill-server >/dev/null 2>&1
	$TMUX kill-server >/dev/null 2>&1
	rm -f "$DIR/actual" "$DIR/expected"
	rmdir "$DIR"
}
trap cleanup 0 1 15

check()
{
	format=$1
	shift
	$TMUX set -g side-status-format "$format" || exit 1
	: >"$DIR/expected"
	n=0
	for line in "$@"; do
		printf '%s\n' "$line" >>"$DIR/expected"
		n=$((n + 1))
	done
	while [ "$n" -lt "$HEIGHT" ]; do
		printf '       │\n' >>"$DIR/expected"
		n=$((n + 1))
	done
	i=0
	while [ "$i" -lt 50 ]; do
		$TMUX2 capture-pane -p -t outer:0.0 >"$DIR/actual" || exit 1
		cmp -s "$DIR/actual" "$DIR/expected" && return
		sleep 0.1
		i=$((i + 1))
	done
	echo "format '$format' at height $HEIGHT differs:" >&2
	diff -u "$DIR/expected" "$DIR/actual" >&2
	exit 1
}

resize()
{
	HEIGHT=$1
	$TMUX2 resize-window -t outer:0 -y "$HEIGHT" || exit 1
}

$TMUX new-session -d -s inner -x 40 -y 8 'sleep 100' || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g side-status left || exit 1
$TMUX set -g side-status-width 8 || exit 1
$TMUX2 new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$TMUX2 set -g status off || exit 1
$TMUX2 set -g window-size manual || exit 1
$TMUX2 respawn-pane -k -t outer:0.0 "$TMUX attach -t inner" || exit 1
sleep 1

# Empty, consecutive, leading and trailing breaks, with both spellings.
check ''
check 'one#[nl]#[nl]three#[nl]' 'one    │' '       │' 'three  │'
check '

three
' '       │' '       │' 'three  │'
check 'one
two' 'one    │' 'two    │'

# Escaped hashes must not turn literal text into a line break.
check '##[nl]#[nl]two' '#[nl]  │' 'two    │'
check '####[nl]#[nl]two' '##[nl] │' 'two    │'
check '###[nl]two' '#      │' 'two    │'
check 'one##' 'one#   │'

# Invalid or unfinished styles do not consume the following valid lines.
check 'one#[invalid-style]two#[nl]three' 'onetwo │' 'three  │'
check 'one#[unfinished' 'one    │'
check '#[ignore]A#[nl]B
C' 'A#[nl]B│' 'C      │'

# Alignment carries over a break, as do character widths.
check '#[align=centre]X#[nl]Y' '   X   │' '   Y   │'
check '#[align=absolute-centre]X#[nl]Y' '   X   │' '   Y   │'
check 'abcde界#[nl]abcdef界#[nl]abcdeéf' \
    'abcde界│' 'abcdef │' 'abcdeéf│'

format='H1#[nl]H2#[nl]#[list=on]L0#[nl]L1#[nl]'
format=$format'#[list=focus]L2#[nl]#[list=on]L3#[nl]L4#[nl]'
format=$format'#[nolist]F1#[nl]F2'
check "$format" 'H1     │' 'H2     │' 'L0     │' 'L1     │' \
    'L2     │' 'L3     │' 'F1     │' 'F2     │'
resize 6
check "$format" 'H1     │' 'H2     │' 'L1     │' 'L2     │' \
    'F1     │' 'F2     │'
resize 4
check "$format" 'H1     │' 'H2     │' 'F1     │' 'F2     │'
resize 3
check "$format" 'H1     │' 'H2     │' 'F1     │'
resize 1
check "$format" 'H1     │'

# Markers consume only list lines and cannot replace the focused line.
format='H#[nl]#[list=on]#[list=left-marker]^#[nl]'
format=$format'#[list=right-marker]v#[nl]#[list=on]L0#[nl]L1#[nl]'
format=$format'#[list=focus]L2#[nl]#[list=on]L3#[nl]L4#[nl]'
format=$format'#[nolist]F'
resize 5
check "$format" 'H      │' '^      │' 'L2     │' 'v      │' 'F      │'
resize 3
check "$format" 'H      │' 'L2     │' 'F      │'
resize 2
check "$format" 'H      │' 'F      │'

# A large logical list does not require a tall terminal.
format=$(awk 'BEGIN {
	printf "H#[nl]#[list=on]"
	for (i = 0; i < 1000; i++) {
		if (i == 800)
			printf "#[list=focus]"
		if (i == 801)
			printf "#[list=on]"
		printf "L%d#[nl]", i
	}
	printf "#[nolist]F"
}')
resize 3
check "$format" 'H      │' 'L800   │' 'F      │'

exit 0
