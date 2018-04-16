#!/usr/bin/env perl
# $XTermId: 256colors2.pl,v 1.23 2016/12/10 22:38:26 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 1999-2014,2016 by Thomas E. Dickey
# Copyright 2002 by Steve Wall
# Copyright 1999 by Todd Larason
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
#
# If -s is not given, use the resources for colors 0-15 - usually more-or-less
# a reproduction of the standard ANSI colors, but possibly more pleasing
# shades.

use strict;
use warnings;

use Getopt::Std;
use Encode 'encode_utf8';

our ( $opt_8, $opt_c, $opt_d, $opt_h, $opt_q, $opt_r, $opt_s, $opt_u );
&getopts('8cdhqrsu') || die("Usage: $0 [options]");
die(
    "Usage: $0 [options]\n
Options:
  -8  use 8-bit controls
  -c  use colons for separating parameter values in SGR 38/48
  -d  use rgb values rather than palette index
  -h  display this message
  -q  quieter output by merging all palette initialization
  -r  display the reverse of the usual palette
  -s  modify system colors, i.e., 0..15
  -u  use UTF-8 when emitting 8-bit controls
"
) if ($opt_h);

our $cube = 6;
our (@steps);
our ( $red,  $green, $blue );
our ( $gray, $level, $color );
our ( $csi,  $osc,   $sep, $st );

our @rgb;

sub map_cube($) {
    my $value = $_[0];
    $value = ( 5 - $value ) if defined($opt_r);
    return $value;
}

sub map_gray($) {
    my $value = $_[0];
    $value = ( 23 - $value ) if defined($opt_r);
    return $value;
}

sub define_color($$$$) {
    my $index = $_[0];
    my $r     = $_[1];
    my $g     = $_[2];
    my $b     = $_[3];

    printf( "%s4", $osc ) unless ($opt_q);
    printf( ";%d;rgb:%2.2x/%2.2x/%2.2x", $index, $r, $g, $b );
    printf( "%s", $st ) unless ($opt_q);

    $rgb[$index] = sprintf "%d%s%d%s%d", $r, $sep, $g, $sep, $b;
}

sub select_color($) {
    my $index = $_[0];
    if ( $opt_d and defined( $rgb[$index] ) ) {
        printf "%s48;2%s%sm  ", $csi, $sep, $rgb[$index];
    }
    else {
        printf "%s48;5%s%sm  ", $csi, $sep, $index;
    }
}

sub system_color($$$$) {
    my $color = shift;
    my $red   = shift;
    my $green = shift;
    my $blue  = shift;
    &define_color( 15 - $color, $red, $green, $blue ) if ($opt_r);
    &define_color( $color, $red, $green, $blue ) unless ($opt_r);
}

if ($opt_8) {
    $csi = "\x9b";
    $osc = "\x9d";
    $st  = "\x9c";
}
else {
    $csi = "\x1b[";
    $osc = "\x1b]";
    $st  = "\x1b\\";
}

if ($opt_c) {
    $sep = ":";
}
else {
    $sep = ";";
}

if ( $opt_8 and $opt_u ) {
    my $lc_ctype = `locale 2>/dev/null | fgrep LC_CTYPE | sed -e 's/^.*=//'`;
    if ( $lc_ctype =~ /utf.?8/i ) {
        binmode( STDOUT, ":utf8" );
    }
}

printf( "%s4", $osc ) if ($opt_q);
if ($opt_s) {
    &system_color( 0,  0,   0,   0 );
    &system_color( 1,  205, 0,   0 );
    &system_color( 2,  0,   205, 0 );
    &system_color( 3,  205, 205, 0 );
    &system_color( 4,  0,   0,   238 );
    &system_color( 5,  205, 0,   205 );
    &system_color( 6,  0,   205, 205 );
    &system_color( 7,  229, 229, 229 );
    &system_color( 8,  127, 127, 127 );
    &system_color( 9,  255, 0,   0 );
    &system_color( 10, 0,   255, 0 );
    &system_color( 11, 255, 255, 0 );
    &system_color( 12, 92,  92,  255 );
    &system_color( 13, 255, 0,   255 );
    &system_color( 14, 0,   255, 255 );
    &system_color( 15, 255, 255, 255 );
}

# colors 16-231 are a 6x6x6 color cube
@steps = ( 0, 95, 135, 175, 215, 255 );
for ( $red = 0 ; $red < $cube ; $red++ ) {
    for ( $green = 0 ; $green < $cube ; $green++ ) {
        for ( $blue = 0 ; $blue < $cube ; $blue++ ) {
            &define_color(
                16 +
                  ( map_cube($red) * $cube * $cube ) +
                  ( map_cube($green) * $cube ) +
                  map_cube($blue),
                int( $steps[$red] ),
                int( $steps[$green] ),
                int( $steps[$blue] )
            );
        }
    }
}

# colors 232-255 are a grayscale ramp, intentionally leaving out
# black and white
for ( $gray = 0 ; $gray < 24 ; $gray++ ) {
    $level = ( map_gray($gray) * 10 ) + 8;
    &define_color( 232 + $gray, $level, $level, $level );
}
printf( "%s", $st ) if ($opt_q);

# display the colors

# first the system ones:
print "System colors:\n";
for ( $color = 0 ; $color < 8 ; $color++ ) {
    &select_color($color);
}
printf "%s0m\n", $csi;
for ( $color = 8 ; $color < 16 ; $color++ ) {
    &select_color($color);
}
printf "%s0m\n\n", $csi;

# now the color cube
print "Color cube, ${cube}x${cube}x${cube}:\n";
for ( $green = 0 ; $green < $cube ; $green++ ) {
    for ( $red = 0 ; $red < $cube ; $red++ ) {
        for ( $blue = 0 ; $blue < $cube ; $blue++ ) {
            $color = 16 + ( $red * $cube * $cube ) + ( $green * $cube ) + $blue;
            &select_color($color);
        }
        printf "%s0m ", $csi;
    }
    print "\n";
}

# now the grayscale ramp
print "Grayscale ramp:\n";
for ( $color = 232 ; $color < 256 ; $color++ ) {
    &select_color($color);
}
printf "%s0m\n", $csi;
