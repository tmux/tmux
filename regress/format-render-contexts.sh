#!/bin/sh
# Exercise format and style rendering in live contexts.

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export PATH TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMUX="$TEST_TMUX -Lformat-render-contexts-$$ -f/dev/null"
TMUX2="$TEST_TMUX -Lformat-render-contexts-outer-$$ -f/dev/null"
LIMIT=20000

cleanup()
{
	$TMUX kill-server 2>/dev/null
	$TMUX2 kill-server 2>/dev/null
}

fail()
{
	echo "$1" >&2
	cleanup
	exit 1
}

run_cmd()
{
	if command -v timeout >/dev/null 2>&1; then
		timeout 10 "$@"
	else
		"$@"
	fi
}

bounded()
{
	name=$1
	text=$2

	n=$(printf '%s' "$text" | wc -c)
	[ "$n" -le "$LIMIT" ] || fail "$name too large: $n bytes"
}

tmux_run()
{
	name=$1
	shift

	out=$(run_cmd $TMUX "$@" 2>&1)
	rc=$?
	bounded "$name" "$out"
	[ "$rc" -eq 0 ] || fail "$name failed: $out"
	printf '%s' "$out"
}

capture()
{
	out=$(run_cmd $TMUX2 capture-pane -p -t out:0 2>/dev/null)
	rc=$?
	bounded "capture-pane" "$out"
	[ "$rc" -eq 0 ] || fail "capture-pane failed"
	printf '%s\n' "$out"
}

capture_esc()
{
	out=$(run_cmd $TMUX2 capture-pane -pe -t out:0 2>/dev/null)
	rc=$?
	bounded "capture-pane -e" "$out"
	[ "$rc" -eq 0 ] || fail "capture-pane -e failed"
	printf '%s\n' "$out"
}

assert_alive()
{
	tmux_run "server alive ($1)" display-message -p alive >/dev/null
}

wait_for()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		if capture | grep -F "$marker" >/dev/null 2>&1; then
			sleep 0.1
			return 0
		fi
		sleep 0.2
		i=$((i + 1))
	done
	fail "timed out waiting for $marker"
}

render_status_options()
{
	label=$1
	fmt=$2

	tmux_run "$label status-left" \
		set-option -g status-left "SL$label: $fmt" >/dev/null
	tmux_run "$label clear status-right" \
		set-option -g status-right '' >/dev/null
	tmux_run "$label status-format left" \
		set-option -g status-format[0] '#{E:status-left}' >/dev/null
	tmux_run "$label refresh status-left" refresh-client -S >/dev/null
	wait_for "SL$label:"

	tmux_run "$label clear status-left" \
		set-option -g status-left '' >/dev/null
	tmux_run "$label status-right" \
		set-option -g status-right "SR$label: $fmt" >/dev/null
	tmux_run "$label status-format right" \
		set-option -g status-format[0] \
		'#[align=right]#{E:status-right}' >/dev/null
	tmux_run "$label refresh status-right" refresh-client -S >/dev/null
	wait_for "SR$label:"
	assert_alive "$label status-left/right"
}

render_status_format()
{
	label=$1
	fmt=$2

	tmux_run "$label status-format" \
		set-option -g status-format[0] "SF$label: $fmt" >/dev/null
	tmux_run "$label refresh status-format" refresh-client -S >/dev/null
	wait_for "SF$label:"
	assert_alive "$label status-format"
}

render_message()
{
	label=$1
	fmt=$2

	tmux_run "$label display-message" \
		display-message -t fmt:0 -d 1000 "DM$label: $fmt" >/dev/null
	wait_for "DM$label:"
	assert_alive "$label display-message"
}

render_choose_tree()
{
	label=$1
	fmt=$2

	tmux_run "$label choose-tree" \
		choose-tree -t fmt:0 -F "CT$label: $fmt" >/dev/null
	wait_for "CT$label:"
	tmux_run "$label quit choose-tree" send-keys -t fmt:0 q >/dev/null
	assert_alive "$label choose-tree"
}

render_customize()
{
	label=$1
	fmt=$2

	tmux_run "$label customize-mode" \
		customize-mode -t fmt:0 -F "CM$label: $fmt" >/dev/null
	sleep 0.5
	out=$(capture)
	bounded "$label customize-mode capture" "$out"
	tmux_run "$label quit customize-mode" send-keys -t fmt:0 q >/dev/null
	assert_alive "$label customize-mode"
}

render_list_output()
{
	label=$1
	fmt=$2

	tmux_run "$label bind-key" \
		bind-key -T root F12 display-message "LK$label: $fmt" >/dev/null
	out=$(tmux_run "$label list-keys" list-keys -T root F12)
	bounded "$label list-keys" "$out"
	printf '%s' "$out" | grep -F "LK$label:" >/dev/null 2>&1 ||
		fail "$label list-keys missing rendered command"

	tmux_run "$label set show option" \
		set-option -g "@format_render_$label" "SO$label: $fmt" >/dev/null
	out=$(tmux_run "$label show-options" show-options -g "@format_render_$label")
	bounded "$label show-options" "$out"
	printf '%s' "$out" | grep -F "SO$label:" >/dev/null 2>&1 ||
		fail "$label show-options missing option"

	assert_alive "$label list output"
}

run_corpus()
{
	label=$1
	fmt=$2

	render_status_options "$label" "$fmt"
	render_status_format "$label" "$fmt"
	render_message "$label" "$fmt"
	render_choose_tree "$label" "$fmt"
	render_customize "$label" "$fmt"
	render_list_output "$label" "$fmt"
}

check_sgr_sanity()
{
	esc=$(printf '\033')

	tmux_run "set message style fill" \
		set-option -g message-style \
		"fg=#080808,bg=#ffff00,fill=#ffff00,bold" >/dev/null
	tmux_run "show filled message" \
		display-message -t fmt:0 -d 1000 \
		'#[bg=blue,italics] hello, #[fg=#080808,bg=default] world!' \
		>/dev/null
	wait_for "hello,  world!"

	out=$(capture_esc)
	printf '%s' "$out" | grep -F 'world!' >/dev/null 2>&1 ||
		fail "message text missing from SGR capture"
	printf '%s' "$out" | grep "$esc" >/dev/null 2>&1 ||
		fail "SGR capture has no escapes"

	world_sgr=$(
		printf '%s\n' "$out" |
		awk -v esc="$esc" '
		/world!/ {
			i = index($0, "world!")
			pre = substr($0, 1, i - 1)
			n = split(pre, parts, esc "\\[")
			print parts[n]
			exit
		}'
	)

	case "$world_sgr" in
	*'48;'*) ;;
	*) fail "world segment missing explicit background SGR: $world_sgr" ;;
	esac
	case "$world_sgr" in
	*'49'*)
		fail "world segment used default background instead of message fill: $world_sgr"
		;;
	esac

	assert_alive "message SGR sanity"
}

wide_fmt()
{
	printf 'wide: \0316\0225\0316\0273\0316\0273\0316\0267\0316\0275\0316\0271\0316\0272\0316\0254 \0344\0270\0255\0346\0226\0207 #{=12:\0344\0270\0255\0346\0226\0207abc}'
}

long_style_fmt()
{
	printf '%s' '#[fg=colour1,bg=colour2,us=colour3,acs,bright,dim,underscore,blink,reverse,hidden,italics,strikethrough,double-underscore,curly-underscore,dotted-underscore,dashed-underscore,overline,range=user|aaaaaaaaaaaaaaaa,align=absolute-centre,list=on,fill=colour200,width=4294967295,pad=4294967295]OVERLONG-STYLE#[default]'
}

trap cleanup 0 1 15
cleanup

tmux_run "new inner session" \
	new-session -d -s fmt -x 100 -y 30 "exec sleep 1000" >/dev/null
tmux_run "new inner window" \
	new-window -t fmt -n second "exec sleep 1000" >/dev/null
tmux_run "select first window" select-window -t fmt:0 >/dev/null
tmux_run "status interval" set-option -g status-interval 1 >/dev/null
tmux_run "base status" set-option -g status on >/dev/null
tmux_run "nested option" \
	set-option -g @nested 'NESTED-#{session_name}-#{window_index}' >/dev/null

run_cmd $TMUX2 new-session -d -s out -x 100 -y 30 "$TMUX attach -t fmt" \
	>/dev/null 2>&1 || fail "failed to start outer client"

i=0
while [ "$i" -lt 50 ]; do
	c=$(tmux_run "wait clients" list-clients -F x | grep -c x)
	[ "$c" -eq 1 ] && break
	sleep 0.2
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "inner client did not attach"

run_corpus A 'plain #{session_name}:#{window_index} #{@nested}'
run_corpus B "$(wide_fmt)"
run_corpus C '#[fg=colour10,bg=colour17,bold,italics]styled #{pane_current_command}#[default]'
run_corpus D '#[push-default]#[fg=red,bg=blue]push #{?pane_active,active,inactive}#[pop-default] after'
run_corpus E "$(long_style_fmt)"

check_sgr_sanity

cleanup
exit 0
