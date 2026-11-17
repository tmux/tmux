#!/bin/sh

# Wide-character padding must not count towards the reflowed line width.

. ./input-common.inc

check_reflow()
{
	# Resizing can move the first physical line into history.
	$TMUX capture-pane -pN"$2" -t "$1:" -S - -E - |
	    normalize_capture >"$TMP"
	printf '%s\n' "${3:-$text}" >"$EXP"
	cmp "$TMP" "$EXP" || fail "$1 reflow (width $width, flags -pN$2)"
}

text='中文文档甲   中文文档乙   中文文档丙   中文文档丁   中文文档戊   中文文档己'

# Default and RGB backgrounds exercise compact and extended padding cells.
for padding in compact extended; do
	style=''
	if [ "$padding" = extended ]; then
		style='\033[48;2;1;2;3m'
	fi
	width=96
	start_pane_history "wide-$padding" 96 18 "${style}${text}\\n"
	check_reflow "wide-$padding" ''
	for width in 47 96 47 96; do
		$TMUX resize-window -t "wide-$padding:" -x "$width" -y 18 ||
		    exit 1
		check_reflow "wide-$padding" J
		if [ "$width" = 96 ]; then
			check_reflow "wide-$padding" ''
		fi
	done
done

# Include both a split between wide characters and an odd-width boundary.
text='中文文档'
start_pane_history boundary 8 18 "${text}\\n"
for width in 4 8 3 8; do
	$TMUX resize-window -t boundary: -x "$width" -y 18 || exit 1
	check_reflow boundary J
	case "$width" in
	4)
		check_reflow boundary '' '中文
文档'
		;;
	3)
		check_reflow boundary '' '中
文
文
档'
		;;
	8)
		check_reflow boundary ''
		;;
	esac
done

exit $exit_status
