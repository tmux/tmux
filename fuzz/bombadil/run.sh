#!/bin/sh
# Run the Bombadil terminal fuzzer against a tmux binary in an isolated
# environment (private socket directory and HOME, clean configuration).
#
#     sh run.sh [bombadil options...]
#
# Environment:
#     TMUX_BIN     tmux binary to test (default: ../../tmux)
#     TIME_LIMIT   how long to fuzz (default: 1m)
#     BOMBADIL     bombadil executable (default: ./node_modules/.bin/bombadil)
#
# Extra arguments are passed to "bombadil terminal test", e.g.
# --output-path, --seed or --reproduce.
set -u

cd "$(dirname "$0")" || exit 1

TMUX_BIN=${TMUX_BIN:-$PWD/../../tmux}
TIME_LIMIT=${TIME_LIMIT:-1m}
BOMBADIL=${BOMBADIL:-$PWD/node_modules/.bin/bombadil}

if [ ! -x "$TMUX_BIN" ]; then
	echo "no tmux binary at $TMUX_BIN; build tmux or set TMUX_BIN" >&2
	exit 1
fi
if [ ! -x "$BOMBADIL" ]; then
	echo "no bombadil at $BOMBADIL; run \"npm install\" here first" >&2
	exit 1
fi

WORK=$(mktemp -d "${TMPDIR:-/tmp}/bombadil-tmux.XXXXXX") || exit 1
cleanup() {
	TMUX_TMPDIR=$WORK "$TMUX_BIN" kill-server 2>/dev/null
	rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

# TMUX unset so this works from inside tmux; HOME redirected so neither tmux
# nor its pane shells touch user dotfiles; -c "$WORK" runs pane shells in the
# throwaway dir so random input can't create files in the repo.
env -u TMUX \
    HOME="$WORK" \
    TMUX_TMPDIR="$WORK" \
    "$BOMBADIL" terminal test \
	--specification "$PWD/specification.ts" \
	--time-limit "$TIME_LIMIT" \
	"$@" \
	"$TMUX_BIN" -f "$PWD/tmux.conf" new-session -A -s bombadil -c "$WORK"
exit $?
