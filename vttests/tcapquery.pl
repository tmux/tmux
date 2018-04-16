#!/usr/bin/env perl
# $XTermId: tcapquery.pl,v 1.20 2017/01/22 18:34:06 tom Exp $
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
# Test the tcap-query option of xterm.

use strict;
use warnings;

use Getopt::Std;
use IO::Handle;

our (
    $opt_a, $opt_b, $opt_c, $opt_e, $opt_f, $opt_i,
    $opt_k, $opt_m, $opt_t, $opt_x, $opt_X
);
&getopts('abcefikmt:x:X') || die(
    "Usage: $0 [options]\n
Options:\n
  -a      (same as -c -e -f -k -m)
  -b      use both terminfo and termcap (default is termcap)
  -c      cursor-keys
  -e      editing keypad-keys
  -f      function-keys
  -i      use terminfo rather than termcap names
  -k      numeric keypad-keys
  -m      miscellaneous (none of -c, -e, -f, -k)
  -t NAME use given NAME for \$TERM, set that in xterm's tcap keyboard
  -x KEY  extended cursor/editing key (terminfo only)
  -X      test all extended cursor- and/or editing-keys (terminfo)
"
);

if (
    not(   defined($opt_c)
        or defined($opt_e)
        or defined($opt_f)
        or defined($opt_k)
        or defined($opt_m)
        or defined($opt_x) )
  )
{
    $opt_a = 1;
}

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

sub hexified($) {
    my $value  = $_[0];
    my $result = "";
    my $n;

    for ( $n = 0 ; $n < length($value) ; ++$n ) {
        $result .= sprintf( "%02X", ord substr( $value, $n, 1 ) );
    }
    return $result;
}

sub modify_tcap($) {
    my $name  = $_[0];
    my $param = hexified($name);
    no_reply( "\x1bP+p" . $param . "\x1b\\" );
}

sub query_tcap($$) {
    my $tcap   = $_[0];
    my $tinfo  = $_[1];
    my $param1 = hexified($tcap);
    my $param2 = hexified($tinfo);
    my $reply;

    # uncomment one of the following lines
    if ( defined($opt_b) ) {
        $reply = get_reply( "\x1bP+q" . $param1 . ";" . $param2 . "\x1b\\" );
    }
    elsif ( defined($opt_i) ) {
        $reply = get_reply( "\x1bP+q" . $param2 . "\x1b\\" );
    }
    else {
        $reply = get_reply( "\x1bP+q" . $param1 . "\x1b\\" );
    }

    return unless defined $reply;
    if ( $reply =~ /\x1bP1\+r[[:xdigit:]]+=[[:xdigit:]]*.*/ ) {
        my $value = $reply;
        my $n;

        $value =~ s/^\x1bP1\+r//;
        $value =~ s/\x1b\\//;

        my $result = "";
        for ( $n = 0 ; $n < length($value) ; ) {
            my $c = substr( $value, $n, 1 );

            # handle semicolon and equals
            if ( $c =~ /[[:punct:]]/ ) {
                $n += 1;
                $result .= $c;
            }
            else {

                # handle hex-data
                my $k = hex substr( $value, $n, 2 );
                if ( $k == 0x1b ) {
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
                $n += 2;
            }
        }

        printf "%s\n", $result;
    }
}

# extended-keys are a feature of ncurses 5.0 and later
sub query_extended($) {
    my $name = $_[0];
    my $n;

    $name = "k" . $name if ( $name !~ /^k/ );

    for ( $n = 2 ; $n <= 7 ; ++$n ) {
        my $test = $name;
        $test = $test . $n if ( $n > 2 );
        query_tcap( $name, $test );
    }
}

query_tcap( "TN", "name" );
if ( defined($opt_t) ) {
    printf "Setting TERM=%s\n", $opt_t;
    modify_tcap($opt_t);
}

# See xtermcapKeycode()
if ( defined($opt_a) || defined($opt_c) ) {
    query_tcap( "ku", "kcuu1" );
    query_tcap( "kd", "kcud1" );
    query_tcap( "kr", "kcuf1" );
    query_tcap( "kl", "kcub1" );

    query_tcap( "kF", "kind" );
    query_tcap( "kR", "kri" );
    query_tcap( "%i", "kRIT" );
    query_tcap( "#4", "kLFT" );
}

if ( defined($opt_a) || defined($opt_e) ) {
    query_tcap( "kD", "kdch1" );
    query_tcap( "kI", "kich1" );

    query_tcap( "kh",  "khome" );
    query_tcap( "\@7", "kend" );
    query_tcap( "#2",  "kHOM" );
    query_tcap( "*7",  "kEND" );

    query_tcap( "*6",  "kslt" );
    query_tcap( "#6",  "kSLT" );
    query_tcap( "\@0", "kfnd" );
    query_tcap( "*0",  "kFND" );

    query_tcap( "kN", "knp" );
    query_tcap( "kP", "kpp" );

    query_tcap( "%c", "kNXT" );
    query_tcap( "%e", "kPRV" );
}

if ( defined($opt_a) || defined($opt_f) ) {
    query_tcap( "k1", "kf1" );
    query_tcap( "k2", "kf2" );
    query_tcap( "k3", "kf3" );
    query_tcap( "k4", "kf4" );
    query_tcap( "k5", "kf5" );
    query_tcap( "k6", "kf6" );
    query_tcap( "k7", "kf7" );
    query_tcap( "k8", "kf8" );
    query_tcap( "k9", "kf9" );
    query_tcap( "k;", "kf10" );
    query_tcap( "F1", "kf11" );
    query_tcap( "F2", "kf12" );
    query_tcap( "F3", "kf13" );
    query_tcap( "F4", "kf14" );
    query_tcap( "F5", "kf15" );
    query_tcap( "F6", "kf16" );
    query_tcap( "F7", "kf17" );
    query_tcap( "F8", "kf18" );
    query_tcap( "F9", "kf19" );
    query_tcap( "FA", "kf20" );
    query_tcap( "FB", "kf21" );
    query_tcap( "FC", "kf22" );
    query_tcap( "FD", "kf23" );
    query_tcap( "FE", "kf24" );
    query_tcap( "FF", "kf25" );
    query_tcap( "FG", "kf26" );
    query_tcap( "FH", "kf27" );
    query_tcap( "FI", "kf28" );
    query_tcap( "FJ", "kf29" );
    query_tcap( "FK", "kf30" );
    query_tcap( "FL", "kf31" );
    query_tcap( "FM", "kf32" );
    query_tcap( "FN", "kf33" );
    query_tcap( "FO", "kf34" );
    query_tcap( "FP", "kf35" );
    query_tcap( "FQ", "kf36" );
    query_tcap( "FR", "kf37" );
    query_tcap( "FS", "kf38" );
    query_tcap( "FT", "kf39" );
    query_tcap( "FU", "kf40" );
    query_tcap( "FV", "kf41" );
    query_tcap( "FW", "kf42" );
    query_tcap( "FX", "kf43" );
    query_tcap( "FY", "kf44" );
    query_tcap( "FZ", "kf45" );
    query_tcap( "Fa", "kf46" );
    query_tcap( "Fb", "kf47" );
    query_tcap( "Fc", "kf48" );
    query_tcap( "Fd", "kf49" );
    query_tcap( "Fe", "kf50" );
    query_tcap( "Ff", "kf51" );
    query_tcap( "Fg", "kf52" );
    query_tcap( "Fh", "kf53" );
    query_tcap( "Fi", "kf54" );
    query_tcap( "Fj", "kf55" );
    query_tcap( "Fk", "kf56" );
    query_tcap( "Fl", "kf57" );
    query_tcap( "Fm", "kf58" );
    query_tcap( "Fn", "kf59" );
    query_tcap( "Fo", "kf60" );
    query_tcap( "Fp", "kf61" );
    query_tcap( "Fq", "kf62" );
    query_tcap( "Fr", "kf63" );
}

if ( defined($opt_a) || defined($opt_k) ) {
    query_tcap( "K1", "ka1" );
    query_tcap( "K3", "ka3" );
    query_tcap( "K4", "kc1" );
    query_tcap( "K5", "kc3" );
}

if ( defined($opt_a) || defined($opt_m) ) {
    query_tcap( "kB", "kcbt" );
    query_tcap( "kC", "kclr" );
    query_tcap( "&8", "kund" );

    query_tcap( "kb", "kbs" );

    query_tcap( "%1", "khlp" );
    query_tcap( "#1", "kHLP" );

    query_tcap( "Co", "colors" );
}

if ( defined($opt_x) ) {
    query_extended($opt_x);
}

if ( defined($opt_X) ) {
    if ( defined($opt_c) ) {
        query_extended("DN");
        query_extended("UP");
        query_extended("LFT");
        query_extended("RIT");
    }
    if ( defined($opt_e) ) {
        query_extended("DC");
        query_extended("END");
        query_extended("HOM");
        query_extended("IC");
        query_extended("NXT");
        query_extended("PRV");
    }
}
