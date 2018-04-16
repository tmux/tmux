#!/bin/sh
# $XTermId: doublechars.sh,v 1.17 2011/12/11 16:21:22 tom Exp $
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
# Illustrate the use of double-size characters by drawing successive lines in
# the commonly used video attributes.
#
# Use the -w option to force the output to wrap.  It will look ugly, because
# the double-high lines will be split.

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

SAVE=yes
WRAP=no
if test $# != 0 ; then
    while test $# != 0
    do
    	case $1 in
	-n)	SAVE=no ;;
	-w)	WRAP=yes ;;
	esac
	shift
    done
fi

if test $SAVE = yes ; then
    exec </dev/tty
    old=`stty -g`
    stty raw -echo min 0  time 5

    $CMD $OPT "${ESC}[18t${SUF}" > /dev/tty
    IFS=';' read junk high wide

    stty $old

    wide=`echo $wide|sed -e 's/t.*//'`
    original=${ESC}[8\;${high}\;${wide}t${SUF}

    if ( trap "echo exit" EXIT 2>/dev/null ) >/dev/null
    then
	trap '$CMD $OPT "$original" >/dev/tty; exit' EXIT HUP INT TRAP TERM
    else
	trap '$CMD $OPT "$original" >/dev/tty; exit' 0    1   2   5    15
    fi

fi

if test $WRAP = yes ; then
	# turn on wrapping and force the screen to 80 columns
	$CMD $OPT "${ESC}[?7h" >/dev/tty
	$CMD $OPT "${ESC}[?40l" >/dev/tty
else
	# force the screen to 132 columns
	$CMD $OPT "${ESC}[?40h" >/dev/tty
	$CMD $OPT "${ESC}[?3h" >/dev/tty
fi

for SGR in 0 1 4 5 7
do
	$CMD $OPT "${ESC}[0;${SGR}m" >/dev/tty
	for DBL in 5 3 4 6 5
	do
		$CMD $OPT "${ESC}#${DBL}" >/dev/tty
		echo "The quick brown fox jumps over the lazy dog" >/dev/tty
	done
	echo
done
$CMD $OPT "${ESC}[0m" >/dev/tty
