#!/bin/sh

# The socket directory must be an existing absolute path without "..". An
# empty $TMUX_TMPDIR is skipped.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
unset TMUX TMUX_TMPDIR

TMP=$(mktemp -d)
TMP=$(cd "$TMP" && pwd -P)
L="testSP$$"
DEFAULT="$(cd /tmp && pwd -P)/tmux-$(id -u)/$L"

cleanup()
{
	"$TEST_TMUX" -S "$DEFAULT" kill-server 2>/dev/null
	"$TEST_TMUX" -S "$TMP/tmux-$(id -u)/$L" kill-server 2>/dev/null
	rm -rf "$TMP" "$DEFAULT" "$DEFAULT.lock"
}
trap cleanup 0 1 15

# check_path expected env...
check_path()
{
	expected=$1; shift
	env "$@" "$TEST_TMUX" -L"$L" -f/dev/null new -d 'sleep 60' || exit 1
	actual=$(env "$@" "$TEST_TMUX" -L"$L" display -p '#{socket_path}')
	env "$@" "$TEST_TMUX" -L"$L" kill-server
	[ "$actual" = "$expected" ] || exit 1
}

# check_error expected env...
check_error()
{
	expected=$1; shift
	if actual=$(env "$@" "$TEST_TMUX" -L"$L" -f/dev/null new -d \
	    'sleep 60' 2>&1); then
		env "$@" "$TEST_TMUX" -L"$L" kill-server
		exit 1
	fi
	[ "$actual" = "$expected" ] || exit 1
}

check_path "$DEFAULT" TMUX_TMPDIR=
check_path "$TMP/tmux-$(id -u)/$L" TMUX_TMPDIR="$TMP"

check_error "socket directory tmp is not an absolute path" TMUX_TMPDIR=tmp
check_error "couldn't resolve socket directory $TMP/missing (No such file or directory)" \
    TMUX_TMPDIR="$TMP/missing"
check_error "socket directory $TMP/.. contains .." TMUX_TMPDIR="$TMP/.."
check_error "socket directory /../tmp contains .." TMUX_TMPDIR=/../tmp
check_error "socket directory $TMP/../.. contains .." TMUX_TMPDIR="$TMP/../.."

exit 0
