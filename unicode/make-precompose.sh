#!/bin/sh
# $XTermId: make-precompose.sh,v 1.6 2007/02/05 01:06:36 Thomas.Wolff Exp $
# $XFree86: xc/programs/xterm/unicode/make-precompose.sh,v 1.4 2005/03/29 04:00:32 tsi Exp $

cat precompose.c.head | sed -e's/@/$/g'

# extract canonical decomposition data from UnicodeData.txt,
# pad hex values to 5 digits,
# sort numerically on base character, then combining character,
# then reduce to 4 digits again where possible
cut UnicodeData.txt -d ";" -f 1,6 |
 grep ";[0-9,A-F]" | grep " " |
 sed -e "s/ /, 0x/;s/^/{ 0x/;s/;/, 0x/;s/$/},/" |
 sed -e "s,0x\(....\)\([^0-9A-Fa-f]\),0x0\1\2,g" |
 (sort -k 3 || sort +2) |
 sed -e "s,0x0\(...[0-9A-Fa-f]\),0x\1,g"

cat precompose.c.tail
