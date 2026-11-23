# Let pending redraws flush, then wait for the terminal output to drain.
set -e
binary=$1
socket=$2
client=$3
sleep 0.02
while [ "$("$binary" -S "$socket" display-message -p -c "$client" '#{client_pending}')" != 0 ]; do
	sleep 0.01
done
[ "$("$binary" -S "$socket" display-message -p -c "$client" '#{client_discarded}')" = 0 ]
