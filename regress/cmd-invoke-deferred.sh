#!/bin/sh

# Deferred command callbacks.
#
# Several commands store a parsed tree and invoke it later through
# cmd_invoke_get: if-shell (then/else, sync and background), run-shell -C, and -
# once a key is pressed by an attached client - a key binding, confirm-before and
# command-prompt. The headless cases run directly on the inner server; the
# client cases use the nested-tmux pattern from prompt-mechanics.sh: an outer
# server hosts the inner client in a pane and keys are injected with send-keys.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT="$TEST_TMUX -Ltest -f/dev/null"
IN="$TEST_TMUX -Ltest2 -f/dev/null"

$OUT kill-server 2>/dev/null
$IN kill-server 2>/dev/null
trap "$OUT kill-server 2>/dev/null; $IN kill-server 2>/dev/null" 0 1 15

fail() { echo "[FAIL] $1" >&2; exit 1; }
settle() { sleep 0.5; }

CONF=$(mktemp)
trap "rm -f $CONF; $OUT kill-server 2>/dev/null; $IN kill-server 2>/dev/null" 0 1 15

# --- Headless deferred callbacks: no client needed. ------------------------
$IN new -d -x80 -y23 -nbase "sh -c 'exec sleep 1000'" || exit 1
$IN set -g status on || exit 1
$IN set -g status-keys emacs || exit 1
$IN set -g window-size manual || exit 1

$IN if-shell true 'set -g @t then' 'set -g @t else' || exit 1
[ "$($IN show -gv @t)" = then ] || fail "if-shell true ran wrong branch"
$IN if-shell false 'set -g @f then' 'set -g @f else' || exit 1
[ "$($IN show -gv @f)" = else ] || fail "if-shell false ran wrong branch"

# Background (-b) if-shell evaluates its condition in a shell asynchronously.
$IN if-shell -b 'true' 'set -g @b yes' 'set -g @b no' || exit 1
settle
[ "$($IN show -gv @b)" = yes ] || fail "background if-shell ran wrong branch"

# A multi-command then-body runs both commands.
$IN if-shell true 'new-window -dn IF1 ; new-window -dn IF2' '' || exit 1
$IN list-windows -F '#{window_name}' | grep -qx IF1 || fail "if-shell body cmd1 missing"
$IN list-windows -F '#{window_name}' | grep -qx IF2 || fail "if-shell body cmd2 missing"

# run-shell -C invokes its argument as tmux commands.
$IN run-shell -C 'set -g @rc ran' || exit 1
[ "$($IN show -gv @rc)" = ran ] || fail "run-shell -C did not run command"

# --- Client deferred callbacks: key binding, confirm-before, command-prompt. -
$IN bind -n M-c confirm-before -p '(ok) ' 'set -g @cb confirmed' || exit 1
$IN bind -n M-d command-prompt -I pre -p '(cmd) ' 'set -g @cp %%' || exit 1
# A brace body must come through the lexer, so bind it from a config.
cat <<'EOF' >$CONF
bind -n M-k { new-window -dn K1 ; new-window -dn K2 }
EOF
$IN source-file $CONF || exit 1

$OUT new -d -x80 -y24 || exit 1
$OUT set -g status off || exit 1
$OUT set -g window-size manual || exit 1
$OUT send-keys -l "$IN attach" || exit 1
$OUT send-keys Enter || exit 1
sleep 1

# Key binding with a stored multi-command body fires on keypress.
$OUT send-keys M-k || exit 1
settle
$IN list-windows -F '#{window_name}' | grep -qx K1 || fail "key binding body cmd1 missing"
$IN list-windows -F '#{window_name}' | grep -qx K2 || fail "key binding body cmd2 missing"

# confirm-before runs its command only when confirmed.
$IN set -g @cb none || exit 1
$OUT send-keys M-c || exit 1
settle
$OUT capture-pane -p | tail -1 | grep -qF '(ok)' || fail "confirm-before prompt not shown"
$OUT send-keys n || exit 1
settle
[ "$($IN show -gv @cb)" = none ] || fail "confirm-before ran command after 'n'"
$OUT send-keys M-c || exit 1
settle
$OUT send-keys y || exit 1
settle
[ "$($IN show -gv @cb)" = confirmed ] || fail "confirm-before did not run command after 'y'"

# command-prompt feeds the typed line (with -I prefill) into its template.
$IN set -g @cp none || exit 1
$OUT send-keys M-d || exit 1
settle
$OUT capture-pane -p | tail -1 | grep -qF '(cmd)' || fail "command-prompt not shown"
$OUT send-keys -l X || exit 1
$OUT send-keys Enter || exit 1
settle
[ "$($IN show -gv @cp)" = preX ] || fail "command-prompt recovered '$($IN show -gv @cp)'"

$OUT kill-server 2>/dev/null
$IN kill-server 2>/dev/null
exit 0
