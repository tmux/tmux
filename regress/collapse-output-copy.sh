#!/bin/sh

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"

$TMUX kill-server 2>/dev/null
trap '$TMUX kill-server 2>/dev/null' 0
trap 'exit 1' 1 2 3 15

$TMUX new-session -d -x20 -y10 \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007FIRST-OUTPUT\r\n\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007012345678901234567890123456789\r\nlast\r\n\033]133;D;7\007\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1

expected=$(printf '012345678901234567890123456789\nlast')
for keys in emacs vi; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode -c || exit 1
	$TMUX send -X search-backward-text second || exit 1
	$TMUX set-buffer sentinel || exit 1
	$TMUX send -X copy-output || exit 1
	[ "$($TMUX show-buffer)" = "$expected" ] || exit 1

	$TMUX send -X select-output || exit 1
	[ "$($TMUX display -p '#{selection_present}')" = 1 ] || exit 1
	$TMUX send -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = "$expected" ] || exit 1
	case "$($TMUX capture-pane -Mp)" in
	*FIRST-OUTPUT*) exit 1 ;;
	esac

	# All-output commands include the original source, even while folded.
	$TMUX send -X copy-output -a || exit 1
	all=$($TMUX show-buffer)
	case "$all" in
	*FIRST-OUTPUT*012345678901234567890123456789*last*) ;;
	*) exit 1 ;;
	esac
	$TMUX send -X select-output -a || exit 1
	$TMUX send -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = "$all" ] || exit 1
	$TMUX send -X cancel || exit 1
done

# A prompt after an output end on the same source line moves left when folded.
$TMUX new-window -n shared \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007head\r\ntail\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007SECOND-OUTPUT\r\n\033]133;D;0\007\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1
for keys in emacs vi; do
	$TMUX set -g mode-keys "$keys" || exit 1
	$TMUX copy-mode -c || exit 1
	$TMUX send -X search-backward-text P1 || exit 1
	$TMUX send -X start-of-line || exit 1
	$TMUX send -X copy-output || exit 1
	[ "$($TMUX show-buffer)" = SECOND-OUTPUT ] || exit 1
	$TMUX send -X select-output || exit 1
	$TMUX send -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = SECOND-OUTPUT ] || exit 1
	$TMUX send -X cancel || exit 1
done

# An output start before the next prompt on the same row anchors the old fold.
$TMUX new-window -n inline \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007FIRST\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007SECOND\033]133;D;0\007\r\n\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1
for keys in emacs vi; do
	$TMUX set -w -t :inline mode-keys "$keys" || exit 1
	for prompt in P0 P1 P2; do
		if [ "$prompt" = P0 ]; then
			expected=FIRST
			hidden=SECOND
		else
			expected=SECOND
			hidden=FIRST
		fi
		$TMUX copy-mode -t :inline -c || exit 1
		$TMUX send -t :inline -X search-backward-text "$prompt>" || exit 1
		$TMUX set-buffer sentinel || exit 1
		$TMUX send -t :inline -X copy-output || exit 1
		[ "$($TMUX show-buffer)" = "$expected" ] || exit 1
		$TMUX set-buffer sentinel || exit 1
		$TMUX send -t :inline -X select-output || exit 1
		$TMUX send -t :inline -X copy-selection || exit 1
		[ "$($TMUX show-buffer)" = "$expected" ] || exit 1
		case "$($TMUX capture-pane -t :inline -Mp)" in
		*"$hidden"*) exit 1 ;;
		esac
		$TMUX send -t :inline -X cancel || exit 1
	done
done

# Manual controls use anchors before the next prompt on a shared source row.
$TMUX new-window -n separator \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;D;0\007SEPARATOR\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007SECOND\033]133;D;0\007\r\n\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1
$TMUX new-window -n silent \
	"printf '\033]133;A\007P0> \033]133;B\007first\r\n\033]133;C\007FIRST\033]133;D;0\007\033]133;A\007P1> \033]133;B\007silent\r\n\033]133;D;0\007SEPARATOR\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1
$TMUX new-window -n inline-start \
	"printf '\033]133;A\007P0> \033]133;B\007first\033]133;C\007FIRST\r\n\033]133;D;0\007\033]133;A\007P1> \033]133;B\007second\r\n\033]133;C\007SECOND\033]133;D;0\007\r\n\033]133;A\007P2> \033]133;B\007'; exec sleep 100" || exit 1
sleep 1
for region in first separator silent inline-start; do
	prompt='P0>'
	neighbor=SECOND
	case "$region" in
	first)
		target=:inline
		fold_text=FIRST
		;;
	separator)
		target=:separator
		fold_text=SEPARATOR
		;;
	silent)
		target=:silent
		fold_text=SEPARATOR
		prompt='P1>'
		neighbor=FIRST
		;;
	inline-start)
		target=:inline-start
		fold_text=FIRST
		;;
	esac
	for keys in emacs vi; do
		$TMUX set -w -t "$target" mode-keys "$keys" || exit 1
		for view in ordinary expanded collapsed; do
			case "$view" in
			ordinary) $TMUX copy-mode -t "$target" || exit 1 ;;
			expanded) $TMUX copy-mode -t "$target" -U || exit 1 ;;
			collapsed) $TMUX copy-mode -t "$target" -c || exit 1 ;;
			esac
			for command in collapse-output expand-output \
			    toggle-output toggle-output; do
				$TMUX send -t "$target" -X \
					search-backward-text "$prompt" || exit 1
				$TMUX send -t "$target" -X "$command" || exit 1
				case "$command" in
				collapse-output) visible=0 ;;
				expand-output) visible=1 ;;
				toggle-output) visible=$((1 - visible)) ;;
				esac
				screen=$($TMUX capture-pane -t "$target" -Mp) || exit 1
				case "$screen" in
				*"$fold_text"*) shown=1 ;;
				*) shown=0 ;;
				esac
				if [ "$shown" != "$visible" ]; then
					echo "$keys $view $command failed for $fold_text"
					exit 1
				fi
				case "$screen" in
				*"$neighbor"*) shown=1 ;;
				*) shown=0 ;;
				esac
				if [ "$view" = collapsed ]; then
					expected=0
				else
					expected=1
				fi
				if [ "$shown" != "$expected" ]; then
					echo "$keys $view $command changed another fold"
					exit 1
				fi
			done
			$TMUX send -t "$target" -X cancel || exit 1
		done
	done
done

# Output cells before a shared prompt still belong to the earlier command.
for keys in emacs vi; do
	$TMUX set -w -t :inline mode-keys "$keys" || exit 1
	for command in collapse-output expand-output toggle-output; do
		$TMUX copy-mode -t :inline -c || exit 1
		$TMUX send -t :inline -X search-backward-text P0 || exit 1
		$TMUX send -t :inline -X select-output || exit 1
		$TMUX send -t :inline -X clear-selection || exit 1
		$TMUX send -t :inline -X search-backward-text FIRST || exit 1
		$TMUX send -t :inline -X "$command" || exit 1
		screen=$($TMUX capture-pane -t :inline -Mp) || exit 1
		case "$screen" in
		*SECOND*) echo "$keys $command changed the next fold"; exit 1 ;;
		esac
		case "$screen" in
		*FIRST*) shown=1 ;;
		*) shown=0 ;;
		esac
		expected=0
		if [ "$command" = expand-output ]; then
			expected=1
		fi
		[ "$shown" = "$expected" ] || exit 1
		$TMUX send -t :inline -X cancel || exit 1
	done
done

# C on the command line must not join commands or change their output ranges.
for keys in emacs vi; do
	$TMUX set -w -t :inline-start mode-keys "$keys" || exit 1
	$TMUX copy-mode -t :inline-start -c || exit 1
	$TMUX send -t :inline-start -X search-backward-text P0 || exit 1
	$TMUX set-buffer sentinel || exit 1
	$TMUX send -t :inline-start -X copy-output || exit 1
	[ "$($TMUX show-buffer)" = FIRST ] || exit 1
	$TMUX send -t :inline-start -X select-output || exit 1
	$TMUX send -t :inline-start -X copy-selection || exit 1
	[ "$($TMUX show-buffer)" = FIRST ] || exit 1
	case "$($TMUX capture-pane -t :inline-start -Mp)" in
	*SECOND*) echo "$keys select-output expanded another fold"; exit 1 ;;
	esac
	$TMUX send -t :inline-start -X cancel || exit 1
done

exit 0
