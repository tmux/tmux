#!/bin/sh
# Exercise format/style rendering in live contexts.  These paths use
# format_draw rather than plain format expansion, so they have historically
# found different crashes and runaway output.

PATH=/bin:/usr/bin
TERM=screen
LANG=C.UTF-8
LC_ALL=C.UTF-8
export TERM LANG LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)

TMUX="$TEST_TMUX -Ltest -f/dev/null"
TMUX2="$TEST_TMUX -Ltest2 -f/dev/null"
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
	name="$1"
	text="$2"
	n=$(printf '%s' "$text" | wc -c | tr -d ' ')

	[ "$n" -le "$LIMIT" ] || fail "$name produced $n bytes"
}

tmux_run()
{
	name="$1"
	shift

	out=$(run_cmd $TMUX "$@" 2>&1)
	rc=$?
	bounded "$name" "$out"
	[ "$rc" -eq 0 ] || fail "$name failed ($rc): $out"

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
	out=$(run_cmd $TMUX2 capture-pane -Cep -t out:0 2>/dev/null)
	rc=$?
	bounded "capture-pane -e" "$out"
	[ "$rc" -eq 0 ] || fail "capture-pane -e failed"
	printf '%s\n' "$out"
}

assert_alive()
{
	out=$(tmux_run "server alive ($1)" display-message -p alive)
	[ "$out" = "alive" ] || fail "server not alive after $1"
}

wait_for()
{
	marker="$1"
	i=0
	while [ "$i" -lt 50 ]; do
		if capture | grep -q "$marker"; then
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
	label="$1"
	fmt="$2"

	tmux_run "$label status-left" \
		set-option -g status-left "SL$label: $fmt" >/dev/null
	tmux_run "$label status-right" \
		set-option -g status-right "SR$label: $fmt" >/dev/null
	tmux_run "$label status-format left/right" \
		set-option -g status-format[0] \
		'#{E:status-left}#[align=right]#{E:status-right}' >/dev/null
	wait_for "SL$label"
	assert_alive "$label status-left/right"
}

render_status_format()
{
	label="$1"
	fmt="$2"

	tmux_run "$label clear status-left" set-option -g status-left '' >/dev/null
	tmux_run "$label clear status-right" set-option -g status-right '' >/dev/null
	tmux_run "$label status-format" \
		set-option -g status-format[0] "SF$label: $fmt" >/dev/null
	wait_for "SF$label"
	assert_alive "$label status-format"
}

render_message()
{
	label="$1"
	fmt="$2"

	tmux_run "$label display-message" \
		display-message -t fmt:0 -d 1000 "DM$label: $fmt" >/dev/null
	wait_for "DM$label"
	assert_alive "$label display-message"
}

render_choose_tree()
{
	label="$1"
	fmt="$2"

	tmux_run "$label choose-tree" \
		choose-tree -t fmt:0 -F "CT$label: $fmt" >/dev/null
	wait_for "CT$label"
	tmux_run "$label quit choose-tree" send-keys -t fmt:0 q >/dev/null
	assert_alive "$label choose-tree"
}

render_customize()
{
	label="$1"
	fmt="$2"

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
	label="$1"
	fmt="$2"

	tmux_run "$label bind-key" \
		bind-key -T root F12 display-message "LK$label: $fmt" >/dev/null
	out=$(tmux_run "$label list-keys" list-keys -T root F12)
	bounded "$label list-keys output" "$out"

	tmux_run "$label option value" \
		set-option -g status-left "LO$label: $fmt" >/dev/null
	out=$(tmux_run "$label list-options" show-options -g status-left)
	bounded "$label list-options output" "$out"

	assert_alive "$label list-keys/list-options"
}

check_sgr_sanity()
{
	tmux_run "sgr status style" \
		set-option -g status-style fg=default,bg=default >/dev/null
	tmux_run "sgr status-format" \
		set-option -g status-format[0] \
		'#[fg=colour1,bg=default]SGR-STATUS#[default]' >/dev/null
	wait_for "SGR-STATUS"
	out=$(capture_esc | tail -n 1)
	bounded "sgr status capture" "$out"
	case "$out" in
		*SGR-STATUS*) ;;
		*) fail "status SGR capture lost text: $out" ;;
	esac
	case "$out" in
		*"\\033["*) ;;
		*) fail "status SGR capture has no escape reset/style data: $out" ;;
	esac

	tmux_run "sgr message style" \
		set-option -g message-style fg=colour2,bg=default >/dev/null
	tmux_run "sgr display-message" \
		display-message -t fmt:0 -d 1000 \
		'#[bg=default]SGR-MESSAGE#[default]' >/dev/null
	wait_for "SGR-MESSAGE"
	out=$(capture_esc | tail -n 1)
	bounded "sgr message capture" "$out"
	case "$out" in
		*SGR-MESSAGE*) ;;
		*) fail "message SGR capture lost text: $out" ;;
	esac
	case "$out" in
		*"\\033["*) ;;
		*) fail "message SGR capture has no escape reset/style data: $out" ;;
	esac

	assert_alive "SGR sanity"
}

run_corpus()
{
	label="$1"
	fmt="$2"

	render_status_options "$label" "$fmt"
	render_status_format "$label" "$fmt"
	render_message "$label" "$fmt"
	render_choose_tree "$label" "$fmt"
	render_customize "$label" "$fmt"
	render_list_output "$label" "$fmt"
}

trap cleanup 0 1 15
cleanup

tmux_run "new inner session" \
	new-session -d -s fmt -x 100 -y 30 "exec sleep 1000" >/dev/null
tmux_run "new inner window" \
	new-window -t fmt -n second "exec sleep 1000" >/dev/null
tmux_run "select visible window" select-window -t fmt:0 >/dev/null
tmux_run "set status interval" set-option -g status-interval 1 >/dev/null
tmux_run "set base message style" \
	set-option -g message-style fg=default,bg=default >/dev/null
tmux_run "set nested option" \
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

run_corpus A \
	'#[bold,dim,underscore,blink,reverse,italics,strikethrough,double-underscore,curly-underscore,dotted-underscore,dashed-underscore,overline,fg=colour196,bg=colour22,us=colour45]LONGSTYLE#[default]plain'
run_corpus B \
	'wide: Ελληνικά 中文 😀 #{=12:中文😀abc}'
run_corpus C \
	'#[fg=colour2,bg=colour4]base#[push-default]#[fg=colour1,bg=default]push#[pop-default]pop#[default]'
run_corpus D \
	'#[list=on]list#[list=focus]focus#[nolist] #[align=centre]centre#[noalign] #[range=left]L#[range=right]R#[norange]'
run_corpus E \
	'#{E:@nested}:#{T:%%H:%%M}:#{?#{==:#{session_name},fmt},yes,no}:#{?0,bad,}'
run_corpus F \
	'job:#(printf job-ok)'
run_corpus G \
	'empty:#{E:}:#{T:}:#{definitely_not_a_variable}:#{?0,no,}'

check_sgr_sanity

cleanup
exit 0
