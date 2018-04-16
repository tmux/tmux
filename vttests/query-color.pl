#!/usr/bin/env perl
# $XTermId: query-color.pl,v 1.5 2017/01/22 18:34:06 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2012-2014,2017 by Thomas E. Dickey
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
# Test the color-query features of xterm using OSC 4.

# TODO: extend to the OSC 5 colors
# TODO: show result in #rrggbb format.

use strict;
use warnings;

use Getopt::Std;
use IO::Handle;

our ($opt_s);
&getopts('s') || die(
    "Usage: $0 [options] [color1[-color2]]\n
Options:\n
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

sub visible($) {
    my $reply = $_[0];
    my $n;
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

    return $result;
}

sub query_color($) {
    my $param = $_[0];
    my $reply;
    my $n;
    my $st    = $opt_s ? qr/\007/ : qr/\x1b\\/;
    my $op    = 4;
    my $osc   = qr/\x1b]$op/;
    my $match = qr/${osc}.*${st}/;

    $reply = get_reply( "\x1b]$op;" . $param . ";?" . $ST );

    printf "query{%s}%*s", &visible($param), 3 - length($param), " ";

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

        printf "{%s}", visible($reply);
    }
    printf "\n";
}

sub query_colors($$) {
    my $lo = $_[0];
    my $hi = $_[1];
    my $n;
    for ( $n = $lo ; $n <= $hi ; ++$n ) {
        query_color($n);
    }
}

if ( $#ARGV >= 0 ) {
    while ( $#ARGV >= 0 ) {
        if ( $ARGV[0] =~ /-/ ) {
            my @args = split /-/, $ARGV[0];
            &query_colors( $args[0], $args[1] );
        }
        else {
            &query_colors( $ARGV[0], $ARGV[0] );
        }
        shift @ARGV;
    }
}
else {
    &query_colors( 0, 7 );
}
