#!/bin/sh

# Exercise cmd_template_replace through command-prompt, which passes prompt
# input as argv to args_make_commands. This covers the quoting modes, indexed
# replacements, and cases where replacements are intentionally not made.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
LANG=C.UTF-8
export TERM LC_ALL LANG

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMP=$(mktemp -d) || exit 1
TMUX_TMPDIR="$TMP"
export TMUX_TMPDIR

OUT="$TEST_TMUX -LtestA$$ -f/dev/null"
IN="$TEST_TMUX -LtestB$$ -f/dev/null"

cleanup()
{
	$OUT kill-server 2>/dev/null
	$IN kill-server 2>/dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT

fail()
{
	echo "[FAIL] $1"
	exit 1
}

settle()
{
	sleep 0.5
}

wait_option()
{
	option=$1
	expected=$2
	i=0

	while [ "$i" -lt 30 ]; do
		value=$($IN show -gqv "$option" 2>/dev/null || true)
		[ "$value" = "$expected" ] && return 0
		i=$((i + 1))
		sleep 0.2
	done
	fail "expected $option to be '$expected' but got '$value'"
}

accept_prompt()
{
	key=$1
	value=$2

	$OUT send-keys "$key" || exit 1
	settle
	$OUT send-keys -l "$value" || exit 1
	$OUT send-keys Enter || exit 1
	settle
}

accept_two_prompts()
{
	key=$1
	first=$2
	second=$3

	$OUT send-keys "$key" || exit 1
	settle
	$OUT send-keys -l "$first" || exit 1
	$OUT send-keys Enter || exit 1
	settle
	$OUT send-keys -l "$second" || exit 1
	$OUT send-keys Enter || exit 1
	settle
}

reset_options()
{
	for option in \
	    @r @first @second @plain @raw_tail @one @two @two_again \
	    @double_index @marker; do
		$IN set -g "$option" SENTINEL || exit 1
	done
	$IN set -g @marker unchanged || exit 1
}

$IN new -d -x80 -y24 "sh -c 'exec sleep 1000'" || exit 1
$IN set -g status on || exit 1
$IN set -g status-keys emacs || exit 1
$IN set -g window-size manual || exit 1

$IN bind -n M-s command-prompt -p '(single)' "set -g @r '%%'" ||
	exit 1
$IN bind -n M-f command-prompt -p '(first)' \
	"set -g @first '%%' ; set -g @second '%%'" ||
	exit 1
$IN bind -n M-d command-prompt -p '(double)' 'set -g @r "%%%"' ||
	exit 1
$IN bind -n M-r command-prompt -p '(raw)' "set -g @r %1" ||
	exit 1
$IN bind -n M-a command-prompt -p '(all)' \
	"set -g @first %1 ; set -g @second %1" ||
	exit 1
$IN bind -n M-m command-prompt -p 'one,two' \
	"set -g @one %1 ; set -g @two %2 ; set -g @two_again %2" ||
	exit 1
$IN bind -n M-i command-prompt -p 'one,two' 'set -g @double_index "%2%"' ||
	exit 1
$IN bind -n M-n command-prompt -p '(none)' \
	"set -g @plain no-template-markers" ||
	exit 1

$OUT new -d -x80 -y24 || exit 1
$OUT set -g status off || exit 1
$OUT set -g window-size manual || exit 1
$OUT send-keys -l "$IN attach" || exit 1
$OUT send-keys Enter || exit 1
sleep 1

reset_options

# %% is for templates already inside single quotes. A single quote in the
# replacement must stay data and must not close the surrounding quotes.
payload="can't ; set -g @marker changed ; done"
accept_prompt M-s "$payload"
wait_option @r "$payload"
wait_option @marker unchanged

# Only the first %% is replaced.
reset_options
payload="first'value"
accept_prompt M-f "$payload"
wait_option @first "$payload"
wait_option @second %%

# %%% keeps the previous double-quote escaping behaviour.
reset_options
payload='a"$;~\z'
accept_prompt M-d "$payload"
wait_option @r "$payload"

# %1 is intentionally left raw as an escape hatch.
reset_options
$IN set -g @raw_tail unchanged || exit 1
accept_prompt M-r 'raw ; set -g @raw_tail yes'
wait_option @r raw
wait_option @raw_tail yes

# All instances of %1 are replaced.
reset_options
accept_prompt M-a repeat
wait_option @first repeat
wait_option @second repeat

# %1 and %2 select different prompt values, and all instances of %2 are
# replaced.
reset_options
accept_two_prompts M-m one two
wait_option @one one
wait_option @two two
wait_option @two_again two

# %idx% uses double-quote escaping for indexed replacements.
reset_options
payload='b"$;~\y'
accept_two_prompts M-i ignored "$payload"
wait_option @double_index "$payload"

# A template without replacement markers is left alone.
reset_options
accept_prompt M-n 'unused ; set -g @marker changed'
wait_option @plain no-template-markers
wait_option @marker unchanged

exit 0
