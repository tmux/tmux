#!/bin/sh

# Regression test for control clients that disconnect before MSG_IDENTIFY_DONE.

PATH=/bin:/usr/bin
TERM=screen

[ -z "$TEST_TMUX" ] && TEST_TMUX=$(readlink -f ../tmux)
SOCK=$(mktemp -u)
TMUX="$TEST_TMUX -S$SOCK"
trap "$TMUX kill-server 2>/dev/null; rm -f $SOCK" 0 1 15

$TMUX -f/dev/null new -d || exit 1

perl -MIO::Socket::UNIX -e '
use strict;
use warnings;

my $socket_path            = $ARGV[0];
my $MSG_IDENTIFY_LONGFLAGS = 111;
my $PROTOCOL_VERSION       = 8;
my $CLIENT_CONTROL         = 0x2000;

my $sock = IO::Socket::UNIX->new(
    Type => SOCK_STREAM,
    Peer => $socket_path
) or die "connect failed: $!";

print {$sock} pack("LLLLQ<",
    $MSG_IDENTIFY_LONGFLAGS,
    16 + 8,
    $PROTOCOL_VERSION,
    $$,
    $CLIENT_CONTROL
) or die "write failed: $!";

close($sock) or die "close failed: $!";
' "$SOCK" || exit 1

sleep 1
$TMUX has || exit 1
$TMUX kill-server 2>/dev/null

exit 0
