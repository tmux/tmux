#!/bin/sh

. ./input-common.inc

for sync in off on; do
	begin=
	end=
	if [ "$sync" = on ]; then
		begin='\033[?2026h'
		end='\033[?2026l'
	fi
	for op in L M; do
		for row in 1 5; do
			name="lines-$sync-$op-$row"
			start_pane "$name" 8 6 \
			    "111\033[2;1H222\033[3;1H333\033[4;1H444\033[5;1H555\033[6;1H666${begin}\033[2;4r\033[$row;3H\033[2$op\033[r${end}"
			check_capture "$name" '111
222
333
444
555
666'
		done
		for row in 2 4; do
			name="lines-$sync-$op-margin-$row"
			start_pane "$name" 8 6 \
			    "111\033[2;1H222\033[3;1H333\033[4;1H444\033[5;1H555\033[6;1H666${begin}\033[2;4r\033[$row;1H\033[$op\033[r${end}"
			case "$op-$row" in
			L-2) expected='111

222
333
555
666' ;;
			M-2) expected='111
333
444

555
666' ;;
			*-4) expected='111
222
333

555
666' ;;
			esac
			check_capture "$name" "$expected"
		done
		name="lines-$sync-$op-clear"
		start_pane "$name" 8 6 \
		    "111\033[2;1H222\033[3;1H333\033[4;1H444\033[5;1H555\033[6;1H666${begin}\033[2;4r\033[2;1H\033[2$op\033[r${end}"
		if [ "$op" = L ]; then
			expected='111


222
555
666'
		else
			expected='111
444


555
666'
		fi
		check_capture "$name" "$expected"
		name="lines-$sync-$op-clamp"
		start_pane "$name" 8 6 \
		    "111\033[2;1H222\033[3;1H333\033[4;1H444\033[5;1H555\033[6;1H666${begin}\033[2;4r\033[3;1H\033[99$op\033[r${end}"
		check_capture "$name" '111
222


555
666'
	done
done

$TMUX kill-server 2>/dev/null
exit $exit_status
