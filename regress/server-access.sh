#!/bin/sh

# Exercise the unprivileged server-access surface: user and group ACL entries,
# read-only/writable transitions, listing, removal, implied addition, and
# command errors. Actual connections from another UID require privileges and
# deliberately do not form part of this test.

PATH=/bin:/usr/bin
TERM=screen
export PATH TERM

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
TMUX="$TEST_TMUX -LtestA$$ -f/dev/null"
OUT=$(mktemp) || exit 1

fail()
{
	echo "$*" >&2
	exit 1
}

cleanup()
{
	$TMUX kill-server 2>/dev/null
	rm -f "$OUT"
}
trap cleanup 0 1 15

expect_fail()
{
	message=$1
	shift
	if $TMUX "$@" >"$OUT" 2>&1; then
		fail "$message: command unexpectedly succeeded"
	fi
}

assert_entry()
{
	entry=$1
	$TMUX server-access -l >"$OUT" || exit 1
	grep -Fxq "$entry" "$OUT" ||
		fail "missing ACL entry '$entry'"
}

assert_no_entry()
{
	entry=$1
	$TMUX server-access -l >"$OUT" || exit 1
	if grep -Fxq "$entry" "$OUT"; then
		fail "unexpected ACL entry '$entry'"
	fi
}

owner_uid=$(id -u)
owner_name=$(id -un)

# nobody is the non-owner test identity. Derive its primary group rather than
# assuming whether the system calls it nobody, nogroup, or something else.
acl_user=nobody
acl_group=$(id -gn "$acl_user" 2>/dev/null) ||
	fail "the nobody account is unavailable"
[ "$owner_name" != "$acl_user" ] || fail "test cannot run as nobody"

$TMUX new-session -d -s access 'exec sleep 100' || exit 1

# The owner is installed as writable at server startup (root is intentionally
# omitted from display output).
if [ "$owner_uid" != 0 ]; then
	assert_entry "$owner_name (U,W)"
fi

# User ACL lifecycle, including duplicate and missing-entry errors.
$TMUX server-access -a "$acl_user" || exit 1
assert_entry "$acl_user (U,W)"
expect_fail "duplicate user addition" server-access -a "$acl_user"
$TMUX server-access -r "$acl_user" || exit 1
assert_entry "$acl_user (U,R)"
$TMUX server-access -w "$acl_user" || exit 1
assert_entry "$acl_user (U,W)"
$TMUX server-access -d "$acl_user" || exit 1
assert_no_entry "$acl_user (U,W)"
expect_fail "removing absent user" server-access -d "$acl_user"

# -r and -w imply -a when an entry is absent.
$TMUX server-access -r "$acl_user" || exit 1
assert_entry "$acl_user (U,R)"
$TMUX server-access -d "$acl_user" || exit 1

# The same lifecycle for a group exercises the separate ACL key space.
$TMUX server-access -g -a -r "$acl_group" || exit 1
assert_entry "$acl_group (G,R)"
$TMUX server-access -g -w "$acl_group" || exit 1
assert_entry "$acl_group (G,W)"
expect_fail "duplicate group addition" server-access -g -a "$acl_group"
$TMUX server-access -g -d "$acl_group" || exit 1
assert_no_entry "$acl_group (G,W)"
expect_fail "removing absent group" server-access -g -d "$acl_group"

# Parser and lookup errors, plus the immutable owner/root entries.
expect_fail "missing ACL subject" server-access -a
expect_fail "unknown user" server-access -a "tmux-no-user-$$"
expect_fail "unknown group" server-access -g -a "tmux-no-group-$$"
expect_fail "conflicting add/delete" server-access -a -d "$acl_user"
expect_fail "conflicting read/write" server-access -r -w "$acl_user"
expect_fail "changing server owner" server-access -r "$owner_name"
if [ "$owner_name" != root ]; then
	expect_fail "changing root access" server-access -a root
fi

$TMUX has-session -t access || fail "server exited during ACL updates"
exit 0
