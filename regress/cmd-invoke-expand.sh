#!/bin/sh

# Invoke-time expansion and conditionals.
#
# The parser builds syntax only; environment/tilde expansion, assignments and
# %if/%elif/%else are all evaluated when the tree is invoked. This drives every
# one of those through a single config and checks the resulting option values:
#   - FOO=bar assignments set the environment, read back with ${FOO} and $FOO
#   - %hidden assignments behave the same
#   - ~ expands to the home directory
#   - %if / %elif / %else selects the correct branch

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -Ltest -f/dev/null"
$TMUX kill-server 2>/dev/null

CONF=$(mktemp)
trap "rm -f $CONF" 0 1 15

cat <<'EOF' >$CONF
FOO=hello
set -g @assign "${FOO}"
set -g @dollar "$FOO"
%hidden BAR=secret
set -g @hidden "${BAR}"
set -g @undef "x${NOSUCHVAR_ZZZ}y"
set -g @tilde ~
set -g @baduser ~nosuchuser_zzz9
%if 1
set -g @iftrue then
%else
set -g @iftrue else
%endif
%if 0
set -g @iffalse then
%else
set -g @iffalse else
%endif
%if 0
set -g @elif a
%elif 1
set -g @elif b
%else
set -g @elif c
%endif
EOF

$TMUX -f/dev/null start \; new-session -d 2>/dev/null || exit 1
$TMUX source-file $CONF || exit 1

check() {
	got=$($TMUX show -gv "$1")
	[ "$got" = "$2" ] || { echo "$1: got [$got] expected [$2]" >&2; exit 1; }
}
check @assign hello
check @dollar hello
check @hidden secret
check @undef xy			# undefined environment variable expands to nothing
check @baduser ""		# unknown user in ~user expands to nothing
check @iftrue then
check @iffalse else
check @elif b

# ~ expands to an absolute home directory (value is machine dependent).
tilde=$($TMUX show -gv @tilde)
case "$tilde" in
/*) ;;
*) echo "@tilde did not expand to an absolute path: [$tilde]" >&2; exit 1;;
esac

$TMUX kill-server 2>/dev/null
exit 0
