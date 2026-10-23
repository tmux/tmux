#!/bin/sh

. ./regress-common.inc

# Each pane, popup and status line has its own hyperlinks, numbered by the
# same counter. When one is drawn after another, each must keep its own URI.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR

INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"

cleanup()
{
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

# Fill each line of a pane with one character linked to one URI.
cat >"$DIR/emitter.sh" <<'EOF'
printf '\033]8;;%s\033\134' "$1"
for i in 1 2 3 4 5 6 7 8 9 10 11; do
	printf '\033[%u;1H%s' $i "$2"
done
printf '\033]8;;\033\134\033[1;1H'
touch "$3"
sleep 100
EOF
EMITTER="sh '$DIR/emitter.sh'"

# Print each character of a captured line with the URI it is linked to.
links()
{
	$OUTER capture-pane -p -e -S $1 -E $1 -t outer:0.0 | perl -ne '
		my (%seen, $uri);
		while (/\e\]8;[^;]*;([^\e]*)\e\\|([A-Z])/g) {
			if (defined $1) {
				$uri = $1;
			} else {
				$seen{"$2=$uri"} = 1;
			}
		}
		print join(" ", sort keys %seen), "\n";'
}

$INNER new-session -d -s inner -x 41 -y 12 \
    "$EMITTER https://example.com/a AAAAAAAAAAAAAAAAAAAA '$DIR/a'" || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:hyperlinks' || exit 1
$INNER set-option -g status-style default || exit 1
$INNER set-option -g status-format[0] \
    "#[link=https://example.com/s]SSSSSSSSSSSSSSSSSSSS" || exit 1
$INNER split-window -h -l 20 -t inner:0 \
    "$EMITTER https://example.com/b BBBBBBBBBBBBBBBBBBBB '$DIR/b'" || exit 1

$OUTER new-session -d -s outer -x 41 -y 12 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" || exit 1

wait_for_clients 1 $INNER || exit 1
wait_until test -e "$DIR/a" || exit 1
wait_until test -e "$DIR/b" || exit 1

# Draw both panes and the status line again, one after another.
$INNER refresh-client || exit 1
wait_for_output "A=https://example.com/a B=https://example.com/b" links 5 || {
	links 5 >&2
	exit 1
}
wait_for_output "S=https://example.com/s" links 11 || {
	links 11 >&2
	exit 1
}

# Then a popup over part of a pane in a new window, drawn again with the pane.
c=CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC
$INNER new-window -t inner: "$EMITTER https://example.com/c $c '$DIR/c'" ||
    exit 1
wait_until test -e "$DIR/c" || exit 1
$INNER display-popup -B -x 0 -y 0 -w 10 -h 8 \
    "$EMITTER https://example.com/p PPPPPPPPPP '$DIR/p'" &
wait_until test -e "$DIR/p" || exit 1
$INNER refresh-client || exit 1
wait_for_output "C=https://example.com/c P=https://example.com/p" links 3 || {
	links 3 >&2
	exit 1
}

exit 0
