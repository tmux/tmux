#!/usr/bin/env perl
# $XTermId: dynamic.pl,v 1.4 2017/01/22 18:34:06 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2011-2014,2017 by Thomas E. Dickey
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
# Test the dynamic-color query option of xterm.
# The programs xtermcontrol and xtermset provide more options.

use strict;
use warnings;

use Getopt::Std;
use IO::Handle;

our @color_names = (
    "VT100 text foreground",
    "VT100 text background",
    "text cursor",
    "mouse foreground",
    "mouse background",
    "Tektronix foreground",
    "Tektronix background",
    "highlight background",
    "Tektronix cursor",
    "highlight foreground"
);

our ( $opt_c, $opt_r );
&getopts('c:r') || die(
    "Usage: $0 [options]\n
Options:\n
  -c XXX  set cursor-color
  -r      reset colors
"
);

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

sub query_color($) {
    my $code   = $_[0];
    my $param1 = $code + 10;
    my $reply;

    $reply = get_reply("\x1b]$param1;?\007");

    return unless defined $reply;
    if ( $reply =~ /\x1b]$param1;.*\007/ ) {
        my $value = $reply;

        $value =~ s/^\x1b]$param1;//;
        $value =~ s/\007//;

        printf "%24s = %s\n", $color_names[$code], $value;
    }
}

sub query_colors() {
    my $n;

    for ( $n = 0 ; $n <= 9 ; ++$n ) {
        &query_color($n);
    }
}

sub reset_colors() {
    my $n;

    for ( $n = 0 ; $n <= 9 ; ++$n ) {
        my $code = 110 + $n;
        &no_reply("\x1b]$code\007");
    }
}

if ( defined($opt_c) ) {
    &no_reply("\x1b]12;$opt_c\007");
}
if ( defined($opt_r) ) {
    &reset_colors();
}

&query_colors();
