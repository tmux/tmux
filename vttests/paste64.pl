#!/usr/bin/env perl
# $XTermId: paste64.pl,v 1.13 2014/12/28 21:16:36 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2006,2014 by Thomas E. Dickey
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
# Test the paste64 option of xterm.

use strict;
use warnings;

use Term::ReadKey;
use IO::Handle;
use MIME::Base64;

our $target = "";

sub to_hex($) {
    my $value  = $_[0];
    my $result = "";
    my $n;

    for ( $n = 0 ; $n < length($value) ; ++$n ) {
        $result .= sprintf( "%02X", ord substr( $value, $n, 1 ) );
    }
    return $result;
}

sub show_string($) {
    my $value = $_[0];
    my $n;

    my $result = "";
    for ( $n = 0 ; $n < length($value) ; $n += 1 ) {
        my $c = ord substr( $value, $n, 1 );
        if ( $c == ord '\\' ) {
            $result .= "\\\\";
        }
        elsif ( $c == 0x1b ) {
            $result .= "\\E";
        }
        elsif ( $c == 0x7f ) {
            $result .= "^?";
        }
        elsif ( $c == 32 ) {
            $result .= "\\s";
        }
        elsif ( $c < 32 ) {
            $result .= sprintf( "^%c", $c + 64 );
        }
        elsif ( $c > 128 ) {
            $result .= sprintf( "\\%03o", $c );
        }
        else {
            $result .= chr($c);
        }
    }

    printf "%s\r\n", $result;
}

sub get_reply($) {
    my $command = $_[0];
    my $reply   = "";

    printf "send: ";
    show_string($command);

    print STDOUT $command;
    autoflush STDOUT 1;
    while (1) {
        my $test = ReadKey 1;
        last if not defined $test;

        #printf "%d:%s\r\n", length($reply), to_hex($test);
        $reply .= $test;
    }
    return $reply;
}

sub get_paste() {
    my $reply = get_reply( "\x1b]52;" . $target . ";?\x1b\\" );

    printf "read: ";
    show_string($reply);

    my $data = $reply;
    $data =~ s/^\x1b]52;[[:alnum:]]*;//;
    $data =~ s/\x1b\\$//;
    printf "chop: ";
    show_string($data);

    $data = decode_base64($data);
    printf "data: ";
    show_string($data);
}

sub put_paste() {
    ReadMode 1;

    printf "data: ";
    my $data = ReadLine 0;
    chomp $data;
    ReadMode 5;

    $data = encode_base64($data);
    chomp $data;
    printf "data: ";
    show_string($data);

    my $send = "\x1b]52;" . $target . ";" . $data . "\x1b\\";

    printf "send: ";
    show_string($send);
    print STDOUT $send;
    autoflush STDOUT 1;
}

sub set_target() {
    ReadMode 1;

    printf "target: ";
    $target = ReadLine 0;
    $target =~ s/[^[:alnum:]]//g;
    ReadMode 5;
    printf "result: %s\r\n", $target;
}

ReadMode 5, 'STDIN';    # allow single-character inputs
while (1) {
    my $cmd;

    printf "\r\nCommand (? for help):";
    $cmd = ReadKey 0;
    if ( $cmd eq "?" ) {
        printf "\r\np=put selection,"
          . " g=get selection,"
          . " q=quit,"
          . " r=reset target,"
          . " s=set target\r\n";
    }
    elsif ( $cmd eq "p" ) {
        printf " ...put selection\r\n";
        put_paste();
    }
    elsif ( $cmd eq "g" ) {
        printf " ...get selection\r\n";
        get_paste();
    }
    elsif ( $cmd eq "q" ) {
        printf " ...quit\r\n";
        last;
    }
    elsif ( $cmd eq "r" ) {
        printf " ...reset\r\n";
        $target = "";
    }
    elsif ( $cmd eq "s" ) {
        printf " ...set target\r\n";
        set_target();
    }
}
ReadMode 0, 'STDIN';    # Reset tty mode before exiting
