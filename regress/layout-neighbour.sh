#!/bin/sh

# Closing and floating prefer the preceding tiled sibling, falling forward
# only when necessary. Preserve unrelated allocations in custom layouts.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
trap 'stop_server $TMUX; rm -rf "$DIR"' 0
trap 'exit 1' 1 2 15

# Report a failed assertion with the active test case.
fail()
{
	echo "$axis $operation: $*" >&2
	exit 1
}

# Run a command against the isolated server.
run()
{
	$TMUX "$@" || fail "failed: $*"
}

# Capture tiled rectangles without focus or stacking metadata.
geometry()
{
	run list-panes -f '#{!:#{pane_floating_flag}}' \
	    -F '#{pane_id}:#{pane_left},#{pane_top},#{pane_width},#{pane_height}'
}

# Capture one pane's rectangle, including floating panes.
rectangle()
{
	run display -p -t "$1" \
	    '#{pane_left},#{pane_top},#{pane_width},#{pane_height}'
}

# Compare a pane format with its expected value.
check()
{
	got=$(run display -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# Start a fresh window whose panes remain alive until explicitly removed.
reset()
{
	stop_server $TMUX || fail 'could not stop previous server'
	A=$(run new-session -dP -F '#{pane_id}' -x 121 -y 81 'exec cat') || exit 1
}

# Create three deliberately unequal sibling allocations.
three_panes()
{
	reset
	B=$(run split-window -dP -F '#{pane_id}' -"$axis" -t "$A" -l 40 'exec cat') || exit 1
	C=$(run split-window -dP -F '#{pane_id}' -"$axis" -t "$B" -l 15 'exec cat') || exit 1
}

# Remove a tile through the command, process-exit, or floating path.
remove()
{
	case $operation in
	close) run kill-pane -t "$1" ;;
	exit) run send-keys -t "$1" C-d ;;
	float) run break-pane -Wd -s "$1" ;;
	esac
}

for axis in h v; do
	if [ "$axis" = h ]; then
		cross=v
		dimension='#{pane_width}'
		cross_geometry='#{pane_top},#{pane_height}'
		other_size=60
		window_size=121
	else
		cross=h
		dimension='#{pane_height}'
		cross_geometry='#{pane_left},#{pane_width}'
		other_size=40
		window_size=81
	fi
	for operation in close exit float; do
		# A temporary split must return space to the pane it split.
		reset
		C=$(run split-window -dP -F '#{pane_id}' -"$axis" \
		    -t "$A" -l "$other_size" 'exec cat') || exit 1
		before=$(geometry)
		B=$(run split-window -dP -F '#{pane_id}' -"$axis" -t "$A" 'exec cat') || exit 1
		remove "$B"
		wait_for_output "$before" geometry ||
		    fail 'temporary split changed the original layout'
		if [ "$operation" = float ]; then
			# Retiling must split the same preceding neighbour, preserving C.
			other=$(rectangle "$C")
			run join-pane -d -s "$B" -t "$B"
			[ "$(rectangle "$C")" = "$other" ] || fail 'retiling resized another tile'
			run break-pane -Wd -s "$B"
			[ "$(geometry)" = "$before" ] || fail 'repeated floating changed layout'
		fi

		for position in first middle last; do
			three_panes
			case $position in
			first) target=$A; recipient=$B; untouched=$C ;;
			middle) target=$B; recipient=$A; untouched=$C ;;
			last) target=$C; recipient=$B; untouched=$A ;;
			esac
			old=$(run display -p -t "$recipient" "$dimension")
			space=$(run display -p -t "$target" "$dimension")
			other=$(rectangle "$untouched")
			remove "$target"
			wait_for_output "$((old + space + 1))" \
			    $TMUX display -p -t "$recipient" "$dimension" ||
			    fail "$position: wrong recipient for released space"
			[ "$(rectangle "$untouched")" = "$other" ] ||
			    fail "$position: unrelated allocation changed"
		done

		# Skip a floating leaf between the departed tile and its recipient.
		three_panes
		F=$(run new-pane -dP -F '#{pane_id}' -t "$A" -x 20 -y 8 'exec cat') || exit 1
		floating=$(rectangle "$F")
		other=$(rectangle "$C")
		old=$(run display -p -t "$A" "$dimension")
		space=$(run display -p -t "$B" "$dimension")
		remove "$B"
		wait_for_output "$((old + space + 1))" \
		    $TMUX display -p -t "$A" "$dimension" ||
		    fail 'floating sibling blocked the preceding tile'
		[ "$(rectangle "$F")" = "$floating" ] || fail 'floating geometry changed'
		[ "$(rectangle "$C")" = "$other" ] || fail 'following allocation changed'

		# Loaded nested layouts retain their internal split and unrelated tiles.
		three_panes
		D=$(run split-window -dP -F '#{pane_id}' -"$cross" -t "$A" -l 17 'exec cat') || exit 1
		saved=$(run display -p '#{window_layout}')
		run select-layout "$saved"
		first_cross=$(run display -p -t "$A" "$cross_geometry")
		second_cross=$(run display -p -t "$D" "$cross_geometry")
		other=$(rectangle "$C")
		old=$(run display -p -t "$A" "$dimension")
		space=$(run display -p -t "$B" "$dimension")
		remove "$B"
		wait_for_output "$((old + space + 1))" \
		    $TMUX display -p -t "$A" "$dimension" ||
		    fail 'preceding nested layout did not absorb space'
		check "$D" "$dimension" "$((old + space + 1))"
		check "$A" "$cross_geometry" "$first_cross"
		check "$D" "$cross_geometry" "$second_cross"
		[ "$(rectangle "$C")" = "$other" ] || fail 'loaded layout changed unrelated tile'

		# A preceding subtree containing only floats is skipped, so the
		# following tile absorbs space when no preceding tile remains.
		three_panes
		D=$(run split-window -dP -F '#{pane_id}' -"$cross" -t "$A" 'exec cat') || exit 1
		run break-pane -Wd -s "$A"
		run break-pane -Wd -s "$D"
		first_float=$(rectangle "$A")
		second_float=$(rectangle "$D")
		remove "$B"
		wait_for_output "$window_size" \
		    $TMUX display -p -t "$C" "$dimension" ||
		    fail 'following tile did not absorb the collapsed group'
		[ "$(rectangle "$A")" = "$first_float" ] || fail 'first float moved'
		[ "$(rectangle "$D")" = "$second_float" ] || fail 'second float moved'
	done
done

exit 0
