#!/usr/bin/env perl
# $XTermId: resize.pl,v 1.6 2017/01/22 18:34:06 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2004-2014,2017 by Thomas E. Dickey
#
#                         All Rights Reserved
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Except as contained in this notice, the name(s) of the above copyright
# holders shall not be used in advertising or otherwise to promote the
# sale, use or other dealings in this Software without prior written
# authorization.
# -----------------------------------------------------------------------------
# resize.sh rewritten into Perl for comparison.
# See also Term::ReadKey.

use strict;
use warnings;

use IO::Handle;

sub write_tty {
    open TTY, "+</dev/tty" or die("Cannot open /dev/tty\n");
    autoflush TTY 1;
    print TTY @_;
    close TTY;
}

sub get_reply {
    open TTY, "+</dev/tty" or die("Cannot open /dev/tty\n");
    autoflush TTY 1;
    my $old = `stty -g`;
    system "stty raw -echo min 0 time 5";

    print TTY @_;
    my $reply = <TTY>;
    close TTY;
    system "stty $old";
    return $reply;
}

sub csi_field {
    my $first  = $_[0];
    my $second = $_[1];
    $first =~ s/^[^0-9]+//;
    while ( --$second > 0 ) {
        $first =~ s/^[\d]+//;
        $first =~ s/^[^\d]+//;
    }
    $first =~ s/[^\d]+.*$//;
    return $first;
}

our $original = get_reply("\x1b[18t");
our $high;
our $wide;

if ( defined($original) and ( $original =~ /\x1b\[8;\d+;\d+t/ ) ) {
    $high = csi_field( $original, 2 );
    $wide = csi_field( $original, 3 );
    printf "parsed terminal size $high,$wide\n";
}
else {
    die "Cannot get current terminal size via escape sequence\n";
}

#
our $maximize = get_reply("\x1b[19t");
our $maxhigh;
our $maxwide;

if ( defined($maximize) and ( $maximize =~ /^\x1b\[9;\d+;\d+t/ ) ) {
    $maxhigh = csi_field( $maximize, 2 );
    $maxwide = csi_field( $maximize, 3 );
    $maxhigh != 0 or $maxhigh = $high * 2;
    $maxwide != 0 or $maxwide = $wide * 2;
    printf "parsed terminal maxsize $maxhigh,$maxwide\n";
}
else {
    die "Cannot get maximum terminal size via escape sequence\n";
}

our $zapped;
our ( $w, $h, $a );

sub catch_zap {
    $zapped++;
}
$SIG{INT}  = \&catch_zap;
$SIG{QUIT} = \&catch_zap;
$SIG{KILL} = \&catch_zap;
$SIG{HUP}  = \&catch_zap;
$SIG{TERM} = \&catch_zap;

$w      = $wide;
$h      = $high;
$a      = 1;
$zapped = 0;
while ( $zapped == 0 ) {

    #	sleep 1
    printf "resizing to $h by $w\n";
    write_tty( "\x1b[8;$h;$w" . "t" );
    if ( $a == 1 ) {
        if ( $w == $maxwide ) {
            $h += $a;
            if ( $h = $maxhigh ) {
                $a = -1;
            }
        }
        else {
            $w += $a;
        }
    }
    else {
        if ( $w == $wide ) {
            $h += $a;
            if ( $h = $high ) {
                $a = 1;
            }
        }
        else {
            $w += $a;
        }
    }
}
write_tty($original);
