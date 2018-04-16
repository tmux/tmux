#!/bin/sh
# $XTermId: dynamic2.sh,v 1.3 2011/12/11 16:21:22 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2006,2011 by Thomas E. Dickey
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
# Demonstrate the use of dynamic colors by setting each dynamic color
# successively to different values.

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

LIST="00 30 80 d0 ff"
FULL="10 11 12 13 14 15 16 17 18"

echo "reading current color settings"

exec </dev/tty
old=`stty -g`
stty raw -echo min 0  time 5

original=
for N in $FULL
do
    $CMD $OPT "${ESC}]$N;?${SUF}" > /dev/tty
    read reply
    eval original$N='${reply}${SUF}'
    original=${original}${reply}${SUF}
done
stty $old

if ( trap "echo exit" EXIT 2>/dev/null ) >/dev/null
then
    trap '$CMD $OPT "$original" >/dev/tty; exit' EXIT HUP INT TRAP TERM
else
    trap '$CMD $OPT "$original" >/dev/tty; exit' 0    1   2   5    15
fi

while true
do
    for N in $FULL
    do
	case $N in
	10) echo "coloring text foreground";;
	11) echo "coloring text background";;
	12) echo "coloring text cursor";;
	13) echo "coloring mouse foreground";;
	14) echo "coloring mouse background";;
	15) echo "coloring tektronix foreground";;
	16) echo "coloring tektronix background";;
	17) echo "coloring highlight background";;
	18) echo "coloring tektronix cursor";;
	esac
	for R in $LIST
	do
	    for G in $LIST
	    do
		for B in $LIST
		do
		    $CMD $OPT "${ESC}]$N;rgb:$R/$G/$B${SUF}" >/dev/tty
		    sleep 1
		done
	    done
	done
	eval 'restore=$'original$N
	$CMD $OPT "$restore" >/dev/tty
	sleep 1
    done
done
