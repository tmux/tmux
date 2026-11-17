#!/bin/sh

# Closing, floating and retiling prefer the preceding tiled sibling, falling
# forward only when necessary. Floating leaves and floating-only subtrees
# must be skipped, and unrelated allocations must remain unchanged.

. ./regress-common.inc

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM
pane_geometry='#{pane_left},#{pane_top},#{pane_width},#{pane_height}'

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
	echo "$axis $operation $test_case: $*" >&2
	exit 1
}

run()
{
	$TMUX "$@" || fail "failed: $*"
}

# Capture tiled rectangles without focus or stacking metadata.
geometry()
{
	run list-panes -f '#{!:#{pane_floating_flag}}' \
	    -F "#{pane_id}:$pane_geometry"
}

# Capture selected panes, including their tiled/floating state.
rectangles()
{
	for pane in "$@"; do
		run display -p -t "$pane" \
		    "#{pane_id}:$pane_geometry:#{pane_floating_flag}" ||
		    exit 1
	done
}

check()
{
	got=$(run display -p -t "$1" "$2") || exit 1
	[ "$got" = "$3" ] || fail "$1 $2: got '$got', expected '$3'"
}

# Keep panes alive until explicitly removed. Each case gets a fresh server.
reset()
{
	stop_server $TMUX || fail 'could not stop previous server'
	A=$(run new-session -dP -F '#{pane_id}' -x 121 -y 81 \
	    'exec cat') || exit 1
}

split()
{
	run split-window -dP -F '#{pane_id}' -"$2" -t "$1" -l "$3" \
	    'exec cat'
}

new_float()
{
	run new-pane -dP -F '#{pane_id}' -t "$1" -x20 -y8 -X10 -Y5 \
	    'exec cat'
}

# Create three deliberately unequal sibling allocations.
three_panes()
{
	reset
	B=$(split "$A" "$axis" 40) || exit 1
	C=$(split "$B" "$axis" 15) || exit 1
	floating_panes=
}

# Put an excluded sibling after every tile, including after the last one.
add_neighbours()
{
	for tiled in "$A" "$B" "$C"; do
		case $neighbours in
		none) ;;
		leaves)
			F=$(new_float "$tiled") || exit 1
			floating_panes="$floating_panes $F"
			;;
		subtrees)
			F=$(split "$tiled" "$axis" 6) || exit 1
			G=$(split "$F" "$cross" 3) || exit 1
			run break-pane -Wd -s "$F"
			run break-pane -Wd -s "$G"
			floating_panes="$floating_panes $F $G"
			;;
		esac
	done
}

remove()
{
	case $operation in
	close) run kill-pane -t "$1" ;;
	exit)
		run send-keys -t "$1" C-d
		wait_for_output '' $TMUX list-panes -a \
		    -f "#{==:#{pane_id},$1}" -F '#{pane_id}' ||
		    fail "$1 did not exit"
		;;
	float)
		run break-pane -Wd -s "$1"
		check "$1" '#{pane_floating_flag}' 1
		;;
	esac
}

# Check the recipient's entire rectangle and all explicitly preserved panes.
release()
{
	release_target=$1
	release_recipient=$2
	shift 2
	preserved_panes="$*"
	preserved=$(rectangles "$@") || exit 1
	values=$(run display -p -t "$release_recipient" "$allocation") ||
	    exit 1
	set -- $values
	recipient_size=$1
	recipient_offset=$2
	recipient_cross="$3 $4"
	values=$(run display -p -t "$release_target" "$dimension $offset") ||
	    exit 1
	set -- $values
	merged_size=$((recipient_size + $1 + 1))
	merged_offset=$recipient_offset
	[ "$2" -lt "$merged_offset" ] && merged_offset=$2
	remove "$release_target"
	check "$release_recipient" "$allocation" \
	    "$merged_size $merged_offset $recipient_cross"
	after=$(rectangles $preserved_panes) || exit 1
	[ "$after" = "$preserved" ] || fail 'unrelated allocation changed'
}

# Retiling must split the chosen donor approximately in half, fill its old
# allocation, and restore the target's original cross-axis allocation.
retile()
{
	tile_target=$1
	tile_donor=$2
	tile_side=$3
	shift 3
	preserved_panes="$*"
	preserved=$(rectangles "$@") || exit 1
	values=$(run display -p -t "$tile_donor" "$allocation") || exit 1
	set -- $values
	donor_size=$1
	donor_offset=$2
	donor_cross="$3 $4"
	run join-pane -d -s "$tile_target" -t "$tile_target"
	check "$tile_target" '#{pane_floating_flag}' 0
	new_size=$(run display -p -t "$tile_target" "$dimension") || exit 1
	remaining=$(run display -p -t "$tile_donor" "$dimension") || exit 1
	[ "$((new_size + remaining + 1))" -eq "$donor_size" ] ||
	    fail 'retiled pane did not share the donor allocation'
	difference=$((new_size - remaining))
	[ "$difference" -ge -1 ] && [ "$difference" -le 1 ] ||
	    fail 'retiled allocation was not split in half'
	if [ "$tile_side" = before ]; then
		new_offset=$donor_offset
		donor_offset=$((donor_offset + new_size + 1))
	else
		new_offset=$((donor_offset + remaining + 1))
	fi
	check "$tile_target" "$allocation" "$new_size $new_offset $tile_cross"
	check "$tile_donor" "$allocation" \
	    "$remaining $donor_offset $donor_cross"
	after=$(rectangles $preserved_panes) || exit 1
	[ "$after" = "$preserved" ] || fail 'retiling changed unrelated allocation'
}

for axis in h v; do
	if [ "$axis" = h ]; then
		cross=v
		dimension='#{pane_width}'
		offset='#{pane_left}'
		cross_geometry='#{pane_height} #{pane_top}'
		other_size=60
	else
		cross=h
		dimension='#{pane_height}'
		offset='#{pane_top}'
		cross_geometry='#{pane_width} #{pane_left}'
		other_size=40
	fi
	allocation="$dimension $offset $cross_geometry"
	for operation in close exit float; do
		test_case=temporary-split
		reset
		C=$(split "$A" "$axis" "$other_size") || exit 1
		before=$(geometry) || exit 1
		B=$(run split-window -dP -F '#{pane_id}' -"$axis" \
		    -t "$A" 'exec cat') || exit 1
		remove "$B"
		after=$(geometry) || exit 1
		[ "$after" = "$before" ] || fail 'temporary split changed layout'

		for neighbours in none leaves subtrees; do
			for position in first middle last; do
				test_case="$position-$neighbours"
				three_panes
				add_neighbours
				case $position in
				first) target=$A; recipient=$B; untouched=$C ;;
				middle) target=$B; recipient=$A; untouched=$C ;;
				last) target=$C; recipient=$B; untouched=$A ;;
				esac
				release "$target" "$recipient" "$untouched" $floating_panes
			done
		done

		# A parsed nested sibling can receive space in either direction.
		# Its floating child keeps its geometry and tiled children their split.
		for direction in previous next; do
			test_case="nested-$direction"
			three_panes
			if [ "$direction" = previous ]; then
				target=$B; recipient=$A
			else
				target=$A; recipient=$B
			fi
			D=$(split "$recipient" "$cross" 17) || exit 1
			E=$(new_float "$recipient") || exit 1
			saved=$(run display -p '#{window_layout}') || exit 1
			run select-layout "$saved"
			other_cross=$(run display -p -t "$D" "$cross_geometry") ||
			    exit 1
			release "$target" "$recipient" "$C" "$E"
			shared=$(run display -p -t "$recipient" "$dimension $offset") ||
			    exit 1
			check "$D" "$allocation" "$shared $other_cross"
		done

		# Removing the last tile in a subtree releases the parent's space.
		test_case=recursive-release
		three_panes
		D=$(split "$B" "$cross" 17) || exit 1
		run break-pane -Wd -s "$D"
		release "$B" "$A" "$C" "$D"

		# With no tiled sibling, the floating panes keep the window alive.
		test_case=last-tile
		three_panes
		run break-pane -Wd -s "$A"
		run break-pane -Wd -s "$C"
		before=$(rectangles "$A" "$C") || exit 1
		remove "$B"
		after=$(geometry) || exit 1
		[ -z "$after" ] || fail 'last tile remained in tiled layout'
		after=$(rectangles "$A" "$C") || exit 1
		[ "$after" = "$before" ] || fail 'last tile changed floating panes'
	done

	# Destroying an already-floating cell must release no tiled space.
	for operation in close exit; do
		test_case=remove-floating
		three_panes
		run break-pane -Wd -s "$B"
		before=$(geometry) || exit 1
		remove "$B"
		after=$(geometry) || exit 1
		[ "$after" = "$before" ] || fail 'floating pane released tiled space'
	done

	operation=retile
	for neighbours in none leaves subtrees; do
		for position in first middle last; do
			test_case="$position-$neighbours"
			three_panes
			add_neighbours
			case $position in
			first) target=$A; donor=$B; side=before; untouched=$C ;;
			middle) target=$B; donor=$A; side=after; untouched=$C ;;
			last) target=$C; donor=$B; side=after; untouched=$A ;;
			esac
			tile_cross=$(run display -p -t "$target" "$cross_geometry") ||
			    exit 1
			run break-pane -Wd -s "$target"
			retile "$target" "$donor" "$side" "$untouched" $floating_panes
		done
	done

	for direction in previous next; do
		test_case="nested-$direction"
		three_panes
		if [ "$direction" = previous ]; then
			target=$B; donor=$A; side=after
		else
			target=$A; donor=$B; side=before
		fi
		D=$(split "$donor" "$cross" 17) || exit 1
		E=$(new_float "$donor") || exit 1
		other_cross=$(run display -p -t "$D" "$cross_geometry") || exit 1
		tile_cross=$(run display -p -t "$target" "$cross_geometry") || exit 1
		run break-pane -Wd -s "$target"
		retile "$target" "$donor" "$side" "$C" "$E"
		shared=$(run display -p -t "$donor" "$dimension $offset") || exit 1
		check "$D" "$allocation" "$shared $other_cross"
	done

	# Retiling the first tile in a subtree must also retile its parent.
	test_case=recursive-insert
	three_panes
	D=$(split "$B" "$cross" 17) || exit 1
	run break-pane -Wd -s "$D"
	tile_cross=$(run display -p -t "$B" "$cross_geometry") || exit 1
	run break-pane -Wd -s "$B"
	retile "$B" "$A" after "$C" "$D"

	# There may be no tiled neighbour anywhere, or no parent at all.
	for tree in root nested; do
		test_case="only-tile-$tree"
		reset
		full=$(run display -p -t "$A" "$allocation") || exit 1
		target=$A
		if [ "$tree" = nested ]; then
			B=$(split "$A" "$axis" 40) || exit 1
			C=$(split "$B" "$cross" 17) || exit 1
			run break-pane -Wd -s "$A"
			run break-pane -Wd -s "$C"
			target=$B
		fi
		run break-pane -Wd -s "$target"
		before=$(geometry) || exit 1
		[ -z "$before" ] || fail 'floating left a tiled pane'
		if [ "$tree" = nested ]; then
			before=$(rectangles "$A" "$C") || exit 1
		fi
		run join-pane -d -s "$target" -t "$target"
		check "$target" '#{pane_floating_flag}' 0
		check "$target" "$allocation" "$full"
		if [ "$tree" = nested ]; then
			after=$(rectangles "$A" "$C") || exit 1
			[ "$after" = "$before" ] || fail 'retiling moved floating panes'
		fi
	done
done

exit 0
