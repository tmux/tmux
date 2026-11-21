#!/bin/sh

. ./input-common.inc
LC_ALL=C.UTF-8
export LC_ALL
FORMAT='#{pane_program_state}|#{pane_program_name}|#{pane_program_message}'
REPLY=$(mktemp)
trap 'rm -f "$TMP" "$EXP" "$REPLY"; stop_server $TMUX || exit 1' 0 1 15

check_status()
{
	actual=$($TMUX display-message -p -t "$1" "$FORMAT") || exit 1
	if [ "$actual" != "$2" ]; then
		echo "FAIL: $1: expected <$2>, got <$actual>"
		exit_status=1
	fi
}

report()
{
	start_pane report 80 5 "\033]7501;$1\007"
	check_status report: "$2"
}

reject()
{
	start_pane reject 80 5 "\033]7501;state=idle:app=before:msg=b2s=\007\033]7501;$1\007"
	check_status reject: 'idle|before|ok'
}

query()
{
	start_cmd query 80 5 "stty raw -echo min 0 time 10; printf '\033]7501;?\007\033]7501;?\033\\\\'; printf '\033[5n'; dd bs=1 count=24 2>/dev/null >'$REPLY'; printf '\033]2;query-done\007'; $INPUT_HOLD"
	wait_for_output query-done $TMUX display-message -p -t query: '#{pane_title}' || exit 1
	printf '\033]7501;?\033\\\033]7501;?\033\\\033[0n' >"$EXP"
	cp "$REPLY" "$TMP"
	cmp -s "$TMP" "$EXP" || fail 'query replies'
	check_status query: '||'
	start_cmd query-abort 80 5 "stty raw -echo min 0 time 10; printf '\033]7501;state=done\007\033]7501;?\033x\033]7501;?:id=secret\007\033[5n'; dd bs=1 count=24 2>/dev/null >'$REPLY'; printf '\033]2;query-done\007'; $INPUT_HOLD"
	wait_for_output query-done $TMUX display-message -p -t query-abort: '#{pane_title}' || exit 1
	printf '\033]7501;?\033\\\033[0n' >"$EXP"
	cp "$REPLY" "$TMP"
	cmp -s "$TMP" "$EXP" || fail 'parsed queries'
	check_status query-abort: 'done||'
}

framing()
{
	for end in '\007' '\033\\'; do
		start_pane terminated 80 5 "\033]7501;state=working$end"
		check_status terminated: 'working||'
		start_pane long 80 5 "\033]7501;state=working:x=$(printf '%04096d' 0)$end"
		check_status long: 'working||'
	done
	# OSC handlers run on leaving OSC, including ESC and cancellation.
	# The shared parser also accepts leading zeroes and discards C0 bytes.
	for sequence in '\033]7501;state=done\030' '\033]7501;state=done\032' \
	    '\033]7501;state=done\033x' '\033]7501;state=done\033\000\\' \
	    '\033]07501;state=done\007' '\033]75\00001;state=done\007' \
	    '\033\000]7501;state=done\007'; do
		start_pane parsed 80 5 "\033]7501;state=idle\007$sequence"
		check_status parsed: 'done||'
	done
	start_cmd fragmented 80 5 "printf '\033]7501;state=idle\007\033]7501;state=done'; $TMUX wait-for fragment; printf '\033'; $TMUX wait-for terminator; printf '\\\\'; $INPUT_HOLD"
	printf '\\033]7501;state=done\n' >"$EXP"
	wait_until capture_matches_file "$EXP" "$TMP" $TMUX capture-pane -pPC -t fragmented: || exit 1
	check_status fragmented: 'idle||'
	$TMUX wait-for -S fragment
	printf '\\033]7501;state=done\\033\n' >"$EXP"
	wait_until capture_matches_file "$EXP" "$TMP" $TMUX capture-pane -pPC -t fragmented: || exit 1
	check_status fragmented: 'done||'
	$TMUX wait-for -S terminator
	wait_for_output 'done||' $TMUX display-message -p -t fragmented: "$FORMAT" || exit 1
}

selection()
{
	for id in 'id=child' 'id=' 'id=/invalid' ' id = child ' 'id' 'id=bad\000id'; do
		reject "state=done:$id"
		reject "state=clear:$id"
	done
}

replacement()
{
	for state in idle working done blocked error; do
		report "state=$state:app=clnkr:msg=aGVsbG8=" "$state|clnkr|hello"
	done
	start_pane replace 80 5 '\033]7501;state=working:app=clnkr:msg=aGVsbG8=\007\033]7501;state=done\007'
	check_status replace: 'done||'
	report 'state=working:state=done:app=first:app=last:msg=eA==:msg=eQ==' 'done|last|y'
	for body in 'app=clnkr' 'state=future' 'state=done:state=future'; do reject "$body"; done
	start_pane clear 80 5 '\033]7501;state=error:app=clnkr:msg=aGVsbG8=\007\033]7501;state=clear\007\033]7501;state=clear\007'
	check_status clear: '||'
}

validation()
{
	report ' state = working : app = clnkr : bad : =empty : x=y : state=blocked : kind=question ' 'blocked|clnkr|'
	report 'state=done:app=some/name:msg=aGVsbG8' 'done|some/name|hello'
	report 'state=done:app=cln\000kr:state=wo\011rking:msg=aGVsbG8=' 'working|clnkr|hello'
	report 'state=idle:kind=anything:title=AA==:progress=no' 'idle||'
	report 'state=done:msg=' 'done||'
	report 'state=done:msg=Cgk=' 'done||\n\t'
	report 'state=done:app=#(name):msg=Iyhtc2cp' 'done|_(name)|_(msg)'
	for body in 'state=working:msg=_' 'state=working:msg=A' 'state=working:msg=TQ=' \
	    'state=working:msg=AA==' 'state=working:msg=YQBi' 'state=working:msg=fw==' \
	    'state=working:msg=wK8=' 'state=working:msg=7aCA' 'state=working:msg=4pg=' \
	    'state=working:msg=AA==:msg=b2s='; do reject "$body"; done
	report 'state=working:abcdefghijklmnopq=x' 'working||'
	app=$(printf '%032d' 0)
	report "state=done:app=$app" "done|$app|"
	reject "state=done:app=${app}x"
	reject "state=working:app=${app}x:app=ok"
	decoded=$(printf '%2048s' '' | tr ' ' a)
	encoded=$(printf '%s' "$decoded" | base64 | tr -d '\n')
	report "state=done:msg=$encoded" "done||$decoded"
	encoded=$(printf '%sx' "$decoded" | base64 | tr -d '\n')
	reject "state=working:msg=$encoded:msg=b2s="
	reject "state=working:msg=$(printf '%02733d' 0):msg=b2s="
}

formats()
{
	start_pane empty 80 5 ''
	check_status empty: '||'
	report 'state=done:msg=4piDICJcIiAje3BhbmVfaWR9' 'done||☃ "\\" #{pane_id}'
}

lifecycle()
{
	for state in idle working done blocked error; do
		case $state in
		done|error) expected="$state|clnkr|hello" ;;
		*) expected='||' ;;
		esac
		start_pane prompt 80 5 "\033]7501;state=$state:app=clnkr:msg=aGVsbG8=\007\033]133;A\007\033]133;A\007"
		check_status prompt: "$expected"
		start_pane clear 80 5 "\033]7501;state=$state:app=clnkr:msg=aGVsbG8=\007\033c\033c"
		check_status clear: '||'
		start_pane respawn 80 5 "\033]7501;state=$state:app=clnkr:msg=aGVsbG8=\007"
		$TMUX respawn-pane -k -t respawn: "printf '\033]2;respawned\007'; $INPUT_HOLD" || exit 1
		wait_for_output respawned $TMUX display-message -p -t respawn: '#{pane_title}' || exit 1
		check_status respawn: '||'
	done
}

preservation()
{
	for sequence in '\033[?1049h\033[?1049l' '\033[!p' '\033]133;N\007'; do
		start_pane preserve 80 5 "\033]7501;state=working:app=clnkr:msg=aGVsbG8=\007$sequence"
		check_status preserve: 'working|clnkr|hello'
	done
	start_pane move 80 5 '\033]7501;state=done:app=clnkr\007'
	pane=$($TMUX display-message -p -t move: '#{pane_id}')
	$TMUX new-session -d -s other "$INPUT_HOLD" || exit 1
	$TMUX join-pane -d -s "$pane" -t other: || exit 1
	check_status "$pane" 'done|clnkr|'
	for state in idle working done blocked error; do
		start_pane dead 80 5 ''
		$TMUX set-option -w -t dead: remain-on-exit on || exit 1
		$TMUX set-option -g @dead-status pending || exit 1
		$TMUX set-hook -g pane-died "set-option -gF @dead-status '$FORMAT'" || exit 1
		$TMUX respawn-pane -k -t dead: "printf '\033]7501;state=$state:app=clnkr:msg=aGVsbG8=\007'" || exit 1
		wait_for_output 1 $TMUX display-message -p -t dead: '#{pane_dead}' || exit 1
		case $state in
		done|error) expected="$state|clnkr|hello" ;;
		*) expected='||' ;;
		esac
		check_status dead: "$expected"
		wait_for_output "$expected" $TMUX show-options -gv @dead-status || exit 1
	done
	$TMUX set-hook -gu pane-died || exit 1
}

independence()
{
	start_pane separate 80 5 '\033]7501;state=blocked:app=clnkr\007\033]9;4;1;42\007'
	check_status separate: 'blocked|clnkr|'
	wait_for_output 'normal|42' $TMUX display-message -p -t separate: '#{pane_pb_state}|#{pane_pb_progress}' || exit 1
	start_pane separate 80 5 '\033]9;4;1;42\007\033]7501;state=done\007'
	check_status separate: 'done||'
	wait_for_output 'normal|42' $TMUX display-message -p -t separate: '#{pane_pb_state}|#{pane_pb_progress}' || exit 1
	other=$($TMUX split-window -d -P -F '#{pane_id}' -t separate: "printf '\033]7501;state=error\007'; $INPUT_HOLD") || exit 1
	wait_for_output 'error||' $TMUX display-message -p -t "$other" "$FORMAT" || exit 1
	check_status separate:.0 'done||'
}


case ${1-all} in
all) groups='query framing selection replacement validation formats lifecycle preservation independence' ;;
query|framing|selection|replacement|validation|formats|lifecycle|preservation|independence) groups=$1 ;;
*) echo "unknown group: $1" >&2; exit 1 ;;
esac
for group in $groups; do
	$group || exit 1
	[ "$exit_status" -eq 0 ] || exit "$exit_status"
	echo "PASS: $group"
done
exit "$exit_status"
