#!/bin/sh

# Exercise customize-mode-specific mutations. mode-mutation.sh covers stale
# mode-tree data while customize-mode is open; this covers the actions unique to
# customize mode: adding, setting, editing, resetting, renaming array keys and
# unsetting options, hooks, environment variables and key bindings.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR

TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
OUT="$TEST_TMUX -LtestB$$ -f/dev/null"
FORMAT='CM #{?is_option,O,#{?is_environment,E,K}} #{option_name}#{environment_name}#{key}=#{option_value}#{environment_value}'

cleanup_servers()
{
	$TMUX kill-server 2>/dev/null
	$OUT kill-server 2>/dev/null
	sleep 0.5
}

cleanup()
{
	cleanup_servers
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "$1" >&2
	cleanup
	exit 1
}

capture()
{
	$OUT capture-pane -p -t out:0 2>/dev/null
}

settle()
{
	sleep 0.3
}

wait_clients()
{
	i=0
	while [ "$i" -lt 50 ]; do
		c=$($TMUX list-clients -F x 2>/dev/null | grep -c x)
		[ "$c" -eq "$1" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "expected $1 clients, have $c"
}

wait_mode()
{
	want=$1

	i=0
	while [ "$i" -lt 50 ]; do
		got=$($TMUX display-message -p -t cm:0 '#{pane_in_mode}' \
		    2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.2
		i=$((i + 1))
	done
	fail "pane mode state is $got, expected $want"
}

assert_alive()
{
	$TMUX display-message -p 'alive' >/dev/null 2>&1 ||
		fail "$1: server exited"
}

start_client()
{
	cleanup_servers
	$TMUX new-session -d -s cm -n main -x 80 -y 24 'cat' ||
		fail "new-session failed"
	$OUT new-session -d -s out -x 80 -y 24 "$TMUX attach -t cm" ||
		fail "outer client failed"
	wait_clients 1
}

open_customize()
{
	if [ "$#" -eq 0 ]; then
		$TMUX customize-mode -t cm:0 -F "$FORMAT" ||
			fail "customize-mode failed"
	else
		$TMUX customize-mode -t cm:0 -F "$FORMAT" -f "$1" ||
			fail "customize-mode failed"
	fi
	wait_mode 1
	settle
}

send()
{
	$TMUX send-keys -t cm:0 "$@" || fail "send-keys failed: $*"
}

send_literal()
{
	$TMUX send-keys -t cm:0 -l "$1" || fail "send literal failed: $1"
}

repeat_key()
{
	key=$1
	count=$2

	i=0
	while [ "$i" -lt "$count" ]; do
		send "$key"
		i=$((i + 1))
	done
}

replace_prompt()
{
	send C-u
	send_literal "$1"
	send Enter
	settle
}

assert_equals()
{
	got=$1
	want=$2
	what=$3

	[ "$got" = "$want" ] ||
		fail "$what: got '$got', expected '$want'"
}

assert_empty()
{
	got=$1
	what=$2

	[ -z "$got" ] || fail "$what: got '$got', expected empty"
}

assert_contains()
{
	text=$1
	needle=$2
	what=$3

	echo "$text" | grep -F -q "$needle" ||
		fail "$what: missing '$needle' in '$text'"
}

assert_not_contains()
{
	text=$1
	needle=$2
	what=$3

	echo "$text" | grep -F -q "$needle" &&
		fail "$what: found '$needle' in '$text'"
	return 0
}

test_user_option()
{
	start_client

	open_customize
	send Enter
	settle
	send_literal "cm_add added"
	send Enter
	settle
	assert_equals "$($TMUX show-options -sqv @cm_add)" "added" \
		"added server user option"

	send q
	wait_mode 0
	open_customize '#{==:#{option_name},@cm_add}'
	send Right j Enter
	settle
	replace_prompt "changed"
	assert_equals "$($TMUX show-options -sqv @cm_add)" "changed" \
		"set user option"

	send u y
	settle
	assert_empty "$($TMUX show-options -sqv @cm_add)" "unset user option"
	send q
	wait_mode 0
}

test_reset_window_pane_option()
{
	start_client

	$TMUX set-option -g window-style fg=red ||
		fail "set global window-style failed"
	$TMUX set-window-option -t cm:0 window-style fg=green ||
		fail "set window window-style failed"
	$TMUX set-option -p -t cm:0.0 window-style fg=blue ||
		fail "set pane window-style failed"

	open_customize '#{==:#{option_name},window-style}'
	send j j Right j
	settle
	send d y
	settle

	assert_equals "$($TMUX show-option -gqv window-style)" "default" \
		"reset global window-style"
	assert_empty "$($TMUX show-options -wqv -t cm:0 window-style)" \
		"reset window window-style"
	assert_empty "$($TMUX show-option -p -t cm:0.0 -qv window-style)" \
		"reset pane window-style"
	send q
	wait_mode 0
}

test_environment()
{
	start_client

	open_customize
	repeat_key j 6
	send Enter
	settle
	send_literal "CM_ENV=one"
	send Enter
	settle
	assert_equals "$($TMUX show-environment -t cm CM_ENV)" \
		"CM_ENV=one" "add session environment"

	send q
	wait_mode 0
	open_customize '#{==:#{environment_name},CM_ENV}'
	repeat_key j 6
	send Right j Enter
	settle
	replace_prompt "two"
	assert_equals "$($TMUX show-environment -t cm CM_ENV)" \
		"CM_ENV=two" "set session environment"

	send S
	settle
	replace_prompt "global"
	assert_equals "$($TMUX show-environment -g CM_ENV)" \
		"CM_ENV=global" "set global environment"

	send u y
	settle
	$TMUX show-environment -t cm CM_ENV >/dev/null 2>&1 &&
		fail "unset session environment: variable still exists"
	send q
	wait_mode 0
}

test_user_hook()
{
	start_client

	open_customize
	repeat_key j 3
	send Enter
	settle
	send_literal "cm_hook display-message hook"
	send Enter
	settle
	assert_equals "$($TMUX show-option -qv @cm_hook)" \
		"display-message hook" "add user hook"

	send q
	wait_mode 0
	open_customize '#{==:#{option_name},@cm_hook}'
	repeat_key j 3
	send Right j
	settle
	assert_contains "$(capture)" "@cm_hook" "user hook shown as hook"

	send u y
	settle
	assert_empty "$($TMUX show-option -qv @cm_hook)" "unset user hook"
	send q
	wait_mode 0
}

test_hook_array()
{
	start_client

	$TMUX set-hook -g 'after-new-session[0]' 'display-message old' ||
		fail "set hook array failed"

	open_customize '#{==:#{option_name},after-new-session}'
	repeat_key j 3
	send Right j Right j
	settle

	send a
	settle
	replace_prompt "1"
	hooks=$($TMUX show-hooks -g after-new-session)
	assert_contains "$hooks" "after-new-session[1] display-message old" \
		"renamed hook array key"
	assert_not_contains "$hooks" "after-new-session[0]" \
		"old hook array key removed"

	send Enter
	settle
	replace_prompt "display-message new"
	hooks=$($TMUX show-hooks -g after-new-session)
	assert_contains "$hooks" "after-new-session[1] display-message new" \
		"set hook array value"

	send u y
	settle
	hooks=$($TMUX show-hooks -g after-new-session)
	assert_not_contains "$hooks" "after-new-session[1]" \
		"unset hook array item"
	send q
	wait_mode 0
}

test_changed_only()
{
	start_client

	$TMUX set-option -s @cm_changed yes ||
		fail "set changed server option failed"
	$TMUX bind-key -T prefix User1 display-message changed-only ||
		fail "set changed key failed"

	open_customize
	send C
	settle
	send M-+
	settle
	screen=$(capture)
	assert_contains "$screen" "@cm_changed" "changed-only user option"
	assert_contains "$screen" "User1" "changed-only key"
	assert_not_contains "$screen" "Global Environment" \
		"changed-only hides environment"
	send q
	wait_mode 0
}

test_reset_key()
{
	start_client

	orig=$($TMUX list-keys -T prefix -F '#{key_command}' c) ||
		fail "default c binding not found"
	$TMUX bind-key -T prefix c display-message changed ||
		fail "change c binding failed"

	open_customize '#{==:#{key},c}'
	repeat_key j 10
	send Right j d y
	settle
	after=$($TMUX list-keys -T prefix -F '#{key_command}' c) ||
		fail "reset c binding not found"
	assert_equals "$after" "$orig" "reset key binding"
	send q
	wait_mode 0
}

test_tagged_unset()
{
	start_client

	$TMUX set-option -s @cm_tag_a one || fail "set tag option a failed"
	$TMUX set-option -s @cm_tag_b two || fail "set tag option b failed"

	open_customize '#{m/r:^@cm_tag_,#{option_name}}'
	send Right j t j t U y
	settle
	assert_empty "$($TMUX show-options -sqv @cm_tag_a)" \
		"tagged unset option a"
	assert_empty "$($TMUX show-options -sqv @cm_tag_b)" \
		"tagged unset option b"
	send q
	wait_mode 0
}

test_key_binding()
{
	start_client

	open_customize
	repeat_key j 10
	send Enter
	settle
	send_literal "User0 display-message before"
	send Enter
	settle
	keys=$($TMUX list-keys -T prefix User0 2>/dev/null) ||
		fail "added key binding not found"
	assert_contains "$keys" "display-message before" "add key binding"

	send q
	wait_mode 0
	open_customize '#{==:#{key},User0}'
	repeat_key j 10
	send Right j Right j
	settle

	send s
	settle
	replace_prompt "display-message after"
	keys=$($TMUX list-keys -T prefix User0 2>/dev/null) ||
		fail "edited key binding not found"
	assert_contains "$keys" "display-message after" "set key command"

	send j s
	settle
	replace_prompt "custom note"
	note=$($TMUX list-keys -T prefix -F '#{key_note}' User0)
	assert_equals "$note" "custom note" "set key note"

	send j s
	settle
	keys=$($TMUX list-keys -T prefix User0 2>/dev/null) ||
		fail "repeat key binding not found"
	assert_contains "$keys" "bind-key -r" "toggle key repeat"

	send u y
	settle
	$TMUX list-keys -T prefix User0 >/dev/null 2>&1 &&
		fail "unset key binding: binding still exists"
	send q
	wait_mode 0
}

test_editor()
{
	start_client

	editor="$TMP/editor.sh"
	editor_value="$TMP/editor-value"
	{
		echo '#!/bin/sh'
		printf 'cat "%s" > "$1"\n' "$editor_value"
	} > "$editor" || fail "write editor helper failed"
	chmod +x "$editor" || fail "chmod editor helper failed"

	$TMUX set-option -g editor "$editor" || fail "set editor failed"
	$TMUX set-option -g @cm_edit old || fail "set edit option failed"

	printf 'edited' > "$editor_value" || fail "write editor value failed"
	open_customize '#{==:#{option_name},@cm_edit}'
	send j Right j e

	i=0
	while [ "$i" -lt 50 ]; do
		value=$($TMUX show-option -gqv @cm_edit)
		[ "$value" = "edited" ] && break
		sleep 0.2
		i=$((i + 1))
	done
	assert_equals "$value" "edited" "edit option in editor"
	send q
	wait_mode 0

	$TMUX bind-key -T prefix User2 display-message old ||
		fail "set editor key failed"
	printf 'display-message edited' > "$editor_value" ||
		fail "write editor key command failed"
	open_customize '#{==:#{key},User2}'
	repeat_key j 10
	send Right j Right j e
	i=0
	while [ "$i" -lt 50 ]; do
		value=$($TMUX list-keys -T prefix -F '#{key_command}' User2)
		[ "$value" = "display-message edited" ] && break
		sleep 0.2
		i=$((i + 1))
	done
	assert_equals "$value" "display-message edited" \
		"edit key command in editor"

	printf 'edited note' > "$editor_value" ||
		fail "write editor key note failed"
	send j e
	i=0
	while [ "$i" -lt 50 ]; do
		value=$($TMUX list-keys -T prefix -F '#{key_note}' User2)
		[ "$value" = "edited note" ] && break
		sleep 0.2
		i=$((i + 1))
	done
	assert_equals "$value" "edited note" "edit key note in editor"
	send q
	wait_mode 0

	$TMUX set-environment -t cm CM_EDIT_ENV old ||
		fail "set editor environment failed"
	printf 'env-edited' > "$editor_value" ||
		fail "write editor environment failed"
	open_customize '#{==:#{environment_name},CM_EDIT_ENV}'
	repeat_key j 6
	send Right j e
	i=0
	while [ "$i" -lt 50 ]; do
		value=$($TMUX show-environment -t cm CM_EDIT_ENV)
		[ "$value" = "CM_EDIT_ENV=env-edited" ] && break
		sleep 0.2
		i=$((i + 1))
	done
	assert_equals "$value" "CM_EDIT_ENV=env-edited" \
		"edit environment in editor"
	send q
	wait_mode 0
}

test_stale_current_items()
{
	start_client

	$TMUX set-option -s @cm_stale_option old ||
		fail "set stale option failed"
	open_customize '#{==:#{option_name},@cm_stale_option}'
	send Right j
	settle
	$TMUX set-option -su @cm_stale_option ||
		fail "external unset stale option failed"
	send Enter
	send u y
	send d y
	settle
	assert_alive "stale option current item"
	send q
	wait_mode 0

	$TMUX set-environment -t cm CM_STALE_ENV old ||
		fail "set stale environment failed"
	open_customize '#{==:#{environment_name},CM_STALE_ENV}'
	repeat_key j 6
	send Right j
	settle
	$TMUX set-environment -t cm -u CM_STALE_ENV ||
		fail "external unset stale environment failed"
	send Enter
	send S
	send u y
	settle
	assert_alive "stale environment current item"
	send q
	wait_mode 0

	$TMUX bind-key -T prefix User3 display-message stale ||
		fail "set stale key failed"
	open_customize '#{==:#{key},User3}'
	repeat_key j 10
	send Right j
	settle
	$TMUX unbind-key -T prefix User3 ||
		fail "external unbind stale key failed"
	send s
	send d y
	send u y
	settle
	assert_alive "stale key current item"
	send q
	wait_mode 0
}

test_prompt_invalidation()
{
	start_client

	$TMUX set-option -s @cm_prompt_stale old ||
		fail "set prompt stale option failed"
	open_customize '#{==:#{option_name},@cm_prompt_stale}'
	send Right j Enter
	settle
	$TMUX set-option -su @cm_prompt_stale ||
		fail "external unset prompt option failed"
	replace_prompt "new"
	assert_empty "$($TMUX show-options -sqv @cm_prompt_stale)" \
		"prompt stale option should not be recreated"
	assert_alive "stale option prompt"
	send q
	wait_mode 0

	$TMUX bind-key -T prefix User4 display-message old ||
		fail "set prompt stale key failed"
	open_customize '#{==:#{key},User4}'
	repeat_key j 10
	send Right j Right j s
	settle
	$TMUX unbind-key -T prefix User4 ||
		fail "external unbind prompt key failed"
	replace_prompt "display-message new"
	$TMUX list-keys -T prefix User4 >/dev/null 2>&1 &&
		fail "prompt stale key should stay unbound"
	assert_alive "stale key prompt"
	send q
	wait_mode 0

	$TMUX set-hook -g 'after-new-session[0]' 'display-message old' ||
		fail "set prompt stale hook array failed"
	$TMUX set-hook -g 'after-new-session[1]' 'display-message taken' ||
		fail "set prompt stale hook array target failed"
	open_customize '#{==:#{option_name},after-new-session}'
	repeat_key j 3
	send Right j Right j a
	settle
	replace_prompt "1"
	hooks=$($TMUX show-hooks -g after-new-session)
	assert_contains "$hooks" "after-new-session[0] display-message old" \
		"array rename to existing key keeps old key"
	assert_contains "$hooks" "after-new-session[1] display-message taken" \
		"array rename to existing key keeps target key"
	assert_alive "existing array key prompt"
	send q
	wait_mode 0
}

test_editor_invalidation()
{
	start_client

	editor="$TMP/slow-editor.sh"
	{
		echo '#!/bin/sh'
		echo 'sleep 1'
		echo 'printf edited > "$1"'
	} > "$editor" || fail "write slow editor helper failed"
	chmod +x "$editor" || fail "chmod slow editor helper failed"
	$TMUX set-option -g editor "$editor" || fail "set slow editor failed"

	$TMUX set-option -g @cm_editor_stale old ||
		fail "set editor stale option failed"
	open_customize '#{==:#{option_name},@cm_editor_stale}'
	send j Right j e
	settle
	$TMUX set-option -gu @cm_editor_stale ||
		fail "external unset editor option failed"
	sleep 2
	assert_empty "$($TMUX show-option -gqv @cm_editor_stale)" \
		"editor stale option should not be recreated"
	assert_alive "stale option editor"
	send q
	wait_mode 0

	$TMUX bind-key -T prefix User5 display-message old ||
		fail "set editor stale key failed"
	open_customize '#{==:#{key},User5}'
	repeat_key j 10
	send Right j Right j e
	settle
	$TMUX unbind-key -T prefix User5 ||
		fail "external unbind editor key failed"
	sleep 2
	$TMUX list-keys -T prefix User5 >/dev/null 2>&1 &&
		fail "editor stale key should stay unbound"
	assert_alive "stale key editor"
	send q
	wait_mode 0

	$TMUX set-environment -t cm CM_EDITOR_STALE old ||
		fail "set editor stale environment failed"
	open_customize '#{==:#{environment_name},CM_EDITOR_STALE}'
	repeat_key j 6
	send Right j e
	settle
	$TMUX set-environment -t cm -u CM_EDITOR_STALE ||
		fail "external unset editor environment failed"
	sleep 2
	assert_equals "$($TMUX show-environment -t cm CM_EDITOR_STALE)" \
		"CM_EDITOR_STALE=edited" "editor stale environment is reset"
	assert_alive "stale environment editor"
	send q
	wait_mode 0
}

test_invalid_prompts()
{
	start_client

	open_customize
	send Enter
	settle
	send_literal "bad value"
	send Enter
	settle
	assert_empty "$($TMUX show-options -sqv bad)" "invalid user option name"
	assert_alive "invalid user option prompt"
	send q
	wait_mode 0

	open_customize
	repeat_key j 10
	send Enter
	settle
	send_literal "NoSuchKey display-message invalid"
	send Enter
	settle
	assert_alive "invalid key prompt"
	send q
	wait_mode 0

	$TMUX bind-key -T prefix User6 display-message valid ||
		fail "set invalid command key failed"
	open_customize '#{==:#{key},User6}'
	repeat_key j 10
	send Right j Right j s
	settle
	replace_prompt "not-a-command -Z"
	keys=$($TMUX list-keys -T prefix -F '#{key_command}' User6)
	assert_equals "$keys" "display-message valid" \
		"invalid key command keeps old command"
	assert_alive "invalid key command prompt"
	send q
	wait_mode 0
}

cleanup_servers
test_user_option
test_reset_window_pane_option
test_environment
test_user_hook
test_hook_array
test_changed_only
test_reset_key
test_tagged_unset
test_key_binding
test_editor
test_stale_current_items
test_prompt_invalidation
test_editor_invalidation
test_invalid_prompts
cleanup
exit 0
