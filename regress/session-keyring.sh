#!/bin/sh

# On Linux the server should join a session keyring of its own, so it keeps
# working after the login session's keyring is revoked, and revoke it when it
# exits.

PATH=/bin:/usr/bin
TERM=screen

[ "$(uname)" = Linux ] || exit 0
keyctl session - true >/dev/null 2>&1 || exit 0

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
OUT=$(mktemp -d)
TMUX="$TEST_TMUX -Ltest$$ -f/dev/null"
$TMUX kill-server 2>/dev/null

fail()
{
	echo "$*"
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
	[ -s $OUT/holder ] && kill $(cat $OUT/holder) 2>/dev/null
	rm -rf "$OUT"
}
trap cleanup 0 1 15

# Start the server from a login session with its own keyring, then revoke that
# keyring as pam_keyinit(8) does when the login session ends.
LOGIN=$(keyctl session - sh -c "
	$TMUX new -d 'sleep 1000' && keyctl id @s && keyctl revoke @s
" 2>/dev/null) || fail "could not start server"

# A new pane should still have a usable keyring, with the user keyring linked.
$TMUX new-window -d "keyctl id @s >$OUT/pane 2>&1; $TMUX wait-for -S pane" ||
	fail "could not create pane"
$TMUX wait-for pane
KEYRING=$(cat $OUT/pane)
case "$KEYRING" in
''|*[!0-9]*)
	fail "pane keyring not usable: $KEYRING"
	;;
esac
[ "$KEYRING" != "$LOGIN" ] || fail "pane has the login keyring"
keyctl rlist $KEYRING | grep -qw "$(keyctl id @u)" ||
	fail "user keyring not linked"

# The server should revoke its keyring when it exits, even while a process it
# started (which ignores the SIGHUP from its pane closing) still holds it.
$TMUX new-window -d "
	trap '' HUP
	echo \$\$ >$OUT/holder
	$TMUX wait-for -S held
	exec sleep 100
" || fail "could not create pane"
$TMUX wait-for held
SERVER=$($TMUX display -p '#{pid}')
$TMUX kill-server
n=0
while kill -0 $SERVER 2>/dev/null && [ $n -lt 50 ]; do
	sleep 0.1
	n=$((n + 1))
done
keyctl rdescribe $KEYRING >$OUT/exit 2>&1
grep -q "revoked" $OUT/exit || fail "keyring not revoked: $(cat $OUT/exit)"

exit 0
