#!/bin/sh

. ./regress-common.inc

# Side status motion uses the named binding and leaves the prefix table active
# when unbound, including over the separator and every control range.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
TMUX2="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	stop_server $TMUX2 || exit 1
	stop_server $TMUX || exit 1
}
trap cleanup 0
trap 'exit 1' 1 2 15

fail()
{
	echo "$side/$protocol/$key: $*" >&2
	exit 1
}

# Follow the report with z so its binding proves the report was processed.
move()
{
	if [ "$protocol" = sgr ]; then
		seq=$(printf '\033[<35;%s;%sM' "$column" "$row")
		$TMUX2 send-keys -t outer:0.0 -l "$seq" z || exit 1
	else
		bytes=$(printf '1b 5b 4d 43 %02x %02x 7a' \
		    "$((column + 32))" "$((row + 32))")
		$TMUX2 send-keys -t outer:0.0 -H $bytes || exit 1
	fi
}

$TMUX new-session -d -s inner -x 80 -y 20 \
    "stty raw -echo; printf '\033[?1003h\033[?1006h'; exec cat" || exit 1
$TMUX set -g mouse on || exit 1
$TMUX set -g status off || exit 1
$TMUX set -g side-status-width 14 || exit 1
$TMUX set -g assume-paste-time 0 || exit 1
$TMUX bind -n Any set -g @move any || exit 1
$TMUX bind -n z set -g @table root || exit 1
$TMUX bind -T prefix z set -g @table prefix || exit 1
$TMUX2 new-session -d -s outer -x 80 -y 20 \
    "$TMUX attach -t inner" || exit 1
$TMUX2 set -g status off || exit 1
wait_for_clients 1 $TMUX || exit 1

serial=0
for side in left right; do
	$TMUX set -g side-status "$side" || exit 1
	for suffix in '' Left Right Default Border \
	    Control0 Control1 Control2 Control3 Control4 \
	    Control5 Control6 Control7 Control8 Control9; do
		key=MouseMoveSideStatus$suffix
		serial=$((serial + 1))
		text=motion$serial
		column=3
		[ "$side" = right ] && column=69
		row=1
		case "$suffix" in
		'') range='window|0' ;;
		Left) range=left ;;
		Right) range=right ;;
		Control*) range="control|${suffix#Control}" ;;
		Default) range=; row=3 ;;
		Border)
			range=; row=3; column=14
			[ "$side" = right ] && column=67
			;;
		esac
		format=$text
		[ -n "$range" ] && format="#[range=$range]$text#[norange]"
		$TMUX set -g side-status-format "$format" || exit 1
		wait_for_text "$text" $TMUX2 capture-pane -p || exit 1

		for protocol in sgr x10; do
			$TMUX bind -n "$key" set -g @move "$key" || exit 1
			$TMUX set -g @move pending || exit 1
			$TMUX set -g @table pending || exit 1
			move
			wait_for_output root $TMUX show -gv @table ||
			    fail 'input did not reach the root binding'
			[ "$($TMUX show -gv @move)" = "$key" ] ||
			    fail 'named motion binding did not fire'

			$TMUX unbind -n "$key" || exit 1
			$TMUX set -g @table pending || exit 1
			$TMUX switch-client -T prefix || exit 1
			move
			wait_for_output prefix $TMUX show -gv @table ||
			    fail 'unbound motion left the prefix table'
		done
	done
done

exit 0
