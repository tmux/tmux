#!/bin/sh

# Text sizing protocol (OSC 66): cursor movement, overwriting, erasing and
# capture.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
trap "rm -f $TMP; $TMUX kill-server 2>/dev/null" 0 1 15

$TMUX new -d -x20 -y6 "sleep 60" || exit 1

# check payload cursor lines expected [option]
check()
{
	$TMUX set -g text-sizing "${5:-on}"
	$TMUX respawnp -k "printf '$1'; sleep 60"
	sleep 0.5

	cursor=$($TMUX display -p '#{cursor_x},#{cursor_y}')
	if [ "$cursor" != "$2" ]; then
		echo "$1: cursor is $cursor, expected $2"
		exit 1
	fi

	$TMUX capturep -p -S${3%-*} -E${3#*-} | tr '\n' '|' >$TMP
	if [ "$(cat $TMP)" != "$(printf "$4")" ]; then
		echo "$1: capture is \"$(cat $TMP)\", expected \"$4\""
		exit 1
	fi
}

# Cursor movement and the support check from the specification.
check '\033]66;w=2; \007' 2,0 0-1 '||'
check '\033]66;s=2; \007' 2,0 0-1 '||'
check '\033]66;w=2; \007\033]66;s=2; \007' 4,0 0-1 '||'
check '\033]66;w=2; \007\033]66;s=2; \007' 3,0 0-1 '||' width
check '\033]66;w=2; \007\033]66;s=2; \007' 0,0 0-1 '||' off
check 'ab\033]66;s=2;XY\033\\cd' 8,0 0-1 'abX Y cd||'
check 'ab\033]66;s=2:w=3;Hello\033\\cd' 10,0 0-1 'abHello cd||'
check '\033]66;s=2;e\314\201\007' 2,0 0-1 'e\314\201||'
check '\033]66;s=7:w=7;A\007' 0,0 0-1 '||'
check '\033]66;s=2:x=1;A\007' 2,0 0-1 'A||'
check '\033]66;s=8;A\007' 0,0 0-1 '||'

# Text: long, wider than the block, invalid UTF-8.
X10=xxxxxxxxxx
check "\033]66;w=7;$X10$X10$X10$X10\007" 7,0 0-0 "$X10$X10$X10$X10|"
check '\033]66;w=1:n=1:d=2;ab\007' 1,0 0-0 'ab|'
check "\033]66;w=7:n=1:d=15;$X10$X10$X10$X10$X10$X10\007" 7,0 0-0 \
    "$X10$X10$X10$X10$X10$X10|"
X60="\033]66;w=7:n=1:d=15;$X10$X10$X10$X10$X10$X10\007\r"
check "$X60$X60$X60" 0,0 0-0 "$X10$X10$X10$X10$X10$X10|"
check '\033]66;w=2;\360\237\221\251\007\033]66;w=2;AB\007' 4,0 0-0 \
    '\360\237\221\251AB|'
check '\033]66;w=5;\302AB\377C\007' 5,0 0-0 '\357\277\275AB\357\277\275C|'

# Wrapping and scrolling.
check '\033[1;20H\033]66;s=2;A\007' 2,1 0-2 '|A||'
check '\033[?7l\033[1;20H\033]66;s=2;A\007' 19,0 0-1 '                  A||'
check '\033[6;1H\033]66;s=3;A\007' 3,3 3-4 'A||'

# Overwriting: top-left cell, top row, lower row.
check '\033]66;s=2;AB\007\033[1;1Hq' 1,0 0-1 'q B||'
check '\033]66;s=2;AB\007\033[1;2Hq' 2,0 0-1 ' qB||'
check '\033]66;s=2;AB\007\033[2;1Hzz' 6,1 0-1 'A B|    zz|'
check '\033]66;s=2;ABCDEFGHIJ\007q' 1,2 0-2 'A B C D E F G H I J||q|'
check '\033]66;s=2;A\007\033[2;1H\033]66;s=2;B\007' 4,1 0-2 'A|  B||'

# Inserting and deleting.
check 'abc\033]66;w=2;Z\007\033[H\033[@' 0,0 0-0 ' abcZ|'
check 'abc\033]66;w=2;Z\007\033[H\033[P' 0,0 0-0 'bcZ|'
check 'abc\033[H\033[4h\033]66;s=2;Z\007' 2,0 0-1 'Z abc||'

# Erasing.
check '\033]66;s=2;AB\007\033[2;1H\033[K' 0,1 0-1 '||'
check '\033]66;s=2;AB\007\033[1;2H\033[X' 1,0 0-1 '  B||'
check '\033]66;s=2;AB\007\033[2;1H\033[L' 0,1 0-2 '|||'
check '\033]66;s=2;AB\007\033[1;4H\033[P' 3,0 0-1 'A||'

# Capture with escape sequences.
$TMUX respawnp -k "printf 'ab\033]66;s=2;X\033\\\\'; sleep 60"
sleep 0.5
$TMUX capturep -pe -S0 -E1 >$TMP
printf 'ab\033]66;w=1:s=2;X\033\\\n  \033[C\033[C\n' | cmp -s - $TMP || {
	echo "capture -e does not match"
	exit 1
}

# Capture is not padded when all the text fills the width.
X49=$X10$X10$X10$X10${X10%x}
$TMUX resizew -x60 -y10
$TMUX respawnp -k "printf '\033]66;s=7:w=7;${X49}\033\\\\Y'; sleep 60"
sleep 0.5
if [ "$($TMUX capturep -p -S0 -E0)" != "${X49}Y" ]; then
	echo "capture is \"$($TMUX capturep -p -S0 -E0)\", expected \"${X49}Y\""
	exit 1
fi
$TMUX resizew -x20 -y6

# Copy mode moves over the lines after the first and does not copy them.
$TMUX respawnp -k "printf 'top\n\033]66;s=2;AB\033\\\\ cd\n\nend\n'; sleep 60"
sleep 0.5
$TMUX clear-history
$TMUX set -g mode-keys vi
$TMUX copy-mode
$TMUX send -X history-top
cursor=
for i in 1 2 3; do
	$TMUX send -X cursor-down
	cursor="$cursor $($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')"
done
for i in 1 2; do
	$TMUX send -X cursor-up
	cursor="$cursor $($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')"
done
if [ "$cursor" != " 0,1 0,3 0,4 0,3 0,1" ]; then
	echo "copy mode cursor is$cursor"
	exit 1
fi
$TMUX send -X begin-selection
$TMUX send -X cursor-down
$TMUX send -X copy-selection-and-cancel
if [ "$($TMUX show-buffer | tr '\n' '|')" != "AB cd|e" ]; then
	echo "copy mode buffer is \"$($TMUX show-buffer)\""
	exit 1
fi

# Copy mode moves across each character, on the first and the other lines.
$TMUX respawnp -k "printf 'ab\033]66;s=3;XY\033\\\\\n\nabcd\n'; sleep 60"
sleep 0.5
$TMUX clear-history
$TMUX copy-mode
$TMUX send -X history-top
cursor=
for i in right right right left left down down left right right right right; do
	$TMUX send -X cursor-$i
	cursor="$cursor $($TMUX display -p '#{copy_cursor_x},#{copy_cursor_y}')"
done
if [ "$cursor" != " 1,0 2,0 5,0 2,0 1,0 1,1 1,2 0,2 1,2 2,2 5,2 8,2" ]; then
	echo "copy mode cursor is$cursor"
	exit 1
fi
$TMUX send -X cursor-left
$TMUX send -X cursor-left
$TMUX send -X begin-selection
$TMUX send -X cursor-right
$TMUX send -X cursor-right
$TMUX send -X copy-selection-and-cancel
if [ "$($TMUX show-buffer)" != "XYc" ]; then
	echo "copy mode buffer is \"$($TMUX show-buffer)\""
	exit 1
fi
$TMUX copy-mode
$TMUX send -X history-top
$TMUX send -X cursor-right
$TMUX send -X cursor-right
$TMUX send -X cursor-right
$TMUX send -X begin-selection
$TMUX send -X cursor-down
$TMUX send -X cursor-down
$TMUX send -X copy-selection-and-cancel
if [ "$($TMUX show-buffer)" != "$(printf 'Y\nabcd')" ]; then
	echo "copy mode buffer is \"$($TMUX show-buffer)\""
	exit 1
fi

# Copying from the lines after the first copies the character once.
$TMUX respawnp -k "printf '\033]66;s=3;A\033\\\\\n'; sleep 60"
sleep 0.5
$TMUX clear-history
$TMUX copy-mode
$TMUX send -X begin-selection
$TMUX send -X cursor-down
$TMUX send -X copy-selection-and-cancel
if [ "$($TMUX show-buffer)" != "A" ]; then
	echo "copy mode buffer is \"$($TMUX show-buffer)\""
	exit 1
fi

exit 0
