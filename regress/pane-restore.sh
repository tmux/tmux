#!/bin/sh

# Tile removal follows normal closing rules. Restoration reuses an unchanged
# allocation or splits only the neighbour if that allocation is no longer valid.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"
trap 'stop_server $TMUX; rm -rf "$DIR"' 0
trap 'exit 1' 1 2 15

# Report a failed assertion.
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

# Capture visible tiled rectangles without focus or stacking metadata.
geometry()
{
	run list-panes -f '#{&&:#{!:#{pane_floating_flag}},#{!:#{pane_hidden_flag}}}' \
	    -F '#{pane_id}:#{pane_left},#{pane_top},#{pane_width},#{pane_height}'
}

# Capture a rectangle for comparisons between fresh test windows.
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

# Create an uneven row or column in a fresh server.
reset()
{
	stop_server $TMUX || fail 'could not stop previous server'
	A=$(run new-session -dP -F '#{pane_id}' -x 121 -y 61 'exec cat') || exit 1
	B=$(run split-window -dP -F '#{pane_id}' -"$axis" -t "$A" -l 40 '') || exit 1
	C=$(run split-window -dP -F '#{pane_id}' -"$axis" -t "$B" -l 15 '') || exit 1
}

# Select the neighbours according to the observed closing preference.
neighbours()
{
	if [ "$preference" = preceding ]; then
		neighbour=$A
		other=$C
	else
		neighbour=$C
		other=$A
	fi
}

# Remove the middle pane from the tiles using the selected operation.
leave()
{
	case $operation in
	float) run break-pane -Wd -s "$B" ;;
	hide) run resize-pane -h -t "$B" ;;
	zoom) run resize-pane -Z -t "$B" ;;
	esac
}

# Return the middle pane to its retained tiled position.
restore()
{
	case $operation in
	float) run join-pane -d -s "$B" -t "$B" ;;
	hide) run resize-pane -dH -t "$B" ;;
	zoom) run resize-pane -Z -t "$B" ;;
	esac
}

for axis in h v; do
	if [ "$axis" = h ]; then
		dimension='#{pane_width}'
		size=x
	else
		dimension='#{pane_height}'
		size=y
	fi

	# Use closing as the reference so restoration follows the same neighbour
	# preference without prescribing which sibling must be chosen.
	operation=close
	reset
	first=$(run display -p -t "$A" "$dimension")
	middle=$(run display -p -t "$B" "$dimension")
	last=$(run display -p -t "$C" "$dimension")
	run kill-pane -t "$B"
	if [ "$(run display -p -t "$A" "$dimension")" = "$first" ]; then
		preference=following
		check "$C" "$dimension" "$((last + middle + 1))"
	else
		preference=preceding
		check "$A" "$dimension" "$((first + middle + 1))"
		check "$C" "$dimension" "$last"
	fi
	closed_first=$(rectangle "$A")
	closed_last=$(rectangle "$C")

	for operation in float hide zoom; do
		# Every removal must match the normal closing behaviour.
		reset
		before=$(geometry)
		leave
		[ "$(rectangle "$A")" = "$closed_first" ] || fail 'first tile differs from closing'
		[ "$(rectangle "$C")" = "$closed_last" ] || fail 'last tile differs from closing'
		released=$(geometry)
		restore
		[ "$(geometry)" = "$before" ] || fail 'original allocation was not restored'
		leave
		restore
		[ "$(geometry)" = "$before" ] || fail 'repeated restoration changed sizes'
		run kill-pane -t "$B"
		[ "$(geometry)" = "$released" ] || fail 'removal differs from closing'

		# Changes while absent invalidate the saved allocation. Only the
		# neighbour is split; the other tile keeps its adjusted size.
		reset
		neighbours
		leave
		run resize-pane -"$size" 30 -t "$neighbour"
		untouched=$(rectangle "$other")
		restore
		check "$B" "$dimension" 15
		check "$neighbour" "$dimension" 14
		[ "$(rectangle "$other")" = "$untouched" ] || fail 'unrelated tile changed'

		# Closing the preferred neighbour makes restoration use the other.
		reset
		neighbours
		leave
		run kill-pane -t "$neighbour"
		remaining=$(run display -p -t "$other" "$dimension")
		restore
		other_size=$(run display -p -t "$other" "$dimension")
		middle=$(run display -p -t "$B" "$dimension")
		[ "$((other_size + middle + 1))" -eq "$remaining" ] ||
		    fail 'remaining neighbour was not split'
	done

	# A pane created floating can be tiled and floated without changing
	# the other tiles after the round trip.
	operation=new-float
	reset
	before=$(geometry)
	F=$(run new-pane -dP -F '#{pane_id}' -t "$B" -x 20 -y 8 '') || exit 1
	run join-pane -d -s "$F" -t "$F"
	run break-pane -Wd -s "$F"
	[ "$(geometry)" = "$before" ] || fail 'new float changed the other allocations'
done

exit 0
