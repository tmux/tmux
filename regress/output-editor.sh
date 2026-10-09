#!/bin/sh

# Output and selection editors use either the default or an explicit command.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
OUT="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$OUT kill-server 2>/dev/null
	$TMUX kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup EXIT HUP INT TERM

wait_editor()
{
	i=0
	while [ "$i" -lt 50 ]; do
		modal=$($TMUX display-message -p -t edit:0 '#{window_modal_pane}')
		if [ -f "$DIR/$1" ] && [ -z "$modal" ]; then
			return 0
		fi
		sleep 0.1
		i=$((i + 1))
	done
	echo "editor did not finish: $1" >&2
	exit 1
}

cat >"$DIR/editor.sh" <<EOF || exit 1
#!/bin/sh
cp "\$2" "$DIR/\$1"
EOF

$TMUX new-session -d -s edit -x80 -y20 \
	"printf '\\033]133;A\\007p\\$ \\033]133;B\\007echo\\n\\033]133;C\\007one\\ntwo\\n\\033]133;D;0\\007\\033]133;A\\007p\\$ \\033]133;B\\007'; exec sleep 100" || exit 1
$OUT new-session -d -x80 -y20 "$TMUX attach -t edit" || exit 1
i=0
while [ "$i" -lt 50 ]; do
	[ "$($TMUX list-clients -F '#{client_name}')" ] && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || exit 1

$TMUX set-option -g editor "sh $DIR/editor.sh default" || exit 1
$TMUX set-option -g @output_editor "sh $DIR/editor.sh override" || exit 1
$TMUX set-buffer sentinel || exit 1
$TMUX copy-mode -t edit:0 || exit 1

$TMUX send-keys -t edit:0 -X open-output || exit 1
wait_editor default
[ "$(cat "$DIR/default")" = "$(printf 'one\ntwo')" ] || exit 1

$TMUX send-keys -t edit:0 -X open-output '#{@output_editor}' || exit 1
wait_editor override
[ "$(cat "$DIR/override")" = "$(printf 'one\ntwo')" ] || exit 1

$TMUX send-keys -t edit:0 -X select-output || exit 1
$TMUX send-keys -t edit:0 -X open-selection "sh $DIR/editor.sh selection" || exit 1
wait_editor selection
[ "$(cat "$DIR/selection")" = "$(printf 'one\ntwo')" ] || exit 1
[ "$($TMUX show-buffer)" = sentinel ] || exit 1
[ "$($TMUX display-message -p -t edit:0 '#{selection_present}')" = 1 ] || exit 1

$TMUX send-keys -t edit:0 -X cancel || exit 1

# Both active and stopped single-character selections use the editor binding.
printf 'o' >"$DIR/expected" || exit 1
for keys in emacs vi; do
	$TMUX set-option -w -t edit:0 mode-keys "$keys" || exit 1
	case "$keys" in
	emacs) binding=e ;;
	vi) binding=M-e ;;
	esac
	for state in active stopped; do
		name="$keys-$state"
		$TMUX set-option -g editor "sh $DIR/editor.sh $name" || exit 1
		$TMUX copy-mode -t edit:0 || exit 1
		$TMUX send-keys -t edit:0 -X search-backward one || exit 1
		$TMUX send-keys -t edit:0 -X begin-selection || exit 1
		if [ "$keys" = emacs ]; then
			$TMUX send-keys -t edit:0 -X cursor-right || exit 1
		fi
		if [ "$state" = stopped ]; then
			$TMUX send-keys -t edit:0 -X stop-selection || exit 1
		fi
		[ "$($TMUX display-message -p -t edit:0 \
		    '#{selection_present}')" = 1 ] || exit 1
		$TMUX send-keys -t edit:0 "$binding" || exit 1
		wait_editor "$name"
		cmp "$DIR/expected" "$DIR/$name" || exit 1
		[ "$($TMUX show-buffer)" = sentinel ] || exit 1
		$TMUX send-keys -t edit:0 -X cancel || exit 1
	done
done

# An empty emacs selection still opens the output.
printf 'one\ntwo\n' >"$DIR/expected" || exit 1
$TMUX set-option -w -t edit:0 mode-keys emacs || exit 1
$TMUX set-option -g editor "sh $DIR/editor.sh empty" || exit 1
$TMUX copy-mode -t edit:0 || exit 1
$TMUX send-keys -t edit:0 -X search-backward one || exit 1
$TMUX send-keys -t edit:0 -X begin-selection || exit 1
[ "$($TMUX display-message -p -t edit:0 '#{selection_present}')" = 0 ] ||
    exit 1
$TMUX send-keys -t edit:0 e || exit 1
wait_editor empty
cmp "$DIR/expected" "$DIR/empty" || exit 1
$TMUX send-keys -t edit:0 -X cancel || exit 1

exit 0
