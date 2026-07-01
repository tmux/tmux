#!/bin/sh

# Parse/print golden suite for the command parser.
#
# Binds a spread of command syntax forms into a dedicated key table, then prints
# them back with list-keys (which calls cmd_parse_print on the stored tree). A
# command-valued option is printed too. Three things are checked:
#   - the default (single line) form is byte-for-byte as expected: quoting,
#     escaped \; separators, inline braces, and preservation of unexpanded
#     ${env}, ~ and #{format} inside stored bodies;
#   - the -p (multiline) form is as expected;
#   - the default form round-trips: sourcing it again reproduces the same keys.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
CONF=$(mktemp)
EXP=$(mktemp)
RT1=$(mktemp)
RT2=$(mktemp)
trap "rm -f $TMP $CONF $EXP $RT1 $RT2" 0 1 15

cat <<'EOF' >$CONF
bind -T parsetest a display-message hello
bind -T parsetest b display-message "hello world"
bind -T parsetest c display-message 'literal $HOME #{p} ~'
bind -T parsetest d display-message ""
bind -T parsetest e display-message "#{pane_id}"
bind -T parsetest f display-message a \; display-message b
bind -T parsetest g {
  display-message one
  display-message two
}
bind -T parsetest h if-shell true { display-message yes } { display-message no }
bind -T parsetest m if-shell true { display-message "${HOME}" "~" "~root" "#{pane_id}" }
bind -T parsetest n if-shell true { display-message "a\nb" "x;y" '#literal' }
EOF

# Expected output. The default form is single line (round-trippable); the -p
# form is multiline, with lines inside braced bodies indented by a single tab.
cat <<'EOF' >$EXP
bind-key  -T parsetest a display-message hello
bind-key  -T parsetest b display-message 'hello world'
bind-key  -T parsetest c display-message 'literal $HOME #{p} ~'
bind-key  -T parsetest d display-message ''
bind-key  -T parsetest e display-message '#{pane_id}'
bind-key  -T parsetest f display-message a \; display-message b
bind-key  -T parsetest g display-message one \; display-message two
bind-key  -T parsetest h if-shell true { display-message yes } { display-message no }
bind-key  -T parsetest m if-shell true { display-message ${HOME} ~ ~root '#{pane_id}' }
bind-key  -T parsetest n if-shell true { display-message a\nb 'x;y' '#literal' }
--- options ---
display-message 'hi there'
--- multiline ---
bind-key  -T parsetest a display-message hello
bind-key  -T parsetest b display-message 'hello world'
bind-key  -T parsetest c display-message 'literal $HOME #{p} ~'
bind-key  -T parsetest d display-message ''
bind-key  -T parsetest e display-message '#{pane_id}'
bind-key  -T parsetest f display-message a ; display-message b
bind-key  -T parsetest g display-message one
display-message two
bind-key  -T parsetest h if-shell true {
	display-message yes
} {
	display-message no
}
bind-key  -T parsetest m if-shell true {
	display-message ${HOME} ~ ~root '#{pane_id}'
}
bind-key  -T parsetest n if-shell true {
	display-message a\nb 'x;y' '#literal'
}
EOF

$TMUX -f/dev/null start \; new-session -d 2>/dev/null || exit 1
$TMUX source-file $CONF || exit 1
$TMUX set -g default-client-command 'display-message "hi there"' || exit 1

{
	$TMUX list-keys -T parsetest
	echo "--- options ---"
	$TMUX show -gv default-client-command
	echo "--- multiline ---"
	$TMUX list-keys -p -T parsetest
} >$TMP 2>&1 || exit 1

cmp -s $TMP $EXP || {
	echo "cmd-parse-print: output differs from expected" >&2
	diff -u $EXP $TMP >&2
	exit 1
}

# Round-trip: the default form re-sourced into a fresh server must reproduce the
# identical key list.
$TMUX list-keys -T parsetest >$RT1 || exit 1
$TMUX kill-server 2>/dev/null
$TMUX -f/dev/null start \; new-session -d 2>/dev/null || exit 1
$TMUX source-file $RT1 || exit 1
$TMUX list-keys -T parsetest >$RT2 || exit 1
$TMUX kill-server 2>/dev/null

cmp -s $RT1 $RT2 || {
	echo "cmd-parse-print: default form does not round-trip" >&2
	diff -u $RT1 $RT2 >&2
	exit 1
}

exit 0
