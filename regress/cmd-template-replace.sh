#!/bin/sh

# Exercise invocation-time template replacement through command-prompt. String
# and braced templates are parsed before prompting; responses are literal argv
# values and cannot add arguments or commands to the stored tree.

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
$IN bind -n M-u command-prompt -p '(unmatched)' \
	"set -g @plain '%9 %0 %'" || exit 1

# Command-valued bodies must have the same replacement behaviour as strings.
cat >"$TMP/bindings.conf" <<'EOF'
bind -n M-b command-prompt -p '(braced)' { set -g @r '%%' }
bind -n M-t command-prompt -p '(first)' {
  set -g @first '%%'
  set -g @second '%%'
}
bind -n M-v command-prompt -p 'one,two' {
  set -g @one %1
  set -g @two %2
  set -g @two_again %2
  set -g @r '%%'
}
bind -n M-q command-prompt -p '(indexed)' { set -g @r %1 }
bind -n M-w command-prompt -p '(within)' { set -g @r '%%/%%' }
EOF
$IN source-file "$TMP/bindings.conf" || exit 1

$OUT new -d -x80 -y24 || exit 1
$OUT set -g status off || exit 1
$OUT set -g window-size manual || exit 1
$OUT send-keys -l "$IN attach" || exit 1
$OUT send-keys Enter || exit 1
sleep 1

reset_options

# Quoting has already been parsed. Quotes in a response stay literal data.
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

# %%% is accepted like %%, without adding quotation escapes to the value.
reset_options
payload='a"$;~\z'
accept_prompt M-d "$payload"
wait_option @r "$payload"

# Indexed replacements are also data, so semicolons cannot add commands.
reset_options
$IN set -g @raw_tail unchanged || exit 1
payload='raw ; set -g @raw_tail yes'
accept_prompt M-r "$payload"
wait_option @r "$payload"
wait_option @raw_tail unchanged

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

# %idx% is accepted like %idx, with no quotation escaping.
reset_options
payload='b"$;~\y'
accept_two_prompts M-i ignored "$payload"
wait_option @double_index "$payload"

# Responses are not scanned again for markers from later prompt values.
reset_options
accept_two_prompts M-m '%2/%%' two
wait_option @one '%2/%%'
wait_option @two two
wait_option @two_again two

# A missing response and invalid or trailing percent markers stay literal.
reset_options
accept_prompt M-u unused
wait_option @plain '%9 %0 %'

# Reuse the stored string tree with a different response. Expansion must not
# mutate the tree or carry the first-%% state across separate invocations.
reset_options
payload="another'quote"
accept_prompt M-s "$payload"
wait_option @r "$payload"

# Braced bodies preserve the same first-%% rule across commands and within an
# argument. Indexed replacements do not consume this first-%% marker.
reset_options
payload="brace'quote ; set -g @marker changed"
accept_prompt M-b "$payload"
wait_option @r "$payload"
wait_option @marker unchanged
accept_prompt M-t "$payload"
wait_option @first "$payload"
wait_option @second %%
accept_prompt M-w value
wait_option @r 'value/%%'

reset_options
accept_two_prompts M-v '%2/%%' two
wait_option @one '%2/%%'
wait_option @two two
wait_option @two_again two
wait_option @r '%2/%%'
payload="can't ; set -g @marker changed"
payload="$payload"' ; "$HOME" ~ \ %2'
accept_prompt M-q "$payload"
wait_option @r "$payload"
wait_option @marker unchanged

# A template without replacement markers is left alone.
reset_options
accept_prompt M-n 'unused ; set -g @marker changed'
wait_option @plain no-template-markers
wait_option @marker unchanged

exit 0
