#!/bin/sh

# 883
# if-shell with an error should not core :-)

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
SOCKET=$(mktemp -u testAXXXXXX)
TMUX="$TEST_TMUX -L$SOCKET -f/dev/null"
$TMUX kill-server 2>/dev/null

TMP=$(mktemp)
OUT=$(mktemp)
cleanup() {
	$TMUX kill-server 2>/dev/null
	rm -f "$TMP" "$OUT"
}
trap cleanup 0 1 15

cat <<EOF >"$TMP"
if 'true' 'wibble wobble'
EOF

$TMUX -f"$TMP" -C new <<EOF >"$OUT"
EOF
if ! grep -q "^%config-error $TMP:1: unknown command: wibble$" "$OUT"; then
	cat "$OUT" >&2
	exit 1
fi

cat <<EOF >"$TMP"
wibble wobble
EOF

echo "source $TMP" | $TMUX -C new  >"$OUT"
if ! grep -q "^%config-error $TMP:1: unknown command: wibble$" "$OUT"; then
	cat "$OUT" >&2
	exit 1
fi
