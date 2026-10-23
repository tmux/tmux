#!/bin/sh

# Side status has distinct mouse keys and retains its default actions.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
DIR=$(mktemp -d) || exit 1

# Stop the isolated server and remove generated key lists.
cleanup()
{
	$TMUX kill-server >/dev/null 2>&1
	rm -rf "$DIR"
}
trap cleanup 0
trap 'exit 1' 1 2 15

# Report the failed check.
fail()
{
	echo "$*" >&2
	exit 1
}

$TMUX new-session -d 'sleep 180' || exit 1

# Verify every side suffix with buttons, wheels, motion and modifiers.
for suffix in '' Left Right Default Border Control0 Control1 Control2 \
    Control3 Control4 Control5 Control6 Control7 Control8 Control9; do
	for event in MouseDown1 MouseDown11 MouseUp1 MouseUp11 MouseDrag1 \
	    MouseDragEnd1 SecondClick1 DoubleClick1 TripleClick1 WheelUp \
	    WheelDown MouseMove; do
		for modifier in '' C- M- S-; do
			key=${modifier}${event}SideStatus$suffix
			printf '%s\n' "$key" >>"$DIR/expected"
			printf 'bind -T side-keys %s display-message test\n' \
			    "$key" >>"$DIR/keys.conf"
		done
	done
done
$TMUX source-file "$DIR/keys.conf" || fail 'side key parsing failed'
$TMUX list-keys -T side-keys -F '#{key_string}' >"$DIR/actual" || exit 1
LC_ALL=C sort "$DIR/expected" >"$DIR/expected.sorted"
LC_ALL=C sort "$DIR/actual" >"$DIR/actual.sorted"
cmp -s "$DIR/expected.sorted" "$DIR/actual.sorted" ||
    fail 'side key names did not round trip'

# Item clicks, menus and scrolling retain the existing status actions.
for key in MouseDown1Status C-MouseDown1Status WheelUpStatus WheelDownStatus \
    MouseDown3StatusLeft M-MouseDown3StatusLeft MouseDown3Status \
    M-MouseDown3Status; do
	side_key=$(printf '%s\n' "$key" | sed 's/Status/SideStatus/')
	horizontal=$($TMUX list-keys -T root -F '#{key_command}' \
	    "$key") || exit 1
	vertical=$($TMUX list-keys -T root -F '#{key_command}' \
	    "$side_key") || exit 1
	[ "$horizontal" = "$vertical" ] || fail "$side_key action changed"
done
for control in 7 8 9; do
	horizontal=$($TMUX list-keys -T root -F '#{key_command}' \
	    "MouseDown1Control$control") || exit 1
	vertical=$($TMUX list-keys -T root -F '#{key_command}' \
	    "MouseDown1SideStatusControl$control") || exit 1
	[ "$horizontal" = "$vertical" ] ||
	    fail "control $control action changed"
done

# Border width bindings keep their actions under their new names.
$TMUX list-keys -T root MouseDrag1SideStatusBorder >/dev/null || exit 1
$TMUX list-keys -T root DoubleClick1SideStatusBorder >/dev/null || exit 1
if $TMUX list-keys -T root MouseDrag1SideStatusDefault >/dev/null 2>&1; then
	fail 'side background resize has a default binding'
fi

exit 0
