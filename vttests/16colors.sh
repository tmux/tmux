#!/bin/sh
# $XTermId: 16colors.sh,v 1.14 2011/12/11 16:21:22 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 1999-2003,2011 by Thomas E. Dickey
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
# Show a simple 16-color test pattern.  It is a little more confusing than
# 8colors.sh, since everything is abbreviated to fit on an 80-column line.
# The high (8-15) combinations for foreground or background are marked with
# a '+' sign.

ESC=""
CMD='/bin/echo'
OPT='-n'
SUF=''
TMP=/tmp/xterm$$
eval '$CMD $OPT >$TMP || echo fail >$TMP' 2>/dev/null
( test ! -f $TMP || test -s $TMP ) &&
for verb in printf print ; do
    rm -f $TMP
    eval '$verb "\c" >$TMP || echo fail >$TMP' 2>/dev/null
    if test -f $TMP ; then
	if test ! -s $TMP ; then
	    CMD="$verb"
	    OPT=
	    SUF='\c'
	    break
	fi
    fi
done
rm -f $TMP

if ( trap "echo exit" EXIT 2>/dev/null ) >/dev/null
then
    trap '$CMD "[0m"; exit' EXIT HUP INT TRAP TERM
else
    trap '$CMD "[0m"; exit' 0    1   2   5    15
fi

echo "[0m"
while true
do
    for AT in 0 1 4 7
    do
    	case $AT in
	0) attr="   ";;
	1) attr="BO ";;
	4) attr="UN ";;
	7) attr="RV ";;
	esac
	for FG in 0 1 2 3 4 5 6 7
	do
	    case $FG in
	    0) fcolor="BLK ";;
	    1) fcolor="RED ";;
	    2) fcolor="GRN ";;
	    3) fcolor="YEL ";;
	    4) fcolor="BLU ";;
	    5) fcolor="MAG ";;
	    6) fcolor="CYN ";;
	    7) fcolor="WHT ";;
	    esac
	    for HI in 3 9
	    do
	    	if test $HI = 3 ; then
		    color=" $fcolor"
		else
		    color="+$fcolor"
		fi
		$CMD $OPT "[0;${AT}m$attr$SUF"
		$CMD $OPT "[${HI}${FG}m$color$SUF"
		for BG in 1 2 3 4 5 6 7
		do
		    case $BG in
		    0) bcolor="BLK ";;
		    1) bcolor="RED ";;
		    2) bcolor="GRN ";;
		    3) bcolor="YEL ";;
		    4) bcolor="BLU ";;
		    5) bcolor="MAG ";;
		    6) bcolor="CYN ";;
		    7) bcolor="WHT ";;
		    esac
		    $CMD $OPT "[4${BG}m$bcolor$SUF"
		    $CMD $OPT "[10${BG}m+$bcolor$SUF"
		done
		echo "[0m"
	    done
	done
	sleep 1
    done
done
