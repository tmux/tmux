s/! Uncomment this for "white" text on a dark background./! Set the default text foreground and background colors./
s/!\*VT100\*foreground: gray90/*VT100*foreground: gray90/
s/!\*VT100\*background: black/*VT100*background: black/
/!\*VT100.scrollbar.thumb:[ 	]*vlines2/,/!lines[ 	]*-1,0,0,0,0,-1/s/!//
