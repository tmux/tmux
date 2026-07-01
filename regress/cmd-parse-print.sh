#!/bin/sh

# Parse/print golden suite for the command parser.
#
# Binds a spread of command syntax forms into a dedicated key table, then prints
# them back with list-keys (which calls cmd_parse_print on the stored tree). A
# command-valued option is printed too. The normalized output is compared byte
# for byte against the expected block below, locking in the current parse/print
# behaviour: quoting, separators, braced and nested bodies, and preservation of
# unexpanded ${env}, ~ and #{format} syntax inside stored bodies.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
CONF=$(mktemp)
EXP=$(mktemp)
trap "rm -f $TMP $CONF $EXP" 0 1 15

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

# Expected normalized output. Lines inside the braced bodies are indented with a
# single tab.
cat <<'EOF' >$EXP
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
--- options ---
display-message 'hi there'
EOF

$TMUX -f/dev/null start \; new-session -d 2>/dev/null || exit 1
$TMUX source-file $CONF || exit 1
$TMUX set -g default-client-command 'display-message "hi there"' || exit 1

{
	$TMUX list-keys -T parsetest
	echo "--- options ---"
	$TMUX show -gv default-client-command
} >$TMP 2>&1 || exit 1

$TMUX kill-server 2>/dev/null

cmp -s $TMP $EXP || {
	echo "cmd-parse-print: output differs from expected" >&2
	diff -u $EXP $TMP >&2
	exit 1
}

exit 0
