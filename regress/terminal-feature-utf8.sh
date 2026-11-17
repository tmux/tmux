#!/bin/sh

# The utf8 terminal feature must mark an attached client as UTF-8 even when
# neither its environment nor -u does so. It must also be reported by the
# client terminal-feature format lookup.

PATH=/bin:/usr/bin
TERM=screen
LC_ALL=C.UTF-8
export PATH TERM LC_ALL

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
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
}
trap cleanup 0 1 15

cleanup
$INNER new-session -d -s inner -x40 -y10 'exec sleep 100' || exit 1
$INNER set-option -g status off || exit 1

# A clean C-locale environment avoids the ordinary UTF-8 detection paths.
# -T utf8 is therefore solely responsible for setting client_utf8.
client_command="env -i PATH=/bin:/usr/bin TERM=screen LC_ALL=C $TEST_TMUX -T utf8 -LtestI$$ -f/dev/null attach-session -t inner"
$OUTER new-session -d -s outer -x40 -y10 "$client_command" || exit 1
$OUTER set-option -g status off || exit 1

i=0
while [ "$i" -lt 50 ]; do
	client=$($INNER list-clients -F '#{client_name}' 2>/dev/null)
	utf8=$($INNER list-clients -F '#{client_utf8}' 2>/dev/null)
	[ -n "$client" ] && [ "$utf8" = 1 ] && break
	sleep 0.1
	i=$((i + 1))
done
[ "$i" -lt 50 ] || fail "utf8 terminal feature did not set client_utf8"

features=$($INNER list-clients -F '#{client_termfeatures}')
case ",$features," in
*,utf8,*) ;;
*) fail "utf8 is missing from client_termfeatures: $features" ;;
esac

present=$($INNER display-message -c "$client" -p '#{I/f:utf8}')
[ "$present" = 1 ] || fail "utf8 terminal feature lookup returned '$present'"

exit 0
