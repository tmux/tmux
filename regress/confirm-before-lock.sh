#!/bin/sh

# Exercise confirm-before's waiting and background callbacks, custom keys and
# prompts, default-yes handling and errors. Also cover all three lock command
# entry points with a harmless lock-command which leaves a marker.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
DIR=$(mktemp -d) || exit 1
TMUX_TMPDIR=$DIR
export TMUX_TMPDIR
INNER="$TEST_TMUX -LtestI$$ -f/dev/null"
OUTER="$TEST_TMUX -LtestO$$ -f/dev/null"

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$OUTER kill-server 2>/dev/null
	$INNER kill-server 2>/dev/null
	rm -rf "$DIR"
}
trap cleanup 0 1 15

capture()
{
	$OUTER capture-pane -p -t outer:0.0 2>/dev/null
}

wait_capture()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		captured=$(capture)
		printf '%s\n' "$captured" | grep -Fq "$marker" && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "timed out waiting for '$marker'"
}

wait_option()
{
	want=$1
	i=0
	while [ "$i" -lt 50 ]; do
		got=$($INNER show-option -gqv @confirmed 2>/dev/null)
		[ "$got" = "$want" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "@confirmed is '$got', expected '$want'"
}

wait_file()
{
	marker=$1
	i=0
	while [ "$i" -lt 50 ]; do
		[ -f "$marker" ] && return 0
		sleep 0.1
		i=$((i + 1))
	done
	fail "lock command did not create $marker"
}

$INNER new-session -d -s test -x80 -y24 'exec sleep 100' || exit 1
$INNER set-option -g status on || exit 1
$INNER set-option -g status-position bottom || exit 1
$INNER set-option -g window-size manual || exit 1
$OUTER new-session -d -s outer -x80 -y24 "$INNER attach -t test" || exit 1
$OUTER set-option -g status off || exit 1
$OUTER set-option -g window-size manual || exit 1
sleep 1

client=$($INNER list-clients -F '#{client_name}')
[ -n "$client" ] || fail "inner client did not attach"

$INNER bind-key -n M-a confirm-before \
	'set-option -g @confirmed accepted' || exit 1
$INNER bind-key -n M-b confirm-before -b -p 'Background?' \
	'set-option -g @confirmed background' || exit 1
$INNER bind-key -n M-c confirm-before -c x -p 'Custom?' \
	'set-option -g @confirmed custom' || exit 1
$INNER bind-key -n M-y confirm-before -y -p 'Default?' \
	'set-option -g @confirmed default' || exit 1

# A non-confirming response closes the waiting prompt without running its
# command; reopening and accepting inserts the command after the waiting item.
$INNER set-option -g @confirmed sentinel || exit 1
$OUTER send-keys M-a || exit 1
wait_capture "Confirm 'set-option'? (y/n)"
$OUTER send-keys n || exit 1
sleep 0.2
wait_option sentinel
$OUTER send-keys M-a || exit 1
wait_capture "Confirm 'set-option'? (y/n)"
$OUTER send-keys y || exit 1
wait_option accepted

# -b has no waiting queue item and appends its command to the client queue.
$OUTER send-keys M-b || exit 1
wait_capture 'Background?'
$OUTER send-keys y || exit 1
wait_option background

# A custom printable key is accepted, and -y makes Enter affirmative.
$OUTER send-keys M-c || exit 1
wait_capture 'Custom?'
$OUTER send-keys x || exit 1
wait_option custom
$OUTER send-keys M-y || exit 1
wait_capture 'Default?'
$OUTER send-keys Enter || exit 1
wait_option default

# Reject invalid multi-character and control confirmation keys.
if $INNER confirm-before -b -t "$client" -c xx 'display-message x' \
    >/dev/null 2>&1; then
	fail "multi-character confirm key was accepted"
fi
if $INNER confirm-before -b -t "$client" -c "$(printf '\001')" \
    'display-message x' >/dev/null 2>&1; then
	fail "control confirm key was accepted"
fi

# lock-client, lock-session and lock-server all send lock-command to the real
# attached client. The command returns immediately and unlocks the client.
for kind in client session server; do
	marker="$DIR/locked-$kind"
	$INNER set-option -t test lock-command "printf locked >$marker" || exit 1
	case "$kind" in
	client) $INNER lock-client -t "$client" || exit 1 ;;
	session) $INNER lock-session -t test || exit 1 ;;
	server) $INNER lock-server || exit 1 ;;
	esac
	wait_file "$marker"
done

$INNER has-session -t test || fail "server died during prompt or lock tests"
exit 0
