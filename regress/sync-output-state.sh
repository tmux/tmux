#!/bin/sh

. ./regress-common.inc

# Check live client output after wraps, scrolls and interrupted updates, and
# attribute transitions during full redraws.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
if [ "$#" -eq 0 ]; then
	TEST_TMUX="$TEST_TMUX" sh "$0" bce || exit 1
	TEST_TMUX="$TEST_TMUX" sh "$0" nobce || exit 1
	exit 0
fi
case $1 in
bce) overrides='*:U8=0:bce' ;;
nobce) overrides='*:U8=0:bce@' ;;
*) exit 1 ;;
esac
DIR=$(test_tmpdir) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
INNER="$TEST_TMUX -Li$$ -f/dev/null"
OUTER="$TEST_TMUX -Lo$$ -f/dev/null"
marker=0

cleanup()
{
	stop_server $OUTER || exit 1
	stop_server $INNER || exit 1
	rm -rf "$DIR"
}
trap cleanup 0 1 15

fail()
{
	echo "$*" >&2
	exit 1
}

# The pane echoes literal bytes; a trailing title acknowledges their parsing.
emit()
{
	marker=$((marker + 1))
	printf '%b\033]2;state-%s\007' "$1" "$marker" |
	    $INNER load-buffer -b state - || exit 1
	$INNER paste-buffer -b state -d -r -S -t inner:0.0 || exit 1
	wait_for_output "state-$marker" $INNER display-message -p \
	    -t inner:0.0 '#{pane_title}' || exit 1
}

captures_match()
{
	$INNER capture-pane "$1" -S 0 -E - -t inner:0.0 >"$DIR/inner" ||
	    return 1
	$OUTER capture-pane "$1" -S 0 -E - -t "$2":0.0 >"$DIR/outer" ||
	    return 1
	# -J keeps trailing padding added by full redraws.
	if [ "$1" = -pJ ]; then
		for file in inner outer; do
			sed 's/ *$//' "$DIR/$file" >"$DIR/trimmed"
			mv "$DIR/trimmed" "$DIR/$file"
		done
	fi
	cmp -s "$DIR/inner" "$DIR/outer"
}

check_screen()
{
	for flags in ${3:--pe}; do
		if ! wait_until captures_match "$flags" "${2:-outer}"; then
			diff -u "$DIR/inner" "$DIR/outer" >&2
			fail "$1: client differs from pane ($flags)"
		fi
	done
}

reset_screen()
{
	emit '\033[?2026l\033[r\033[0m\033[2J\033[H'
	seed=abcdefghijklmnopqrstuvwxyz123456789
	for row in 1 2 3 4 5 6 7 8; do
		emit "\033[$row;1Hrow$row-$seed"
	done
	check_screen reset
}

$INNER new-session -d -s inner -x 40 -y 8 \
    "stty raw -echo; printf '\033]2;ready\007'; exec cat" || exit 1
$INNER set-option -g status off || exit 1
$INNER set-option -g window-size manual || exit 1
$INNER set-option -as terminal-features '*:RGB:usstyle:hyperlinks' || exit 1
$INNER set-option -as terminal-overrides "$overrides" || exit 1
wait_for_output ready $INNER display-message -p '#{pane_title}' || exit 1
$OUTER new-session -d -s outer -x 40 -y 8 'sleep 100' || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
$OUTER respawn-pane -k -t outer:0.0 \
    "env TERM=screen $TEST_TMUX -Li$$ -f/dev/null attach -t inner" || exit 1
wait_for_clients 1 $INNER || exit 1
first=$($OUTER display-message -p -t outer:0.0 '#{pane_tty}') || exit 1
$INNER show-messages -T -t "$first" >"$DIR/capabilities" || exit 1
grep -Fq 'U8: (number) 0' "$DIR/capabilities" || fail 'ACS not enabled'
if [ "$1" = bce ]; then
	grep -Fq 'bce: (flag) true' "$DIR/capabilities" ||
	    fail 'BCE not enabled'
else
	grep -Fq 'bce: [missing]' "$DIR/capabilities" || fail 'BCE not disabled'
fi

# Wrap at the bottom of the screen and of a smaller scroll region. Check
# logical line joining as well as text, including a wide character wrapping.
reset_screen
emit '\033[?2026h\033[8;39HABC\033[?2026l'
check_screen bottom-wrap outer '-pe -pJ'
reset_screen
emit '\033[?2026h\033[2;6r\033[6;39H\0347\0225\0214Z\033[r\033[?2026l'
check_screen region-wrap outer '-pe -pJ'
reset_screen
# The wide character wraps without changing a cell on the old line.
emit '\033[?2026h\033[8;40H\0347\0225\0214Z\033[?2026l'
check_screen wide-bottom-wrap outer '-pe -pJ'

# Only the safety timer ends this update. Before it fires, the client must
# still show the old contents; after it fires, both sparse changes must arrive.
reset_screen
$OUTER capture-pane -p -t outer:0.0 >"$DIR/before" || exit 1
emit '\033[?2026h\033[2;3HX\033[7;29HY'
[ "$($INNER display-message -p '#{synchronized_output_flag}')" = 1 ] ||
    fail 'timeout update already ended'
$OUTER capture-pane -p -t outer:0.0 >"$DIR/after" || exit 1
cmp -s "$DIR/before" "$DIR/after" || fail 'update drawn before timeout'
wait_for_output 0 $INNER display-message -p \
    '#{synchronized_output_flag}' || exit 1
check_screen timeout

# Shrinking must discard pending bitmaps of the old size. A later update
# must still work after growing the pane again.
reset_screen
for row in 1 2 3 4 5 6 7 8; do
	emit "\033[$row;1H\033[2Krow$row-abcdefghijklmnopqrstuvwxyz"
done
check_screen resize-seed
emit '\033[?2026h\033[4;27HX\033[8;39HY'
$INNER resize-window -t inner:0 -x 32 -y 6 || exit 1
$OUTER resize-window -t outer:0 -x 32 -y 6 || exit 1
wait_for_output 0 $INNER display-message -p \
    '#{synchronized_output_flag}' || exit 1
check_screen shrink
emit '\033[?2026h\033[3;30HQ'
$INNER resize-window -t inner:0 -x 40 -y 8 || exit 1
$OUTER resize-window -t outer:0 -x 40 -y 8 || exit 1
wait_for_output 0 $INNER display-message -p \
    '#{synchronized_output_flag}' || exit 1
check_screen grow
emit '\033[?2026h\033[8;39HZ\033[?2026l'
check_screen after-resize-update

# Switch screens while dirty cells are pending, then restore the main screen
# while a different set of cells is pending on the alternate screen.
reset_screen
emit '\033[?2026h\033[2;3H\033[1;31mX\033[7;29H\033[4;44mY'
emit '\033[?1049h\033[0mALT\033[?2026l'
check_screen alternate-on
emit '\033[?2026h\033[3;4H\033[7mQ\033[?1049l\033[?2026l'
check_screen alternate-off

# Flush each scroll into the dirty bookkeeping before changing its background
# or region. Also accumulate enough scrolling to replace the whole region.
reset_screen
emit '\033[?2026h\033[2;3HX\033[7;29HY\033[8;1H\n'
emit '\033[44m\033[8;1H\n'
emit '\033[2;6r\033[6;1H\n\033[r\033[0m\033[?2026l'
check_screen changed-scroll-region
emit '\033[?2026h\033[3;3HX\033[3S'
emit '\033[5S\033[?2026l'
check_screen whole-region-scroll

# Attribute changes must also survive full redraws now that tty_draw_line
# retains the previous attributes. Exercise ACS and clearing with/without BCE.
reset_screen
emit '\033[1;1H\033[1;4;7;31;44mstyled\033[0m\033[K'
emit '\033[2;1H\033(0qqxx\033(Bplain\033[K'
emit '\033[3;1H\033[4;58:2::12:34:56munder\033[0m\033[K'
emit '\033[4;1H\033[38;2;123;45;67;48;2;12;34;56mRGB\033[0mplain\033[K'
emit '\033[5;1H\033[7mreverse\033[0m\033[6;1H\033[2K'
emit '\033[7;1H\033[4munderline\033[0m\033[8;1H\033[2K'
check_screen attributes-live
$OUTER pipe-pane -O -t outer:0.0 "cat >'$DIR/redraw'" || exit 1
$INNER refresh-client || exit 1
wait_for_text styled cat "$DIR/redraw" || exit 1
$OUTER pipe-pane -t outer:0.0 || exit 1
check_screen attributes-redraw
emit '\033[?2026h\033[1;2H\033[0mX\033[4;3H\033[7mY'
emit '\033[0m\033[7;1H\033[2K\033[?2026l'
check_screen attributes-sync

# Editing part of a styled tab must retain the style outside the edit, both
# in the grid and on the client, with and without synchronized output.
for style in 4 7; do
	case $style in
	4) attribute=UNDERSCORE ;;
	7) attribute=REVERSE ;;
	esac
	for sync in off on; do
		begin= end=
		if [ "$sync" = on ]; then
			begin='\033[?2026h'
			end='\033[?2026l'
		fi
		for edit in el el1 ed ed1 ech ich dch ascii wide irm; do
			context="tab-$style-$sync-$edit"
			case $edit in
			el) seq='\033[K'; columns=0 ;;
			el1) seq='\033[1K'; columns=7 ;;
			ed) seq='\033[J'; columns=0 ;;
			ed1) seq='\033[1J'; columns=7 ;;
			ech) seq='\033[2X'; columns='0 7' ;;
			ich) seq='\033[2@'; columns='0 9' ;;
			dch) seq='\033[2P'; columns='0 5' ;;
			ascii) seq=Q; columns='0 7' ;;
			wide) seq='\0347\0225\0214'; columns='0 7' ;;
			irm) seq='\033[4hQ\033[4l'; columns='0 8' ;;
			esac
			emit "\033[3;1H\033[2K\033[${style}m        "
			emit '\033[0m\033[3;1H\tT'
			emit "${begin}\033[3;5H${seq}${end}"
			check_screen "$context"
			$INNER capture-pane -pR -t inner:0.0 >"$DIR/raw" ||
			    exit 1
			tab_y=$($INNER display-message -p '#{history_size}') ||
			    exit 1
			tab_y=$((tab_y + 2))
			for column in $columns; do
				if ! grep -Eq \
				    "C $tab_y,$column .*attr=.*$attribute" \
				    "$DIR/raw"; then
					grep "C $tab_y," "$DIR/raw" >&2
					fail "$context: lost $attribute"
				fi
			done
		done
	done
done

# One client can scroll; the other has no scroll-region capability and must
# receive the redrawn lines. Both must see the same sparse changes.
$INNER set-option -as terminal-overrides 'screen-256color:csr@' || exit 1
$OUTER new-session -d -s second -x 40 -y 8 \
    "env TERM=screen-256color $TEST_TMUX -Li$$ -f/dev/null attach -t inner" ||
    exit 1
wait_for_clients 2 $INNER || exit 1
second=$($OUTER display-message -p -t second:0.0 '#{pane_tty}') || exit 1
$INNER show-messages -T -t "$first" >"$DIR/capabilities" || exit 1
grep -Fq 'csr: (string)' "$DIR/capabilities" ||
    fail 'first client cannot scroll'
$INNER show-messages -T -t "$second" >"$DIR/capabilities" || exit 1
grep -Fq 'csr: [missing]' "$DIR/capabilities" || fail 'second client can scroll'
reset_screen
check_screen second-client second
emit '\033[?2026h\033[2;3HX\033[7;29HY\033[8;1H\n'
emit '\033[8;3HZ\033[?2026l'
check_screen scroll-client
check_screen redraw-client second

echo "$1 state checks passed"
exit 0
