#!/bin/sh

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(test_tmpdir) || exit 1
TMUX="$TEST_TMUX -S$DIR/socket -f/dev/null"

cleanup()
{
	status=$?
	stop_server $TMUX || status=1
	rm -rf "$DIR"
	exit "$status"
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$command $axis $before $scope $full: $*" >&2
	exit 1
}

run()
{
	$TMUX "$@" || fail "failed: $*"
}

check_order()
{
	expected=
	index=0
	for pane in "$@"; do
		expected="$expected$index $pane
"
		index=$((index + 1))
	done
	actual=$(run list-panes -t test:1 -F '#{pane_index} #{pane_id}') ||
	    exit 1
	[ "$actual" = "${expected%?}" ] ||
	    fail "unexpected pane order: $actual"
}

test_order()
{
	A=$(run new-window -dP -F '#{pane_id}' \
	    -t test:1 'exec cat') || exit 1
	B=$(run split-window -dP -F '#{pane_id}' \
	    -"$axis" -t "$A" 'exec cat') || exit 1
	C=$(run split-window -dP -F '#{pane_id}' \
	    -"$cross" -t "$B" 'exec cat') || exit 1

	# Target a pane away from the requested edge.
	if [ -n "$before" ]; then
		src=$C; dst=$B
	else
		src=$B; dst=$A
	fi
	if [ "$command" = split-window ]; then
		src=$(run split-window -dP -F '#{pane_id}' $full $before \
		    -"$axis" -t "$dst" 'exec cat') || exit 1
	else
		if [ "$scope" = other-window ]; then
			src=$(run new-window -dP -F '#{pane_id}' \
			    -t test:2 'exec cat') || exit 1
		fi
		run "$command" -d $full $before -"$axis" \
		    -s "$src" -t "$dst"
	fi

	if [ "$command" != split-window ] && [ "$scope" = same-window ]; then
		if [ -z "$full" ]; then
			if [ -n "$before" ]; then
				check_order "$A" "$C" "$B"
			else
				check_order "$A" "$B" "$C"
			fi
		elif [ -n "$before" ]; then
			check_order "$C" "$A" "$B"
		else
			check_order "$A" "$C" "$B"
		fi
	elif [ -z "$full" ]; then
		check_order "$A" "$src" "$B" "$C"
	elif [ -n "$before" ]; then
		check_order "$src" "$A" "$B" "$C"
	else
		check_order "$A" "$B" "$C" "$src"
	fi

	# Full-size panes must also occupy the requested edge and span the window.
	if [ -n "$full" ]; then
		if [ "$axis" = h ]; then
			geometry='#{pane_left} #{pane_right} #{window_width} #{pane_height} #{window_height}'
		else
			geometry='#{pane_top} #{pane_bottom} #{window_height} #{pane_width} #{window_width}'
		fi
		values=$(run display-message -p -t "$src" "$geometry") || exit 1
		set -- $values
		[ "$4" = "$5" ] || fail 'pane does not span the window'
		if [ -n "$before" ]; then
			[ "$1" -eq 0 ] || fail 'pane is not at the first edge'
		else
			[ "$2" -eq "$(($3 - 1))" ] ||
			    fail 'pane is not at the last edge'
		fi
	fi

	run kill-window -t test:1
}

# Keep the server alive while each case creates and destroys its own windows.
run new-session -d -s test -x200 -y100 'exec cat'

for command in join-pane move-pane split-window; do
	if [ "$command" = split-window ]; then
		scopes=new-pane
	else
		scopes='same-window other-window'
	fi
	for axis in h v; do
		if [ "$axis" = h ]; then cross=v; else cross=h; fi
		for before in '' -b; do
			for scope in $scopes; do
				for full in '' -f; do
					test_order
				done
			done
		done
	done
done

exit 0
