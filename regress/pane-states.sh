#!/bin/sh

# Combinations of tiled, floating, zoomed and hidden panes.
#
# Every pane is tiled or floating, zoomed or not, and hidden or not, and these
# are independent. For each combination of those states for two panes (and for
# three), each pane command that changes or depends on them is run and the
# result is compared with what the command should do, using awk to work out the
# expectation from the state before. This covers:
# - select-pane, including -Z and moving to the next, last or a directional pane;
# - resize-pane -Z (toggle a zoom), -H (hide or show), -a -Z (unzoom all) and
#   -a -H (show desktop), which is run once, twice and three times to check
#   the hide, floats and zoomed panes steps;
# - break-pane -W and join-pane to float and tile panes;
# - split-window, new-pane and kill-pane;
# - next-layout, select-layout, rotate-window and resize-window.
#
# The tiled panes are laid out in different shapes: split top to bottom or side
# by side, and for three panes stacked, in a row or nested.
#
# It also checks properties that must hold after every command: the active pane
# is not hidden if any pane is not, there is one active pane, a window with one
# pane is not zoomed, window_zoomed_flag matches the visible zoomed panes and visible panes have
# a size. The status line clicks and the cursor are in other tests.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
SOCK=testA$$
RUN=0

tm()
{
	$TEST_TMUX -L"$SOCK$RUN" -f/dev/null "$@"
}

cleanup()
{
	tm kill-server >/dev/null 2>&1
}
trap cleanup 0
trap 'exit 1' 1 2 15

FMT='#{pane_id} #{pane_floating_flag} #{pane_zoomed_flag} #{pane_hidden_flag} #{pane_active} #{pane_width} #{pane_height} #{window_zoomed_flag}'

# The oracle. The state before is on lines starting B and the state after on
# lines starting A: id floating zoomed hidden active width height window_zoomed.
# op is the operation, tgt the target pane, rc the exit status of the command and
# mode is "setup" to check that the start state was made as wanted, with want
# holding floating, zoomed and hidden for each pane in order.
ORACLE='
function fail(msg)
{
	printf "%s %s (exit %s): %s\n", op, tgt, rc, msg
	bad = 1
	exit 1
}
function sameF(skip,    i, id)
{
	for (i = 1; i <= nb; i++) {
		id = bid[i]
		if (id == skip || !(id in ina))
			continue
		if (aF[id] != bF[id])
			fail("pane " id " floating " bF[id] " became " aF[id])
	}
}
function sameZ(skip,    i, id)
{
	for (i = 1; i <= nb; i++) {
		id = bid[i]
		if (id == skip || !(id in ina))
			continue
		if (aZ[id] != bZ[id])
			fail("pane " id " zoomed " bZ[id] " became " aZ[id])
	}
}
function sameH(skip,    i, id)
{
	for (i = 1; i <= nb; i++) {
		id = bid[i]
		if (id == skip || !(id in ina))
			continue
		if (aH[id] != bH[id])
			fail("pane " id " hidden " bH[id] " became " aH[id])
	}
}
function noZoom(    i)
{
	for (i = 1; i <= na; i++) {
		if (aZ[aid[i]])
			fail("pane " aid[i] " is still zoomed")
	}
}
function covered(t,    i, q)
{
	if (bF[t] || bZ[t])
		return 0
	for (i = 1; i <= nb; i++) {
		q = bid[i]
		if (q != t && bZ[q] && !bH[q])
			return 1
	}
	return 0
}
function newPane(    i, k, id)
{
	k = 0
	for (i = 1; i <= na; i++) {
		id = aid[i]
		if (!(id in inb)) {
			k++
			created = id
		}
	}
	if (k != 1)
		fail(k " new panes, expected 1")
	if (na != nb + 1)
		fail("pane count " nb " became " na)
}
$1 == "B" {
	nb++; id = $2; bid[nb] = id; inb[id] = 1
	bF[id] = $3; bZ[id] = $4; bH[id] = $5; bA[id] = $6
}
$1 == "A" {
	na++; id = $2; aid[na] = id; ina[id] = 1
	aF[id] = $3; aZ[id] = $4; aH[id] = $5; aA[id] = $6
	aW[id] = $7; aT[id] = $8; aWZ[id] = $9
}
END {
	if (mode == "setup") {
		split(want, w, " ")
		for (i = 1; i <= nb; i++) {
			id = bid[i]; p = substr(id, 2) + 0
			if (bF[id] != w[3 * p + 1] || bZ[id] != w[3 * p + 2] ||
			    bH[id] != w[3 * p + 3])
				fail("pane " id " is " bF[id] bZ[id] bH[id] ", wanted " \
				    w[3 * p + 1] w[3 * p + 2] w[3 * p + 3])
		}
		exit 0
	}

	# Properties of any state.
	if (na < 1)
		fail("no panes left")
	nact = 0; anyZ = 0; anyvis = 0
	for (i = 1; i <= na; i++) {
		id = aid[i]
		if (aA[id] == 1) { nact++; act = id }
		if (aZ[id] && !aH[id]) anyZ = 1
		if (!aH[id]) anyvis = 1
	}
	if (nact != 1)
		fail(nact " active panes")
	for (i = 1; i <= na; i++) {
		id = aid[i]
		if (aWZ[id] != anyZ)
			fail("window_zoomed_flag " aWZ[id] " with visible zoomed panes " anyZ)
		if (!aH[id] && (aW[id] < 1 || aT[id] < 1))
			fail("pane " id " is " aW[id] "x" aT[id])
	}
	if (na == 1 && anyZ)
		fail("the only pane is zoomed")
	if (anyvis && aH[act])
		fail("active pane " act " is hidden but others are not")

	# A command that failed changed nothing.
	if (rc != 0) {
		if (na != nb)
			fail("pane count " nb " became " na)
		sameF(""); sameZ(""); sameH("")
		exit 0
	}

	T = tgt
	if (op == "select") {
		if (aA[T] != 1) fail("not active")
		if (aH[T]) fail("still hidden")
		if (covered(T)) noZoom(); else sameZ("")
		sameF(""); sameH(T)
	} else if (op == "zoom") {
		sameF("")
		if (bZ[T]) {
			if (aZ[T]) fail("not unzoomed")
			sameZ(T); sameH("")
		} else {
			if (!aZ[T]) fail("not zoomed")
			if (aH[T]) fail("zoomed pane is hidden")
			if (aA[T] != 1) fail("zoomed pane is not active")
			sameZ(T); sameH(T)
		}
	} else if (op == "hide") {
		sameF("")
		if (bH[T]) {
			if (aH[T]) fail("not shown")
			if (aA[T] != 1) fail("not active when shown")
			if (covered(T)) noZoom(); else sameZ("")
		} else {
			if (!aH[T]) fail("not hidden")
			sameZ("")
		}
		sameH(T)
	} else if (op == "desktop") {
		sameF(""); sameZ("")
		for (i = 1; i <= nb; i++) {
			id = bid[i]
			want = (bH[id] + bF[id] + bZ[id] > 0) ? 1 : 0
			if (aH[id] != want)
				fail("pane " id " hidden " bH[id] " floating " bF[id] \
				    " zoomed " bZ[id] " became hidden " aH[id])
		}
	} else if (op == "desktop2" || op == "desktop3") {
		# The first call hides every floating and zoomed pane, the
		# second shows the floats and the third the zoomed panes. A
		# call with nothing to show is skipped, so with only one kind
		# the second call restores them and the third hides them again.
		nf = 0; nz = 0
		for (i = 1; i <= nb; i++) {
			id = bid[i]
			if (bH[id]) continue
			if (bZ[id]) nz++
			else if (bF[id]) nf++
		}
		sameF(""); sameZ("")
		for (i = 1; i <= nb; i++) {
			id = bid[i]
			want = bH[id]
			if (!bH[id] && bZ[id])
				want = (op == "desktop2") ? (nf > 0) : (nf == 0 || nz == 0)
			else if (!bH[id] && bF[id])
				want = (op == "desktop3") ? (nf == 0 || nz == 0) : 0
			if (aH[id] != want)
				fail("pane " id " hidden " bH[id] " floating " bF[id] \
				    " zoomed " bZ[id] " became hidden " aH[id] \
				    " after " op " (floats " nf ", zooms " nz ")")
		}
	} else if (op == "unzoomall") {
		sameF(""); sameH(""); noZoom()
	} else if (op == "break") {
		if (bF[T] || !aF[T]) fail("not floated")
		sameF(T); sameZ(""); sameH("")
	} else if (op == "join") {
		if (!bF[T] || aF[T]) fail("not tiled")
		sameF(T); sameZ(""); sameH("")
	} else if (op == "kill") {
		if (T in ina) fail("not killed")
		if (na != nb - 1) fail("pane count " nb " became " na)
		sameF(T); sameH(T)
		if (na == 1) noZoom(); else sameZ(T)
	} else if (op == "split") {
		newPane()
		if (aF[created] != bF[T]) fail("new pane floating " aF[created])
		if (aZ[created] || aH[created]) fail("new pane zoomed or hidden")
		sameF(""); sameZ("")
		if (bH[T] && !bF[T]) {
			if (aH[T]) fail("tiled target not shown")
			sameH(T)
		} else
			sameH("")
	} else if (op == "newpane") {
		newPane()
		if (!aF[created]) fail("new pane is not floating")
		if (aZ[created] || aH[created]) fail("new pane zoomed or hidden")
		sameF(""); sameZ(""); sameH("")
	} else if (op == "nextlayout" || op == "tiled") {
		sameF(""); sameH(""); noZoom()
	} else {
		# Moving around or changing layout: floating panes stay floating
		# and nothing is hidden.
		sameF("")
		for (i = 1; i <= nb; i++) {
			id = bid[i]
			if (id in ina && aH[id] > bH[id])
				fail("pane " id " was hidden by this")
		}
	}
	exit 0
}
'

# describe N STATE: print the start state in words.
describe()
{
	n=$1
	s=$2
	i=0
	out=
	while [ $i -lt "$n" ]; do
		bits=$(( (s >> (3 * i)) & 7 ))
		k=T
		[ $((bits & 1)) -ne 0 ] && k=F
		z=
		[ $((bits & 2)) -ne 0 ] && z=z
		h=
		[ $((bits & 4)) -ne 0 ] && h=h
		out="$out %$i=$k$z$h"
		i=$((i + 1))
	done
	echo "$out"
}

# run_one N STATE OP TARGET: set up the state, run the operation, check it.
run_one()
{
	n=$1
	s=$2
	op=$3
	tgt=$4
	RUN=$((RUN + 1))

	# The tiled panes are split in different ways: top to bottom or side by
	# side for two, and stacked, in a row or nested for three.
	set -- new-session -d -x 80 -y 24 cat
	if [ "$n" -eq 2 ]; then
		shape=$((RUN % 2))
	else
		shape=$((RUN % 3))
	fi
	case "$n$shape" in
	20) set -- "$@" ';' split-window -d '' ;;
	21) set -- "$@" ';' split-window -d -h '' ;;
	30) set -- "$@" ';' split-window -d '' ';' split-window -d '' ;;
	31) set -- "$@" ';' split-window -d -h '' ';' split-window -d -h '' ;;
	32) set -- "$@" ';' split-window -d -h -t %0 '' \
	    ';' split-window -d -v -t %1 '' ;;
	esac
	want=
	for stage in 1 2 4; do
		i=0
		while [ $i -lt "$n" ]; do
			bits=$(( (s >> (3 * i)) & 7 ))
			if [ $((bits & stage)) -ne 0 ]; then
				case $stage in
				1) set -- "$@" ';' break-pane -d -W -s "%$i" ;;
				2) set -- "$@" ';' resize-pane -Z -t "%$i" ;;
				4) set -- "$@" ';' resize-pane -H -t "%$i" ;;
				esac
			fi
			i=$((i + 1))
		done
	done
	i=0
	while [ $i -lt "$n" ]; do
		bits=$(( (s >> (3 * i)) & 7 ))
		want="$want $((bits & 1)) $(((bits >> 1) & 1)) $(((bits >> 2) & 1))"
		i=$((i + 1))
	done
	set -- "$@" ';' list-panes -F "B $FMT"
	before=$(tm "$@" 2>&1)
	if ! msg=$(echo "$before" | awk -v mode=setup -v want="$want" \
	    -v op=setup -v tgt=- -v rc=0 "$ORACLE" 2>&1); then
		echo "setup of $(describe "$n" "$s") failed: $msg"
		echo "$before"
		cleanup
		exit 1
	fi

	p="-t $tgt"
	rc=0
	case $op in
	select) tm select-pane -t "$tgt" || rc=$? ;;
	selectZ) tm select-pane -Z -t "$tgt" || rc=$? ;;
	zoom) tm resize-pane -Z -t "$tgt" || rc=$? ;;
	hide) tm resize-pane -H -t "$tgt" || rc=$? ;;
	break) tm break-pane -d -W -s "$tgt" || rc=$? ;;
	join) tm join-pane -d -s "$tgt" -t "$tgt" || rc=$? ;;
	kill) tm kill-pane -t "$tgt" || rc=$? ;;
	split) tm split-window -d -t "$tgt" '' || rc=$? ;;
	newpane) tm new-pane -d -t "$tgt" -x 20 -y 6 '' || rc=$? ;;
	desktop) tm resize-pane -a -H -t %0 || rc=$? ;;
	desktop2)
		tm resize-pane -a -H -t %0 || rc=$?
		tm resize-pane -a -H -t %0 || rc=$?
		;;
	desktop3)
		tm resize-pane -a -H -t %0 || rc=$?
		tm resize-pane -a -H -t %0 || rc=$?
		tm resize-pane -a -H -t %0 || rc=$?
		;;
	unzoomall) tm resize-pane -a -Z -t %0 || rc=$? ;;
	nextlayout) tm next-layout -t %0 || rc=$? ;;
	tiled) tm select-layout -t %0 tiled || rc=$? ;;
	rotate) tm rotate-window -t %0 || rc=$? ;;
	nextpane) tm select-pane -t :.+ || rc=$? ;;
	lastpane) tm last-pane -t %0 || rc=$? ;;
	left) tm select-pane -L || rc=$? ;;
	right) tm select-pane -R || rc=$? ;;
	up) tm select-pane -U || rc=$? ;;
	down) tm select-pane -D || rc=$? ;;
	resize) tm resize-window -t %0 -x 50 -y 14 || rc=$? ;;
	*) echo "unknown operation $op"; exit 1 ;;
	esac >/dev/null 2>&1

	after=$(tm list-panes -F "A $FMT" 2>&1)
	result=$( (echo "$before"; echo "$after") |
	    awk -v mode=check -v op="$op" -v tgt="$tgt" -v rc="$rc" "$ORACLE")
	if [ $? -ne 0 ] || [ -n "$result" ]; then
		echo "FAILED with $n panes, start $(describe "$n" "$s"):"
		echo "  $result"
		echo "before:"; echo "$before" | sed 's/^/  /'
		echo "after:"; echo "$after" | sed 's/^/  /'
		cleanup
		exit 1
	fi
	cleanup
}

PER_PANE="select selectZ zoom hide break join kill split newpane"
GLOBAL="desktop desktop2 desktop3 unzoomall nextlayout tiled rotate nextpane lastpane \
    left right up down resize"

# Two panes: every state, every operation.
s=0
while [ $s -lt 64 ]; do
	for op in $GLOBAL; do
		run_one 2 $s "$op" %0
	done
	for op in $PER_PANE; do
		run_one 2 $s "$op" %0
		run_one 2 $s "$op" %1
	done
	s=$((s + 1))
done

# Three panes: every state, and two operations on each, rotating so that every
# operation and target is used with many different states.
ALL="$GLOBAL"
for op in $PER_PANE; do
	ALL="$ALL $op:0 $op:1 $op:2"
done
set -- $ALL
M=$#
s=0
while [ $s -lt 512 ]; do
	for k in $(( (s * 7 + s / M) % M )) $(( (s * 5 + 11) % M )); do
		set -- $ALL
		shift $k
		item=$1
		case $item in
		*:*) run_one 3 $s "${item%:*}" "%${item#*:}" ;;
		*) run_one 3 $s "$item" %0 ;;
		esac
	done
	s=$((s + 1))
done

exit 0
