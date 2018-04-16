#!/usr/bin/perl -w
# $XTermId: convmap.pl,v 1.14 2016/05/22 19:57:09 tom Exp $
#
# Generate keysym2ucs.c file
#
# See also:
# http://mail.nl.linux.org/linux-utf8/2001-04/msg00248.html
#
# $XFree86: xc/programs/xterm/unicode/convmap.pl,v 1.5 2000/01/24 22:22:05 dawes Exp $

use strict;

our $keysym;
our %name;
our %keysym_to_ucs;
our %keysym_to_keysymname;

sub utf8 ($);

sub utf8 ($) {
    my $c = shift(@_);

    if ($c < 0x80) {
        return sprintf("%c", $c);
    } elsif ($c < 0x800) {
        return sprintf("%c%c", 0xc0 | ($c >> 6), 0x80 | ($c & 0x3f));
    } elsif ($c < 0x10000) {
        return sprintf("%c%c%c",
                       0xe0 |  ($c >> 12),
                       0x80 | (($c >>  6) & 0x3f),
                       0x80 | ( $c        & 0x3f));
    } elsif ($c < 0x200000) {
        return sprintf("%c%c%c%c",
                       0xf0 |  ($c >> 18),
                       0x80 | (($c >> 12) & 0x3f),
                       0x80 | (($c >>  6) & 0x3f),
                       0x80 | ( $c        & 0x3f));
    } elsif ($c < 0x4000000) {
        return sprintf("%c%c%c%c%c",
                       0xf8 |  ($c >> 24),
                       0x80 | (($c >> 18) & 0x3f),
                       0x80 | (($c >> 12) & 0x3f),
                       0x80 | (($c >>  6) & 0x3f),
                       0x80 | ( $c        & 0x3f));

    } elsif ($c < 0x80000000) {
        return sprintf("%c%c%c%c%c%c",
                       0xfe |  ($c >> 30),
                       0x80 | (($c >> 24) & 0x3f),
                       0x80 | (($c >> 18) & 0x3f),
                       0x80 | (($c >> 12) & 0x3f),
                       0x80 | (($c >> 6)  & 0x3f),
                       0x80 | ( $c        & 0x3f));
    } else {
        return utf8(0xfffd);
    }
}

my $unicodedata = "UnicodeData.txt";

# read list of all Unicode names
if (!open(UDATA, $unicodedata) && !open(UDATA, "$unicodedata")) {
    die ("Can't open Unicode database '$unicodedata':\n$!\n\n" .
         "Please make sure that you have downloaded the file\n" .
         "ftp://ftp.unicode.org/Public/UNIDATA/UnicodeData.txt\n");
}
while (<UDATA>) {
    if (/^([0-9,A-F]{4,6});([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*);([^;]*)$/) {
        $name{hex($1)} = $2;
    } else {
        die("Syntax error in line '$_' in file '$unicodedata'");
    }
}
close(UDATA);

# read mapping (from http://wsinwp07.win.tue.nl:1234/unicode/keysym.map)
open(LIST, "<keysym.map") || die ("Can't open map file:\n$!\n");
while (<LIST>) {
    if (/^0x([0-9a-f]{4})\s+U([0-9a-f]{4})\s*(\#.*)?$/){
        my $keysym = hex($1);
        my $ucs = hex($2);
	my $comment = $3;
	$comment =~ s/^#\s*//;
        $keysym_to_ucs{$keysym} = $ucs;
	$keysym_to_keysymname{$keysym} = $comment;
    } elsif (/^\s*\#/ || /^\s*$/) {
    } else {
        die("Syntax error in 'list' in line\n$_\n");
    }
}
close(LIST);

# read entries in keysymdef.h
open(LIST, "</usr/include/X11/keysymdef.h") || die ("Can't open keysymdef.h:\n$!\n");
while (<LIST>) {
    if (/^\#define\s+XK_([A-Za-z_0-9]+)\s+0x([0-9a-fA-F]+)\s*(\/.*)?$/) {
	next if /\/\* deprecated \*\//;
	my $keysymname = $1;
	my $keysym = hex($2);
	$keysym_to_keysymname{$keysym} = $keysymname;
    }
}
close(LIST);

print <<EOT;
/* \$XTermId\$
 * This module converts keysym values into the corresponding ISO 10646
 * (UCS, Unicode) values.
 *
 * The array keysymtab[] contains pairs of X11 keysym values for graphical
 * characters and the corresponding Unicode value. The function
 * keysym2ucs() maps a keysym onto a Unicode value using a binary search,
 * therefore keysymtab[] must remain SORTED by keysym value.
 *
 * The keysym -> UTF-8 conversion will hopefully one day be provided
 * by Xlib via XmbLookupString() and should ideally not have to be
 * done in X applications. But we are not there yet.
 *
 * We allow to represent any UCS character in the range U-00000000 to
 * U-00FFFFFF by a keysym value in the range 0x01000000 to 0x01ffffff.
 * This admittedly does not cover the entire 31-bit space of UCS, but
 * it does cover all of the characters up to U-10FFFF, which can be
 * represented by UTF-16, and more, and it is very unlikely that higher
 * UCS codes will ever be assigned by ISO. So to get Unicode character
 * U+ABCD you can directly use keysym 0x0100abcd.
 *
 * NOTE: The comments in the table below contain the actual character
 * encoded in UTF-8, so for viewing and editing best use an editor in
 * UTF-8 mode.
 *
 * Author: Markus G. Kuhn <mkuhn\@acm.org>, University of Cambridge, April 2001
 *
 * Special thanks to Richard Verhoeven <river\@win.tue.nl> for preparing
 * an initial draft of the mapping table.
 *
 * This software is in the public domain. Share and enjoy!
 *
 * AUTOMATICALLY GENERATED FILE, DO NOT EDIT !!! (unicode/convmap.pl)
 */

#ifndef KEYSYM2UCS_INCLUDED
  
#include "keysym2ucs.h"
#define VISIBLE /* */

#else

#define VISIBLE static

#endif

static struct codepair {
  unsigned short keysym;
  unsigned short ucs;
} keysymtab[] = {
EOT

for $keysym (sort {$a <=> $b} keys(%keysym_to_keysymname)) {
    my $ucs = $keysym_to_ucs{$keysym};
    next if $keysym >= 0xf000 || $keysym < 0x100;
    if ($ucs) {
	printf("  { 0x%04x, 0x%04x }, /*%28s %s %s */\n",
	       $keysym, $ucs, $keysym_to_keysymname{$keysym}, utf8($ucs),
	       defined($name{$ucs}) ? $name{$ucs} : "???" );
    } else {
	printf("/*  0x%04x   %39s ? ??? */\n",
	       $keysym, $keysym_to_keysymname{$keysym});
    }
}

print <<EOT;
};

VISIBLE
long keysym2ucs(KeySym keysym)
{
    int min = 0;
    int max = sizeof(keysymtab) / sizeof(struct codepair) - 1;

    /* first check for Latin-1 characters (1:1 mapping) */
    if ((keysym >= 0x0020 && keysym <= 0x007e) ||
        (keysym >= 0x00a0 && keysym <= 0x00ff))
        return keysym;

    /* also check for directly encoded 24-bit UCS characters */
    if ((keysym & 0xff000000) == 0x01000000)
	return keysym & 0x00ffffff;

    /* binary search in table */
    while (max >= min) {
	int mid = (min + max) / 2;
	if (keysymtab[mid].keysym < keysym)
	    min = mid + 1;
	else if (keysymtab[mid].keysym > keysym)
	    max = mid - 1;
	else {
	    /* found it */
	    return keysymtab[mid].ucs;
	}
    }

    /* no matching Unicode value found */
    return -1;
}
EOT
