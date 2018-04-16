#!/usr/bin/env perl
# $XTermId: query-fonts.pl,v 1.6 2014/02/26 20:14:50 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2010,2014 by Thomas E. Dickey
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
# Test the font-query features of xterm.

# TODO:
# test relative vs absolute font numbering
# test all font-slots
# test selection
# test bold / wide / widebold
# test actual fontname vs nominal
# extend "CSI > Ps; Ps T" to query fontname in hex

use strict;
use warnings;

use Getopt::Std;
use IO::Handle;

our ( $opt_a, $opt_r, $opt_s );
&getopts('ars') || die(
    "Usage: $0 [options]\n
Options:\n
  -a      test using absolute numbers
  -r      test using relative numbers
  -s      use ^G rather than ST
"
);

our $ST = $opt_s ? "\007" : "\x1b\\";

sub no_reply($) {
    open TTY, "+</dev/tty" or die("Cannot open /dev/tty\n");
    autoflush TTY 1;
    my $old = `stty -g`;
    system "stty raw -echo min 0 time 5";

    print TTY @_;
    close TTY;
    system "stty $old";
}

sub get_reply($) {
    open TTY, "+</dev/tty" or die("Cannot open /dev/tty\n");
    autoflush TTY 1;
    my $old = `stty -g`;
    system "stty raw -echo min 0 time 5";

    print TTY @_;
    my $reply = <TTY>;
    close TTY;
    system "stty $old";
    if ( defined $reply ) {
        die("^C received\n") if ( "$reply" eq "\003" );
    }
    return $reply;
}

sub query_font($) {
    my $param = $_[0];
    my $reply;
    my $n;
    my $st    = $opt_s ? qr/\007/ : qr/\x1b\\/;
    my $osc   = qr/\x1b]50/;
    my $match = qr/${osc}.*${st}/;

    $reply = get_reply( "\x1b]50;?" . $param . $ST );

    printf "query{%s}%*s", $param, 3 - length($param), " ";

    if ( defined $reply ) {
        printf "%2d ", length($reply);
        if ( $reply =~ /${match}/ ) {

            $reply =~ s/^${osc}//;
            $reply =~ s/^;//;
            $reply =~ s/${st}$//;
        }
        else {
            printf "? ";
        }

        my $result = "";
        for ( $n = 0 ; $n < length($reply) ; ) {
            my $c = substr( $reply, $n, 1 );
            if ( $c =~ /[[:print:]]/ ) {
                $result .= $c;
            }
            else {
                my $k = ord substr( $reply, $n, 1 );
                if ( ord $k == 0x1b ) {
                    $result .= "\\E";
                }
                elsif ( $k == 0x7f ) {
                    $result .= "^?";
                }
                elsif ( $k == 32 ) {
                    $result .= "\\s";
                }
                elsif ( $k < 32 ) {
                    $result .= sprintf( "^%c", $k + 64 );
                }
                elsif ( $k > 128 ) {
                    $result .= sprintf( "\\%03o", $k );
                }
                else {
                    $result .= chr($k);
                }
            }
            $n += 1;
        }

        printf "{%s}", $result;
    }
    printf "\n";
}

if ($opt_r) {
    my $n;
    query_font("-");
    foreach $n ( 0 .. 5 ) {
        query_font( sprintf "-%d", $n );
    }
    query_font("+");
    foreach $n ( 0 .. 5 ) {
        query_font( sprintf "+%d", $n );
    }
}
if ($opt_a) {
    my $n;
    foreach $n ( 0 .. 5 ) {
        query_font( sprintf "%d", $n );
    }
}
if ( not $opt_a and not $opt_r ) {
    query_font("");
}
