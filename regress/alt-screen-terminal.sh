#!/bin/sh

# With clear-on-attach off tmux draws on the terminal's primary screen, and a
# full-screen program in a pane that fills the window would scroll its pages
# into the terminal's scrollback. The terminal goes to its own alternate
# screen while the program is on the pane's, and comes back with its
# scrollback as it was. An outer tmux pane stands in for the terminal: its
# alternate_on says which screen it is on.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUTER="$TEST_TMUX -LtestA$$ -f/dev/null"
INNER="$TEST_TMUX -LtestB$$ -f/dev/null"
DIR=$(mktemp -d)
trap "$OUTER kill-server 2>/dev/null; $INNER kill-server 2>/dev/null; rm -rf $DIR" 0 1 15

wait_for() {
	n=0
	until eval "$1"; do
		n=$((n + 1))
		[ $n -gt "$2" ] && return 1
		sleep 0.05
	done
}

# Lines before, a full-screen program that pages through 100 lines, a line
# after; each step waits for its file and ends with a marker (OSC 7).
cat >$DIR/prog.sh <<'EOS'
step() {
	while [ ! -e "$1/go$2" ]; do sleep 0.05; done
}
step $1 1
i=0; while [ $i -lt 30 ]; do printf 'before-%02d\r\n' $i; i=$((i + 1)); done
printf '\033]7;k1\033\\'
step $1 2
printf '\033[?1049h\033[H'
i=0; while [ $i -lt 100 ]; do printf 'page-%03d\r\n' $i; i=$((i + 1)); done
printf '\033]7;k2\033\\'
step $1 3
printf '\033[?1049lafter\r\n\033]7;k3\033\\'
exec sleep 100000
EOS

at() {
	touch $DIR/go$1
	wait_for "[ \"\$($INNER display -pt inner: '#{pane_path}')\" = k$1 ]" 400 ||
	    { echo "step $1 not read"; exit 1; }
}
screen() {
	$OUTER display -pt =tmux: '#{alternate_on}'
}

$OUTER new -d -s keep \; set -g default-terminal xterm-256color \; \
    set -g status off \; set -g history-limit 1000 || exit 1
$INNER new -d -s inner -x 80 -y 24 "sh $DIR/prog.sh $DIR" \; \
    set -g status off \; set -s clear-on-attach off || exit 1
$OUTER new -d -s tmux -x 80 -y 24 "unset TMUX; exec $INNER attach -t inner" ||
    exit 1
wait_for "[ -n \"\$($INNER display -p '#{client_termtype}' 2>/dev/null)\" ]" 400 ||
    { echo "client did not attach"; exit 1; }

at 1
wait_for "$OUTER capturep -pt =tmux: | grep -q before-29" 400 ||
    { echo "lines not drawn"; exit 1; }
[ "$(screen)" = 0 ] ||
    { echo "on the alternate screen before the program"; exit 1; }

at 2
wait_for "[ \"\$(screen)\" = 1 ]" 200 ||
    { echo "the program is not on the terminal's alternate screen"; exit 1; }
wait_for "$OUTER capturep -pt =tmux: | grep -q page-099" 400 ||
    { echo "the program's screen not drawn"; exit 1; }

at 3
wait_for "[ \"\$(screen)\" = 0 ]" 200 ||
    { echo "left on the alternate screen after the program"; exit 1; }
wait_for "$OUTER capturep -pt =tmux: | grep -q after" 400 ||
    { echo "the line after not drawn"; exit 1; }
all=$($OUTER capturep -pt =tmux: -S- -E-)
echo "$all" | grep -q before-00 ||
    { echo "the scrollback lost the lines before the program"; exit 1; }
if echo "$all" | grep -q page-; then
	echo "$(echo "$all" | grep -c page-) of the program's rows in the scrollback"
	exit 1
fi
exit 0
