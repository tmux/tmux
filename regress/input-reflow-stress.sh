#!/bin/sh

WIDTHS="80 40 20 10 7 5 4 3 2 1 2 3 4 5 7 10 20 40 80"
HISTORY_LIMIT=220
HISTORY_BOUND=12000
JOINED_BOUND=500000
RAW_BOUND=5000000

record_fail()
{
	echo "FAIL: $1"
	exit_status=1
}

make_payload()
{
	n=0
	i=0
	while [ "$i" -lt 16 ]; do
		printf 'L%04d|ascii|abcdefghijklmnopqrstuvwxyz\n' "$n"; n=$((n + 1))
		printf 'L%04d|wrap|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n' "$n"; n=$((n + 1))
		printf 'L%04d|wide|AあB界C\n' "$n"; n=$((n + 1))
		printf 'L%04d|wide-edge|abcdあZ\n' "$n"; n=$((n + 1))
		printf 'L%04d|combining|é é é\n' "$n"; n=$((n + 1))
		printf 'L%04d|orphan-combining|́A\n' "$n"; n=$((n + 1))
		printf 'L%04d|variation|✔️ ⚠️ 🏝️\n' "$n"; n=$((n + 1))
		printf 'L%04d|emoji|🙂🙂🙂\n' "$n"; n=$((n + 1))
		printf 'L%04d|flag|🇬🇧 🇺🇸 🇯🇵\n' "$n"; n=$((n + 1))
		printf 'L%04d|mixed|Aあé✔️B\n' "$n"; n=$((n + 1))
		printf 'L%04d|style|plain \033[31mred\033[0m plain\n' "$n"; n=$((n + 1))
		printf 'L%04d|hyperlink|\033]8;;https://example.com/%04d\033\\LINK\033]8;;\033\\\n' "$n" "$n"; n=$((n + 1))
		printf 'L%04d|tabs|a\tb\tc\n' "$n"; n=$((n + 1))
		printf 'L%04d|backspace|abc\bX\n' "$n"; n=$((n + 1))
		printf 'carriage-return-%04d|abcdef\rL%04d|carriage-return|XY\n' "$n" "$n"; n=$((n + 1))
		i=$((i + 1))
	done
}

make_alternate_payload()
{
	printf '\033[?1049h'
	printf 'ALT000|wide|AあB界C\n'
	printf 'ALT001|combining|é é é\n'
	printf 'ALT002|style|plain \033[32mgreen\033[0m plain\n'
	printf 'ALT003|emoji|🙂🙂🙂\n'
}

load_and_paste()
{
	buffer=$1
	shift

	"$@" | $TMUX load-buffer -b "$buffer" - || exit 1
	$TMUX paste-buffer -d -b "$buffer" -t stress:0.0 || exit 1
	sleep 0.3
}

assert_alive()
{
	alive=$($TMUX display-message -p -t stress: alive 2>&1) || {
		record_fail "server died after $1"
		return
	}
	[ "$alive" = alive ] || record_fail "server died after $1"
}

capture_joined()
{
	$TMUX capture-pane -pNJ -t stress: -S - -E - >"$TMP"
}

assert_joined_sane()
{
	label=$1
	check_sentinels=${2:-yes}

	capture_joined || {
		record_fail "joined capture failed after $label"
		return
	}

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt "$JOINED_BOUND" ]; then
		record_fail "joined capture too large after $label: $bytes"
	fi

	if grep -q "$(printf '\357\277\275')" "$TMP"; then
		record_fail "replacement character in joined capture after $label"
	fi

	awk -v label="$label" '
		{
			line = $0
			seen_on_line = 0
			while (match(line, /L[0-9][0-9][0-9][0-9]\|/)) {
				id = substr(line, RSTART + 1, 4) + 0
				seen_on_line++
				seen[id]++
				if (id < last) {
					printf("IDs out of order after %s: L%04d after L%04d\n",
					    label, id, last)
					exit 1
				}
				if (seen[id] > 1) {
					printf("duplicate ID after %s: L%04d\n", label, id)
					exit 1
				}
				last = id
				found = 1
				line = substr(line, RSTART + RLENGTH)
			}
			if (seen_on_line > 1) {
				printf("fused IDs after %s: %s\n", label, $0)
				exit 1
			}
		}
		END {
			if (!found) {
				printf("no line IDs after %s\n", label)
				exit 1
			}
		}
	' "$TMP" || {
		cp "$TMP" "$EXP"
		record_fail "joined structure failed after $label"
	}

	if [ "$check_sentinels" = yes ]; then
		for expected in \
		    'L0234|mixed|' \
		    'L0236|hyperlink|' \
		    'L0239|carriage-return|XY'; do
			if ! grep -Fq "$expected" "$TMP"; then
				printf '%s\n' "$expected" >"$EXP"
				record_fail "missing sentinel after $label"
			fi
		done
	fi
}

assert_raw_sane()
{
	label=$1
	width=$2

	$TMUX capture-pane -pR -t stress: >"$TMP" || {
		record_fail "raw capture failed after $label"
		return
	}

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt "$RAW_BOUND" ]; then
		record_fail "raw capture too large after $label: $bytes"
	fi

	awk -v label="$label" -v width="$width" '
		/^C / {
			cell = $0
			sub(/^C /, "", cell)
			split(cell, fields, " ")
			split(fields[1], position, ",")
			row = position[1] + 0
			col = position[2] + 0
			if (col >= width) {
				printf("cell column outside pane after %s: %s\n", label, $0)
				exit 1
			}

			data = $0
			if (data !~ /data=\(/)
				next
			sub(/^.*data=\(/, "", data)
			split(data, data_fields, ",")
			cell_width = data_fields[1] + 0
			padding = ($0 ~ /flags=PADDING/)

			if (!padding && cell_width == 0) {
				printf("zero-width non-padding cell after %s: %s\n", label, $0)
				exit 1
			}
			if (padding && !(last_row == row && last_width > 1 && !last_padding)) {
				printf("padding without preceding wide cell after %s: %s\n", label, $0)
				exit 1
			}

			last_row = row
			last_width = cell_width
			last_padding = padding
		}
	' "$TMP" || {
		cp "$TMP" "$EXP"
		record_fail "raw structure failed after $label"
	}
}

assert_history_sane()
{
	label=$1

	size=$($TMUX display-message -p -t stress: '#{history_size}') || {
		record_fail "history-size failed after $label"
		return
	}
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

	$TMUX capture-pane -pM -t stress: >"$TMP" || {
		record_fail "copy-mode capture failed after $label"
		return
	}

	bytes=$(wc -c <"$TMP")
	if [ "$bytes" -gt 20000 ]; then
		record_fail "copy-mode capture too large after $label"
	fi
	if grep -q "$(printf '\357\277\275')" "$TMP"; then
		record_fail "replacement character in copy-mode after $label"
	fi
	if ! grep -Eq 'L[0-9][0-9][0-9][0-9]\|' "$TMP"; then
		record_fail "no line ID in copy-mode after $label"
	fi
}

run_resize_checks()
{
	for width in $WIDTHS; do
		$TMUX resize-window -t stress: -x "$width" -y 8 || exit 1
		sleep 0.1
		assert_alive "resize to $width"
		assert_joined_sane "resize to $width"
		assert_raw_sane "resize to $width" "$width"
		assert_history_sane "resize to $width"
	done
}

$TMUX kill-server 2>/dev/null
sleep 0.1
$TMUX new-session -d -x 1 -y 1 -s test-setup "sleep 2" || exit 1
$TMUX set-option -g history-limit "$HISTORY_LIMIT" || exit 1
$TMUX new-session -d -x 80 -y 8 -s stress "stty -echo; cat" || exit 1
$TMUX kill-session -t test-setup
sleep 0.3

load_and_paste stress-data make_payload
run_resize_checks

$TMUX copy-mode -H -t stress: || exit 1
for cmd in history-top page-down halfpage-down halfpage-up page-up history-bottom; do
	$TMUX send-keys -t stress: -X "$cmd" || exit 1
	sleep 0.1
	assert_copy_mode_sane "$cmd"
done
$TMUX send-keys -t stress: -X cancel || exit 1

load_and_paste stress-alt make_alternate_payload
for width in 10 4 1 4 10 80; do
	$TMUX resize-window -t stress: -x "$width" -y 8 || exit 1
	sleep 0.1
	assert_alive "alternate resize to $width"
	assert_raw_sane "alternate resize to $width" "$width"
done
printf '\033[?1049l' | $TMUX load-buffer -b stress-alt-exit - || exit 1
$TMUX paste-buffer -d -b stress-alt-exit -t stress:0.0 || exit 1
sleep 0.3
assert_joined_sane "alternate screen exit" no

$TMUX kill-server 2>/dev/null
exit $exit_status
