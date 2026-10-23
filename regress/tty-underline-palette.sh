#!/bin/sh

. ./regress-common.inc

# Copy-mode line numbers bypass the pane palette, but the text beside them
# must still use it even when both request the same underline colour.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
command -v python3 >/dev/null || exit 101
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

cat >"$DIR/emitter.sh" <<'EOF'
printf '\033[4;58:5:1m'
for row in 1 2 3 4 5 6 7 8; do
	printf '\033[%s;1HAAAAAAAAAAAAAAAAAAAA' "$row"
done
printf '\033]2;ready\007'
sleep 100
EOF

$INNER new-session -d -s inner -x 40 -y 8 \
    "sh '$DIR/emitter.sh'" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:RGB:usstyle' || exit 1
$INNER set-option -p 'pane-colours[1]' '#0000ff' || exit 1
$INNER set-option -w copy-mode-line-numbers absolute || exit 1
for option in copy-mode-line-number-style copy-mode-current-line-number-style; do
	$INNER set-option -w "$option" \
	    'fg=default,bg=default,underscore,us=colour1' || exit 1
done
wait_for_output ready $INNER display-message -p '#{pane_title}' || exit 1
$INNER copy-mode || exit 1

$OUTER new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "$TEST_TMUX -Li$$ -f/dev/null attach-session -t inner" || exit 1
wait_for_clients 1 $INNER || exit 1

colours()
{
	$OUTER capture-pane -pe -S 1 -E 1 -t outer:0.0 >"$DIR/capture" ||
	    return 1
	python3 -B - "$DIR/capture" <<'PY'
import re
import sys

underline = None
seen = {'number': set(), 'text': set()}
with open(sys.argv[1]) as f:
    line = f.read()
for match in re.finditer(r'\033\[([0-9;:]*)m|([0-9A])', line):
    if match[2] is not None:
        seen['text' if match[2] == 'A' else 'number'].add(underline)
        continue
    args = iter(int(n or 0) for n in match[1].replace(':', ';').split(';'))
    for value in args:
        if value in (0, 59):
            underline = None
        elif value in (38, 48, 58):
            mode = next(args)
            if mode == 2:
                colour = tuple(next(args) for _ in range(3))
            else:
                colour = next(args)
            if value == 58:
                underline = colour
print(seen['number'] == {1} and seen['text'] == {(0, 0, 255)})
PY
}

wait_for_output True colours || {
	cat "$DIR/capture" >&2
	exit 1
}

exit 0
