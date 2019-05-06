#!/bin/bash
#
#   This file echoes four gradients with 24-bit color codes
#   to the terminal to demonstrate their functionality.
#   The foreground escape sequence is ^[38;2;<r>;<g>;<b>m
#   The background escape sequence is ^[48;2;<r>;<g>;<b>m
#   <r> <g> <b> range from 0 to 255 inclusive.
#   The escape sequence ^[0m returns output to default

#
# From
# https://github.com/gnachman/iTerm2/blob/master/tests/24-bit-color.sh
# and presumably covered by
# https://github.com/gnachman/iTerm2/blob/master/LICENSE
#

SEQ1=
if which gseq >/dev/null 2>&1; then
    SEQ1=gseq
elif seq --version|grep -q GNU; then
    SEQ1=seq
fi
if [ -n "$SEQ1" ]; then
    # GNU seq requires a -ve increment if going backwards
    seq1()
    {
        if [ $1 -gt $2 ]; then
	    $SEQ1 $1 -1 $2
	else
	    $SEQ1 $1 $2
	fi
    }
    SEQ=seq1
else
    SEQ=seq
fi	
SEPARATOR=':'

setBackgroundColor()
{
    echo -en "\033[48${SEPARATOR}2${SEPARATOR}$1${SEPARATOR}$2${SEPARATOR}$3""m"
}

resetOutput()
{
    echo -en "\033[0m\n"
}

# Gives a color $1/255 % along HSV
# Who knows what happens when $1 is outside 0-255
# Echoes "$red $green $blue" where
# $red $green and $blue are integers
# ranging between 0 and 255 inclusive
rainbowColor()
{ 
    let h=$1/43
    let f=$1-43*$h
    let t=$f*255/43
    let q=255-t

    if [ $h -eq 0 ]
    then
        echo "255 $t 0"
    elif [ $h -eq 1 ]
    then
        echo "$q 255 0"
    elif [ $h -eq 2 ]
    then
        echo "0 255 $t"
    elif [ $h -eq 3 ]
    then
        echo "0 $q 255"
    elif [ $h -eq 4 ]
    then
        echo "$t 0 255"
    elif [ $h -eq 5 ]
    then
        echo "255 0 $q"
    else
        # execution should never reach here
        echo "0 0 0"
    fi
}

for i in `$SEQ 0 127`; do
    setBackgroundColor $i 0 0
    echo -en " "
done
resetOutput
for i in `$SEQ 255 128`; do
    setBackgroundColor $i 0 0
    echo -en " "
done
resetOutput

for i in `$SEQ 0 127`; do
    setBackgroundColor 0 $i 0
    echo -n " "
done
resetOutput
for i in `$SEQ 255 128`; do
    setBackgroundColor 0 $i 0
    echo -n " "
done
resetOutput

for i in `$SEQ 0 127`; do
    setBackgroundColor 0 0 $i
    echo -n " "
done
resetOutput
for i in `$SEQ 255 128`; do
    setBackgroundColor 0 0 $i
    echo -n " "
done
resetOutput

for i in `$SEQ 0 127`; do
    setBackgroundColor `rainbowColor $i`
    echo -n " "
done
resetOutput
for i in `$SEQ 255 128`; do
    setBackgroundColor `rainbowColor $i`
    echo -n " "
done
resetOutput
