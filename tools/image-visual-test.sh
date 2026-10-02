#!/bin/sh

# One-screen visual check for tmux image input and layering. Run this inside a
# tmux pane attached to Kitty, WezTerm, Microsoft Terminal, or a basic terminal.

if [ -z "$TMUX" ]; then
	echo "Run this script inside tmux." >&2
	exit 1
fi

columns=$(tput cols 2>/dev/null || echo 0)
lines=$(tput lines 2>/dev/null || echo 0)
if [ "$columns" -lt 72 ] || [ "$lines" -lt 23 ]; then
	echo "This test needs at least 72 columns and 23 rows." >&2
	exit 1
fi

at()
{
	printf '\033[%s;%sH' "$1" "$2"
}

kitty()
{
	printf '\033_G%s\033\\' "$1"
}

# Transmit a 16x16 solid RGBA texture. A one-pixel texture is unsuitable here
# because Kitty filters its transparent texture border into scaled edges.
kitty_solid()
{
	id=$1
	rgba=$2
	payload=$(i=0; while [ "$i" -lt 256 ]; do
		printf "$rgba"
		i=$((i + 1))
	done | base64 | tr -d '\n')
	kitty "a=t,q=2,f=32,s=16,v=16,i=$id;$payload"
}

# A small 26x26 transparent SIXEL folder, used by the supplied bptato tests.
sixel_folder()
{
	printf '\033P0;1q"1;1;26;26#0;2;50;50;50#1;2;0;0;0#2;2;49;49;49#3;2;33;33;33#4;2;47;47;47#5;2;44;44;44#6;2;48;48;48#2?O?C!5?S!4?__?_?__$#0?_wW!5KGwo_-#0?~~!8?!10@BB}w$#2!12?a_A?A?A?A??@E-#0?~~!7?KK~~KK!7?~~$#2!9?K!6?K-#0?F^[owowowowowowowowooS^F$#2?G!10?@@!7?G??G-\033\\'
}

feature_info=$(tmux display-message -p \
    'term=#{client_termname} features=#{client_termfeatures} cells=#{client_cell_width}x#{client_cell_height}')

printf '\033[2J\033[H\033[?25l'
at 1 2
printf 'tmux image visual check'
at 2 2
printf '%s' "$feature_info"

# Red is placed after its text at a positive Kitty z-index, so it must cover
# the text. Green is placed below the text at a negative Kitty z-index.
kitty_solid 101 '\377\000\000\377'
kitty_solid 102 '\000\377\000\377'
kitty_solid 103 '\000\000\377\377'
kitty_solid 104 '\377\000\000\177'

at 4 2
printf '1. positive Kitty z: RED must hide TEXT UNDER at right'
at 4 62
printf 'TEXT UNDER'
at 4 60
kitty 'a=p,q=2,C=1,i=101,p=1,x=1,y=1,w=14,h=14,c=18,r=2,z=1'

at 7 2
printf '2. negative Kitty z: GREEN remains behind TEXT ON TOP'
at 7 60
kitty 'a=p,q=2,C=1,i=102,p=2,x=1,y=1,w=14,h=14,c=18,r=2,z=-1'
at 7 62
printf 'TEXT ON TOP'

at 10 2
printf '3. Kitty z overlap: RED left, BLUE on top at right'
at 11 55
kitty 'a=p,q=2,C=1,i=101,p=3,x=1,y=1,w=14,h=14,c=18,r=3,z=1'
at 11 64
kitty 'a=p,q=2,C=1,i=103,p=4,x=1,y=1,w=14,h=14,c=9,r=3,z=2'

at 15 2
printf '4. alpha: half-transparent RED, dithered on SIXEL'
at 15 60
kitty 'a=p,q=2,C=1,i=104,p=5,x=1,y=1,w=14,h=14,c=18,r=2,z=1'

# SIXEL semantics are temporal: later text destroys a SIXEL image, while a
# later SIXEL image appears over existing text.
at 18 2
printf '5. SIXEL after text: folder covers FOLDER at right'
at 18 65
printf 'FOLDER'
at 18 65
sixel_folder

at 21 2
printf '6. SIXEL before text: no folder remains behind TEXT WINS'
at 21 65
sixel_folder
at 21 65
printf 'TEXT WINS'

at 23 2
printf 'Take one screenshot, then press Enter to return to the shell.'
at 23 66
printf '\033[?25h'
IFS= read -r _image_visual_reply
