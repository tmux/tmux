#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export PATH TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

TMP=$(mktemp "${TMPDIR:-/tmp}/input-reflow-stress.XXXXXX") || exit 1
EXP=$(mktemp "${TMPDIR:-/tmp}/input-reflow-stress.XXXXXX") || exit 1
exit_status=0

WIDTHS="80 40 20 10 7 5 4 3 2 1 2 3 4 5 7 10 20 40 80"
LIVE_WIDTHS="68 24 80"
HEIGHT=24
HISTORY_LIMIT=220
HISTORY_BOUND=12000
JOINED_BOUND=500000
RAW_BOUND=5000000
COPY_BOUND=20000
LIVE_BOUND=50000

cleanup()
{
	rm -f "$TMP" "$EXP"
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}
trap cleanup 0 1 15

record_fail()
{
	echo "FAIL: $1" >&2
	exit_status=1
}

u8()
{
	printf "$1"
}

wide_a()
{
	u8 '\0343\0201\0202'
}

wide_b()
{
	u8 '\0347\0225\0214'
}

combining_acute()
{
	u8 '\0314\0201'
}

zero_width_space()
{
	u8 '\0342\0200\0213'
}

zero_width_joiner()
{
	u8 '\0342\0200\0215'
}

replacement()
{
	u8 '\0357\0277\0275'
}

assert_alive()
{
	$TMUX display-message -p -t stress: alive >/dev/null 2>&1 ||
		record_fail "server exited after $1"
}

make_orphan_payload()
{
	tag=$1

	printf 'OP%s-01|ECH-half|left' "$tag"
	wide_b
	printf 'right'
	printf '\033[6D\033[1X\n'

	printf 'OP%s-02|EL-inside|left' "$tag"
	wide_b
	printf 'right'
	printf '\033[6D\033[K\n'

	printf 'OP%s-03|overwrite-leading|left' "$tag"
	wide_b
	printf 'right'
	printf '\rOP%s-03|overwrite-leading|left!\n' "$tag"

	printf 'OP%s-04|right-edge|' "$tag"
	i=0
	while [ "$i" -lt 60 ]; do
		printf '.'
		i=$((i + 1))
	done
	wide_b
	printf 'Z'
	printf '\033[2D\033[1X\n'

	printf 'OP%s-05|ERASED|left' "$tag"
	wide_b
	printf 'right\r\033[2KOP%s-05|ERASED|left clear\n' "$tag"

	printf 'SENT-%s|ORPHAN-PHASE|complete\n' "$tag"
}

make_payload()
{
	i=0
	while [ "$i" -lt 16 ]; do
		printf 'ASCII%02d|abcdefghijklmnopqrstuvwxyz\n' "$i"

		printf 'WIDE%02d|' "$i"
		wide_a
		printf '|'
		wide_b
		printf '|tail\n'

		printf 'COMB%02d|e' "$i"
		combining_acute
		printf '|zero'
		zero_width_space
		zero_width_joiner
		printf '|done\n'

		printf 'STYLE%02d|\033[1;31mred\033[0m|\033[4munder\033[0m\n' \
			"$i"

		printf 'LINK%02d|\033]8;;https://example.invalid/%d\007link%d\033]8;;\007|end\n' \
			"$i" "$i" "$i"

		printf 'WRAP%02d|%064d\n' "$i" "$i"

		printf 'CRBS%02d|abcdef\rCRBS%02d|XYZ\n' "$i" "$i"
		printf 'BS%02d|abc\bZ\n' "$i"
		i=$((i + 1))
	done

	i=0
	while [ "$i" -lt 6 ]; do
		printf 'L%04d|stable|abcdefghijklmnopqrstuvwxyz\n' "$i"
		i=$((i + 1))
	done

	printf 'SENT-A|SURVIVES|plain logical line\n'
	printf 'SENT-B|SURVIVES|wide-free after erases\n'
	make_orphan_payload PRE
	printf 'SENT-C|SURVIVES|tail logical line\n'
}

make_alternate_payload()
{
	printf '\033[?1049h'
	printf 'ALT-SENT|alternate screen|'
	wide_a
	printf '\nALT-SENT|resize target\n'
}

exit_alternate_payload()
{
	printf '\033[?1049l'
}

load_and_paste()
{
	buffer=$1
	shift

	"$@" >"$EXP"
	$TMUX load-buffer -b "$buffer" "$EXP" || exit 1
	$TMUX paste-buffer -d -b "$buffer" -t stress:0.0 || exit 1
	sleep 0.3
}

capture_joined()
{
	$TMUX capture-pane -pNJ -t stress: -S -5000 -E - >"$TMP"
}

assert_joined_sane()
{
	label=$1

	capture_joined || {
		record_fail "joined capture failed after $label"
		return
	}

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt "$JOINED_BOUND" ]; then
		record_fail "joined capture too large after $label: $bytes"
	fi

	if grep -q "$(replacement)" "$TMP"; then
		record_fail "replacement character in joined capture after $label"
	fi

	out=$(
		awk -v label="$label" '
		{
			line = $0
			seen_on_line = 0
			while (match(line, /L[0-9][0-9][0-9][0-9]\|/)) {
				id = substr(line, RSTART + 1, 4) + 0
				seen_on_line++
				seen[id]++
				ids[++count] = id
				found = 1
				last = id
				line = substr(line, RSTART + RLENGTH)
			}
			if (seen_on_line > 1)
				printf("fused IDs after %s: %s\n", label, $0)
		}
		END {
			if (!found) {
				printf("no line IDs after %s\n", label)
				exit
			}
			for (i = 2; i <= count; i++) {
				if (ids[i] < ids[i - 1])
					drops++
			}
			if (drops > 1)
				printf("IDs out of order after %s\n", label)
			for (id in seen) {
				if (seen[id] > 2)
					printf("duplicate ID after %s: L%04d\n", label, id)
			}
		}' "$TMP"
	)
	[ -z "$out" ] || record_fail "$out"

	for marker in \
		'SENT-A|SURVIVES|plain logical line' \
		'SENT-B|SURVIVES|wide-free after erases' \
		'SENT-C|SURVIVES|tail logical line'
	do
		grep -F "$marker" "$TMP" >/dev/null 2>&1 ||
			record_fail "missing sentinel after $label: $marker"
	done

	if grep '^OP.*-05|ERASED|.*' "$TMP" | grep -q "$(wide_b)"; then
		record_fail "erased wide character visible after $label"
	fi
}

assert_final_logical_text()
{
	label=$1

	capture_joined || {
		record_fail "joined capture failed after $label"
		return
	}

	for marker in \
		'SENT-A|SURVIVES|plain logical line' \
		'SENT-B|SURVIVES|wide-free after erases' \
		'SENT-C|SURVIVES|tail logical line' \
		'OPPRE-03|overwrite-leading|left!' \
		'OPPOST-03|overwrite-leading|left!' \
		'SENT-POST|ORPHAN-PHASE|complete'
	do
		grep -F "$marker" "$TMP" >/dev/null 2>&1 ||
			record_fail "missing expected logical text after $label: $marker"
	done
}

assert_raw_sane()
{
	label=$1

	$TMUX capture-pane -pR -t stress: >"$TMP" ||
		record_fail "raw capture failed after $label"

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt "$RAW_BOUND" ]; then
		record_fail "raw capture too large after $label: $bytes"
	fi

	width=$($TMUX display-message -p -t stress: '#{pane_width}' 2>/dev/null)
	case "$width" in
	''|*[!0-9]*) width=0 ;;
	esac

	out=$(
		awk -v label="$label" -v width="$width" '
		{
			sub(/^[	 ]+/, "")
			if ($1 != "C")
				next

			coord = $2
			sub(/^[0-9]*,/, "", coord)
			sub(/[^0-9].*$/, "", coord)
			col = coord + 0
			if (width > 0 && (col < 0 || col >= width))
				printf("cell column outside width after %s: width %d: %s\n", label, width, $0)

			data = $0
			if (data !~ /data=\(/) {
				printf("cell without data after %s: %s\n", label, $0)
				next
			}
			sub(/^.*data=\(/, "", data)
			split(data, parts, ",")
			cell_width = parts[1] + 0
			padding = ($0 ~ /flags=[^ ]*PADDING/)
			if (!padding && cell_width == 0)
				printf("visible zero-width cell after %s: %s\n", label, $0)
		}' "$TMP"
	)
	[ -z "$out" ] || record_fail "$out"
}

assert_history_sane()
{
	label=$1
	size=$($TMUX display-message -p -t stress: '#{history_size}' 2>/dev/null)
	case "$size" in
	''|*[!0-9]*)
		record_fail "bad history size after $label: $size"
		;;
	*)
		if [ "$size" -gt "$HISTORY_BOUND" ]; then
			record_fail "history too large after $label: $size"
		fi
		;;
	esac
}

assert_copy_mode_sane()
{
	label=$1

	$TMUX capture-pane -pM -S -5000 -E - -t stress: >"$TMP" ||
		record_fail "copy-mode capture failed after $label"

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt "$COPY_BOUND" ]; then
		record_fail "copy-mode capture too large after $label: $bytes"
	fi

	if grep -q "$(replacement)" "$TMP"; then
		record_fail "replacement character in copy-mode after $label"
	fi

	if ! grep -Eq 'L[0-9][0-9][0-9][0-9]\|' "$TMP"; then
		record_fail "no line ID in copy-mode after $label"
	fi
}

run_resize_checks()
{
	for width in $WIDTHS; do
		$TMUX resize-window -t stress: -x "$width" -y "$HEIGHT" || exit 1
		sleep 0.1
		assert_alive "resize to $width"
		assert_joined_sane "resize to $width"
		assert_raw_sane "resize to $width"
		assert_history_sane "resize to $width"
	done
}

wait_outer_contains()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		$TMUX2 capture-pane -p -t out:0 >"$TMP" 2>/dev/null &&
			grep -F "$marker" "$TMP" >/dev/null 2>&1 &&
			return 0
		sleep 0.2
		i=$((i + 1))
	done
	return 1
}

assert_live_client_redraw()
{
	$TMUX set-option -t stress: status on >/dev/null || exit 1
	$TMUX set-option -t stress: status-interval 1 >/dev/null || exit 1

	$TMUX2 kill-server 2>/dev/null
	$TMUX2 new-session -d -s out -x 100 -y 12 "$TMUX attach -t stress" ||
		exit 1

	i=0
	while [ "$i" -lt 50 ]; do
		clients=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$clients" -ge 1 ] && break
		sleep 0.2
		i=$((i + 1))
	done
	[ "$i" -lt 50 ] || {
		record_fail "nested client did not attach"
		return
	}

	for width in $LIVE_WIDTHS; do
		$TMUX set-option -t stress: status-left "REDRAW-$width " >/dev/null ||
			exit 1
		$TMUX2 resize-window -t out: -x "$width" -y 12 || exit 1
		sleep 0.2
	done

	wait_outer_contains 'REDRAW-80' ||
		record_fail "outer capture missing final redraw marker"

	$TMUX2 capture-pane -p -t out:0 >"$TMP" 2>/dev/null ||
		record_fail "outer capture failed"

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt "$LIVE_BOUND" ]; then
		record_fail "outer capture too large: $bytes"
	fi

	if grep -q "$(replacement)" "$TMP"; then
		record_fail "replacement character in outer capture"
	fi

	grep -F 'SENT-POST|ORPHAN-PHASE|complete' "$TMP" >/dev/null 2>&1 ||
		record_fail "outer capture missing expected sentinel"

	grep -F 'REDRAW-24' "$TMP" >/dev/null 2>&1 &&
		record_fail "outer capture contains stale width marker"

	$TMUX2 kill-server 2>/dev/null
	$TMUX set-option -t stress: status off >/dev/null || exit 1
}

$TMUX kill-server 2>/dev/null
$TMUX2 kill-server 2>/dev/null
sleep 0.1

$TMUX new-session -d -x 1 -y 1 -s test-setup "sleep 2" || exit 1
$TMUX set-option -g history-limit "$HISTORY_LIMIT" || exit 1
$TMUX new-session -d -x 80 -y "$HEIGHT" -s stress 'cat' || exit 1
$TMUX kill-session -t test-setup || exit 1
sleep 0.3

load_and_paste stress-data make_payload
assert_joined_sane "initial payload"
assert_raw_sane "initial payload"
assert_history_sane "initial payload"

$TMUX resize-window -t stress: -x 40 -y "$HEIGHT" || exit 1
sleep 0.1
load_and_paste stress-orphan-post make_orphan_payload POST
assert_joined_sane "post-resize orphan payload"
assert_raw_sane "post-resize orphan payload"

run_resize_checks
assert_final_logical_text "return to original width"

$TMUX copy-mode -H -t stress: || exit 1
$TMUX send-keys -t stress: -X history-top
sleep 0.1
assert_alive "copy-mode"
assert_copy_mode_sane "copy-mode history-top"
$TMUX send-keys -t stress: -X cancel
sleep 0.1

assert_live_client_redraw

load_and_paste stress-alt make_alternate_payload
for width in 30 12 80; do
	$TMUX resize-window -t stress: -x "$width" -y "$HEIGHT" || exit 1
	sleep 0.1
	assert_alive "alternate resize to $width"
	assert_raw_sane "alternate resize to $width"
done
load_and_paste stress-alt-exit exit_alternate_payload
sleep 0.3
assert_alive "alternate screen exit"
assert_joined_sane "alternate screen exit"
assert_raw_sane "alternate screen exit"
assert_final_logical_text "alternate screen exit"

cleanup
exit $exit_status
