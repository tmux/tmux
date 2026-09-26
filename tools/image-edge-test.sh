#!/bin/sh

# Compact visual check for image subcell geometry and layering. Run inside a
# tmux pane on Kitty or a SIXEL terminal; it needs at least 90 columns/24 rows.

if [ -z "$TMUX" ]; then
	echo "Run this script inside tmux." >&2
	exit 1
fi

columns=$(tput cols 2>/dev/null || echo 0)
lines=$(tput lines 2>/dev/null || echo 0)
if [ "$columns" -lt 90 ] || [ "$lines" -lt 24 ]; then
	echo "This test needs at least 90 columns and 24 rows." >&2
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

sixel_folder()
{
	printf '\033P0;1q"1;1;26;26#0;2;50;50;50#1;2;0;0;0#2;2;49;49;49#3;2;33;33;33#4;2;47;47;47#5;2;44;44;44#6;2;48;48;48#2?O?C!5?S!4?__?_?__$#0?_wW!5KGwo_-#0?~~!8?!10@BB}w$#2!12?a_A?A?A?A??@E-#0?~~!7?KK~~KK!7?~~$#2!9?K!6?K-#0?F^[owowowowowowowowooS^F$#2?G!10?@@!7?G??G-\033\\'
}

feature_info=$(tmux display-message -p \
    'term=#{client_termname} features=#{client_termfeatures} cells=#{client_cell_width}x#{client_cell_height}')

printf '\033[2J\033[H\033[?25l'
at 1 2
printf 'tmux image edge-case check'
at 2 2
printf '%s' "$feature_info"

kitty_solid 201 '\377\000\000\377'
kitty_solid 202 '\000\377\000\377'
kitty_solid 203 '\000\000\377\377'

# The 26-pixel folder occupies whole grid cells, but its visible right and
# bottom edges must remain partial rather than stretching to those boundaries.
at 4 2
printf '1. partial SIXEL: folder should be the same size as direct output'
at 4 72
sixel_folder

# Chawan emits X/Y for its image padding. The red square must begin slightly
# below and right of the [img] anchor, never over it.
at 8 2
printf '2. Kitty X/Y: RED begins below/right of the [img] anchor'
at 8 72
printf '[img]'
at 9 72
kitty 'a=p,q=2,C=1,i=201,p=1,x=0,y=0,w=16,h=16,c=3,r=2,X=6,Y=6'

# Positive Kitty z overlays text and other lower z images; negative z is
# below text. The shared red/blue edge makes any wrong order obvious.
at 12 2
printf '3. Kitty z: BLUE covers RED; GREEN stays behind TEXT'
at 12 72
printf 'TEXT'
at 12 72
kitty 'a=p,q=2,C=1,i=202,p=2,x=0,y=0,w=16,h=16,c=4,r=2,z=-1'
at 14 72
kitty 'a=p,q=2,C=1,i=201,p=3,x=0,y=0,w=16,h=16,c=4,r=2,z=1'
at 14 74
kitty 'a=p,q=2,C=1,i=203,p=4,x=0,y=0,w=16,h=16,c=4,r=2,z=2'

# SIXEL is temporal: a later image covers text, while later text destroys the
# affected image cells. Both results should match on Kitty and SIXEL output.
at 18 2
printf '4. SIXEL after text: folder covers FOLDER'
at 18 72
printf 'FOLDER'
at 18 72
sixel_folder
at 21 2
printf '5. SIXEL before text: TEXT WINS, no folder remnant'
at 21 72
sixel_folder
at 21 72
printf 'TEXT WINS'

at 23 2
printf 'Take one screenshot, then press Enter to return to the shell.'
printf '\033[?25h'
IFS= read -r _image_edge_reply
