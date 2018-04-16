#!/usr/bin/env perl
# $XTermId: closest-rgb.pl,v 1.9 2017/01/20 22:17:04 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2017 by Thomas E. Dickey
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
# For a given RGB value, show its distance from xterm's 88/256-color
# models or alternatively against rgb.txt

use strict;
use warnings;
use diagnostics;

use Getopt::Std;

our $namedRGB = "/etc/X11/rgb.txt";
our @namedRGB;
our @xtermRGB;

our ( $opt_f, $opt_i, $opt_n );
&getopts('f:in:') || die(
    "Usage: $0 [options]\n
Options:\n
  -f FILE pathname for rgb.txt (default $namedRGB)
  -i      reverse comparison, look for rgb matches in xterm's palette
  -n NUM  number of colors in palette (default: 16)
"
);
$opt_f = $namedRGB unless ($opt_f);
$opt_n = 16        unless ($opt_n);

sub value_of($) {
    my $text  = shift;
    my $value = (
        ( $text =~ /^0[0-7]*$/ ) ? ( oct $text )
        : (
            ( $text =~ /^\d+$/ ) ? $text
            : hex $text
        )
    );
}

sub lookup($) {
    my $value = shift;

    chomp $value;
    $value =~ s/^\s*//;
    $value =~ s/\s*$//;

    my $rgb = $value;
    $rgb =~ s/^((\w+\s+){2,2}(\w+)).*/$1/;
    my @rgb = split /\s+/, $rgb;

    my $name = $value;
    $name =~ s/^((\w+\s+){3,3})//;

    my %result;
    $result{R}    = &value_of( $rgb[0] );
    $result{G}    = &value_of( $rgb[1] );
    $result{B}    = &value_of( $rgb[2] );
    $result{NAME} = $name;
    return \%result;
}

sub xterm16() {
    my @result;
    my $o = 0;
    $result[ $o++ ] = &lookup("0 0 0 black");
    $result[ $o++ ] = &lookup("205 0 0 red3");
    $result[ $o++ ] = &lookup("0 205 0 green3");
    $result[ $o++ ] = &lookup("205 205 0 yellow3");
    $result[ $o++ ] = &lookup("0 0 238 blue2");
    $result[ $o++ ] = &lookup("205 0 205 magenta3");
    $result[ $o++ ] = &lookup("0 205 205 cyan3");
    $result[ $o++ ] = &lookup("229 229 229 gray90");
    $result[ $o++ ] = &lookup("127 127 127 gray50");
    $result[ $o++ ] = &lookup("255 0 0 red");
    $result[ $o++ ] = &lookup("0 255 0 green");
    $result[ $o++ ] = &lookup("255 255 0 yellow");
    $result[ $o++ ] = &lookup("0x5b 0x5c 0xff xterm blue");
    $result[ $o++ ] = &lookup("255 0 255 magenta");
    $result[ $o++ ] = &lookup("0 255 255 cyan");
    $result[ $o++ ] = &lookup("255 255 255 white");
    return @result;
}

sub xtermRGB($) {
    my $base = shift;

    my ( $cube, $cube1, $cube2 ) = $base    #
      ? ( 6, 40, 55 )                       #
      : ( 4, 16, 4 );
    my ( $ramp, $ramp1, $ramp2 ) = $base    #
      ? ( 24, 10, 8 )                       #
      : ( 8, 23.18181818, 46.36363636 );

    my @result = &xterm16;
    my $o      = 16;

    my $red;
    my $green;
    my $blue;
    my $gray;

    for ( $red = 0 ; $red < $cube ; $red++ ) {
        for ( $green = 0 ; $green < $cube ; $green++ ) {
            for ( $blue = 0 ; $blue < $cube ; $blue++ ) {
                my %data;
                $data{R} = ( $red   ? ( $red * $cube1 + $cube2 )   : 0 );
                $data{G} = ( $green ? ( $green * $cube1 + $cube2 ) : 0 );
                $data{B} = ( $blue  ? ( $blue * $cube1 + $cube2 )  : 0 );
                $data{NAME} = sprintf "cube %d,%d,%d", $red, $green, $blue;
                $result[ $o++ ] = \%data;
            }
        }
    }

    for ( $gray = 0 ; $gray < $ramp ; $gray++ ) {
        my $level = ( $gray * $ramp1 ) + $ramp2;
        my %data;
        $data{R}        = $level;
        $data{G}        = $level;
        $data{B}        = $level;
        $data{NAME}     = sprintf "ramp %d", $gray;
        $result[ $o++ ] = \%data;
    }

    return @result;
}

sub xterm88() {
    return &xtermRGB(0);
}

sub xterm256() {
    return &xtermRGB(1);
}

sub load_namedRGB($) {
    my $file = shift;
    open my $fp, $file || die "cannot open $file";
    my @data = <$fp>;
    close $fp;
    my @result;
    my $o = 0;
    for my $i ( 0 .. $#data ) {
        next if ( $data[$i] =~ /^\s*[[:punct:]]/ );

        $result[ $o++ ] = &lookup( $data[$i] );
    }
    return @result;
}

sub distance($$) {
    my %a      = %{ $_[0] };
    my %b      = %{ $_[1] };
    my $R      = $a{R} - $b{R};
    my $G      = $a{G} - $b{G};
    my $B      = $a{B} - $b{B};
    my $result = sqrt( $R * $R + $G * $G + $B * $B );
}

sub show_distances($$) {
    my @ref = @{ $_[0] };
    my @cmp = @{ $_[1] };
    for my $c ( 0 .. $#cmp ) {
        my %cmp  = %{ $cmp[$c] };
        my $best = -1;
        my %best;
        for my $r ( 0 .. $#ref ) {
            my %ref = %{ $ref[$r] };
            my $test = &distance( \%ref, \%cmp );
            if ( $best < 0 ) {
                $best = $test;
                %best = %ref;
            }
            elsif ( $best > $test ) {
                $best = $test;
                %best = %ref;
            }
        }
        printf "%3d %-25s %5.1f   %s\n", $c, $cmp{NAME}, $best, $best{NAME};
    }
}

@namedRGB = &load_namedRGB($opt_f);
printf "%d names from $opt_f\n", $#namedRGB + 1;

if ( $opt_n <= 16 ) {
    @xtermRGB = &xterm16;
}
elsif ( $opt_n <= 88 ) {
    @xtermRGB = &xterm88;
}
else {
    @xtermRGB = &xterm256;
}
printf "%d names from xterm palette\n", $#xtermRGB + 1;

&show_distances( \@xtermRGB, \@namedRGB ) if ($opt_i);
&show_distances( \@namedRGB, \@xtermRGB ) unless ($opt_i);

1;
