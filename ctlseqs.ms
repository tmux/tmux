.\"#! troff -ms $1		-*- Nroff -*-
.\" "Xterm Control Sequences" document
.\" $XTermId: ctlseqs.ms,v 1.366 2017/12/23 00:02:27 tom Exp $
.\"
.\"
.\" Copyright 1996-2016,2017 by Thomas E. Dickey
.\"
.\"                         All Rights Reserved
.\"
.\" Permission is hereby granted, free of charge, to any person obtaining a
.\" copy of this software and associated documentation files (the
.\" "Software"), to deal in the Software without restriction, including
.\" without limitation the rights to use, copy, modify, merge, publish,
.\" distribute, sublicense, and/or sell copies of the Software, and to
.\" permit persons to whom the Software is furnished to do so, subject to
.\" the following conditions:
.\"
.\" The above copyright notice and this permission notice shall be included
.\" in all copies or substantial portions of the Software.
.\"
.\" THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
.\" OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
.\" MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
.\" IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
.\" CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
.\" TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
.\" SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.\"
.\" Except as contained in this notice, the name(s) of the above copyright
.\" holders shall not be used in advertising or otherwise to promote the
.\" sale, use or other dealings in this Software without prior written
.\" authorization.
.\"
.\"
.\" Copyright 1991, 1994 X Consortium
.\"
.\" Permission is hereby granted, free of charge, to any person obtaining
.\" a copy of this software and associated documentation files (the
.\" "Software"), to deal in the Software without restriction, including
.\" without limitation the rights to use, copy, modify, merge, publish,
.\" distribute, sublicense, and/or sell copies of the Software, and to
.\" permit persons to whom the Software is furnished to do so, subject to
.\" the following conditions:
.\"
.\" The above copyright notice and this permission notice shall be
.\" included in all copies or substantial portions of the Software.
.\"
.\" THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
.\" EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
.\" MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
.\" IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
.\" OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
.\" ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
.\" OTHER DEALINGS IN THE SOFTWARE.
.\"
.\" Except as contained in this notice, the name of the X Consortium shall
.\" not be used in advertising or otherwise to promote the sale, use or
.\" other dealings in this Software without prior written authorization
.\" from the X Consortium.
.\"
.\" X Window System is a trademark of X Consortium, Inc.
.\"
.\" Originally written by Edward Moy, University of California,
.\" Berkeley, edmoy@violet.berkeley.edu, for the X.V10R4 xterm.
.\" The X Consortium staff has since updated it for X11.
.\" Updated by Thomas E. Dickey for XFree86 3.2 - XFree86 4.3, and afterward.
.\"
.\" Run this file through troff and use the -ms macro package.
.\"
.ds XT XTerm
.ds xt xterm
.ds LF Patch #331
.ds RF 2017/12/22
.\"
.if n .pl 9999v		\" no page breaks in nroff
.ND
.\" Start a list of controls
.de St
.sp
.nr PD 0
.nr PI 1.0i
.nr VS 16
..
.\" End a list of controls
.de Ed
.nr PD .3v
.nr VS 12
..
.\" Bulleted paragraph
.de bP
.ie n  .IP \(bu 4
.el    .IP \(bu 2
..
.\" Normal leading paragraph
.de lP
.if n .sp
.LP
..
.\" Normal internal paragraph
.de sP
.if n .sp
.if t .sp 0.5
..
.\" Section header
.de Sh
.ds RH \\$1
.br
.SH
\\$1
..
.\" Subsection header
.de Ss
.LP
.B
\\$*
.br
..
.\" Escape single quotes in literal strings from groff's Unicode transform.
.ie \n(.g .ds AQ \(aq
.el       .ds AQ '
.ie \n(.g .ds `` \(lq
.el       .ds `` ``
.ie \n(.g .ds '' \(rq
.el       .ds '' ''
.ds CH \" as nothing
.ds LH \*(XT Control Sequences
.nr s 6*\n(PS/10
.ds L \s\nsBEL\s0
.ds E \s\nsESC\s0
.ds T \s\nsTAB\s0
.ds X \s\nsETX\s0
.ds N \s\nsENQ\s0
.ds ET \s\nsETB\s0
.ds C \s\nsCAN\s0
.ds S \s\nsSUB\s0
.\" space between chars
.ie t .ds s \|
.el .ds s " \"
.nr [W \w'\*L'u
.nr w \w'\*E'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*T'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*X'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*N'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*(ET'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*C'u
.if \nw>\n([W .nr [W \nw
.nr w \w'\*S'u
.if \nw>\n([W .nr [W \nw
.nr [W +\w'\|\|'u
.de []
.nr w \w'\\$2'
.nr H \\n([Wu-\\nwu
.nr h \\nHu/2u
.\" do fancy box in troff
.ie t .ds \\$1 \(br\v'-1p'\(br\v'1p'\h'\\nhu'\\$2\h'\\nHu-\\nhu'\(br\l'-\\n([Wu\(ul'\v'-1p'\(br\l'-\\n([Wu\(rn'\v'1p'\*s
.el .ds \\$1 \\$2\*s
..
.[] Et \v'-1p'\*X\v'1p'
.[] En \v'-1p'\*N\v'1p'
.[] Be \v'-1p'\*L\v'1p'
.[] AP \v'-1p'\s\nsAPC\s0\v'1p'
.[] Bs \v'-1p'\s\nsBS\s0\v'1p'
.[] Cs \v'-1p'\s\nsCSI\s0\v'1p'
.[] S2 \v'-1p'\s\nsSS2\s0\v'1p'
.[] S3 \v'-1p'\s\nsSS3\s0\v'1p'
.[] SS \v'-1p'\s\nsSOS\s0\v'1p'
.[] Eg \v'-1p'\s\nsEPA\s0\v'1p'
.[] Sg \v'-1p'\s\nsSPA\s0\v'1p'
.[] Dc \v'-1p'\s\nsDCS\s0\v'1p'
.[] Ht \v'-1p'\s\nsHTS\s0\v'1p'
.[] Id \v'-1p'\s\nsIND\s0\v'1p'
.[] Nl \v'-1p'\s\nsNEL\s0\v'1p'
.[] Os \v'-1p'\s\nsOSC\s0\v'1p'
.[] RI \v'-1p'\s\nsRI\s0\v'1p'
.[] PM \v'-1p'\s\nsPM\s0\v'1p'
.[] ST \v'-1p'\s\nsST\s0\v'1p'
.[] Ta \v'-1p'\*T\v'1p'
.[] Lf \v'-1p'\s\nsLF\s0\v'1p'
.[] Vt \v'-1p'\s\nsVT\s0\v'1p'
.[] Ff \v'-1p'\s\nsFF\s0\v'1p'
.[] Cr \v'-1p'\s\nsCR\s0\v'1p'
.[] So \v'-1p'\s\nsSO\s0\v'1p'
.[] Sp \v'-1p'\s\nsSP\s0\v'1p'
.[] Si \v'-1p'\s\nsSI\s0\v'1p'
.[] Eb \v'-1p'\*(ET\v'1p'
.[] Ca \v'-1p'\*C\v'1p'
.[] Su \v'-1p'\*S\v'1p'
.[] Es \v'-1p'\*E\v'1p'
.[] Fs \v'-1p'\s\nsFS\s0\v'1p'
.[] Gs \v'-1p'\s\nsGS\s0\v'1p'
.[] Rs \v'-1p'\s\nsRS\s0\v'1p'
.[] Us \v'-1p'\s\nsUS\s0\v'1p'
.[] XX \v'-1p'\s\nsXX\s0\v'1p'
.[] $ $
.[] # #
.[] % %
.[] (( (
.[] ) )
.[] * *
.[] + +
.[] , ,
.[] - -
.[] . .
.[] 0 0
.[] 1 1
.[] 2 2
.[] 3 3
.[] 4 4
.[] 5 5
.[] 6 6
.[] 7 7
.[] 8 8
.[] 9 9
.[] : :
.[] ; ;
.[] = =
.[] / /
.[] < <
.[] > >
.[] ? ?
.[] @ @
.[] A A
.[] cB B
.[] C C
.[] D D
.[] E E
.[] F F
.[] G G
.[] H H
.[] I I
.[] J J
.[] K K
.[] L L
.[] M M
.[] N N
.[] O O
.[] P P
.[] Q Q
.[] R R
.[] S S
.[] T T
.[] V V
.[] W W
.[] XX X
.[] Y Y
.[] Z Z
.[] [[ [
.[] ]] ]
.[] bS \\e
.[] { {
.[] ~ ~
.[] ] ]
.[] & &
.[] ^ ^
.[] _ _
.[] qu \&'
.[] ` \`
.[] a a
.[] b b
.[] c c
.[] d d
.[] e e
.[] f f
.[] g g
.[] h h
.[] i i
.[] j j
.[] k k
.[] l l
.[] m m
.[] n n
.[] o o
.[] p p
.[] q q
.[] r r
.[] cs s
.[] t t
.[] u u
.[] v v
.[] w w
.[] x x
.[] y y
.[] z z
.[] | |
.[] } }
.[] ! !
.[] c" \(lq
.[] c~ ~
.[] Sc \fIc\fP
.ds Cc \fIC\fP
.ds Cb \fIC\v'.3m'\h'-.2m'\s-2b\s0\v'-.3m'\fP
.ds Cx \fIC\v'.3m'\h'-.2m'\s-2x\s0\v'-.3m'\fP
.ds Cy \fIC\v'.3m'\h'-.2m'\s-2y\s0\v'-.3m'\fP
.ds Pa \fIP\v'.3m'\h'-.2m'\s-2a\s0\v'-.3m'\fP
.ds Pb \fIP\v'.3m'\h'-.2m'\s-2b\s0\v'-.3m'\fP
.ds Pc \fIP\v'.3m'\h'-.2m'\s-2c\s0\v'-.3m'\fP
.ds Pd \fIP\v'.3m'\h'-.2m'\s-2d\s0\v'-.3m'\fP
.ds Pe \fIP\v'.3m'\h'-.2m'\s-2e\s0\v'-.3m'\fP
.ds Pg \fIP\v'.3m'\h'-.2m'\s-2g\s0\v'-.3m'\fP
.ds Ph \fIP\v'.3m'\h'-.2m'\s-2h\s0\v'-.3m'\fP
.ds Pi \fIP\v'.3m'\h'-.2m'\s-2i\s0\v'-.3m'\fP
.ds Pl \fIP\v'.3m'\h'-.2m'\s-2l\s0\v'-.3m'\fP
.ds Pm \fIP\v'.3m'\h'-.2m'\s-2m\s0\v'-.3m'\fP
.ds Pn \fIP\v'.3m'\h'-.2m'\s-2n\s0\v'-.3m'\fP
.ds Pp \fIP\v'.3m'\h'-.2m'\s-2p\s0\v'-.3m'\fP
.ds Pr \fIP\v'.3m'\h'-.2m'\s-2r\s0\v'-.3m'\fP
.ds Ps \fIP\v'.3m'\h'-.2m'\s-2s\s0\v'-.3m'\fP
.ds Pt \fIP\v'.3m'\h'-.2m'\s-2t\s0\v'-.3m'\fP
.ds Pu \fIP\v'.3m'\h'-.2m'\s-2u\s0\v'-.3m'\fP
.ds Pv \fIP\v'.3m'\h'-.2m'\s-2v\s0\v'-.3m'\fP
.ds Ix \fIx\fP
.ds Iy \fIy\fP
.ds Iw \fIw\fP
.ds Ih \fIh\fP
.ds Ir \fIr\fP
.ds Ic \fIc\fP
.ie t .nr LL 6.5i
.el .nr LL 72m
.if n .na
.TL
\*(XT Control Sequences
.AU
Edward Moy
.AI
University of California, Berkeley
.sp
Revised by
.AU
Stephen Gildea
.AI
X Consortium (1994)
.AU
Thomas Dickey
.AI
XFree86 Project (1996-2006)
invisible-island.net (2006-2017)
updated for \*(XT \*(LF (\*(RF)
.AU
.
.am BT                  \" add page numbers after first page
.ds CF %
..
.Sh "Definitions"
.IP \*(Sc
The literal character \fIc\fP.
.IP \*(Cc
A single (required) character.
.IP \*(Ps
A single (usually optional) numeric parameter, composed of one of more digits.
.IP \*(Pm
A multiple numeric parameter composed of any number of single numeric
parameters, separated by \*; character(s).
Individual values for the parameters are listed with \*(Ps .
.IP \*(Pt
A text parameter composed of printable characters.
.
.Sh "Control Bytes, Characters, and Sequences"
.LP
ECMA-48 (aka "ISO 6429") documents C1 (8-bit) and C0 (7-bit) codes.
Those are respectively codes 128 to 159 and 0 to 31.
ECMA-48 avoids referring to these codes as characters,
because that term is associated with \fIgraphic characters\fP.
Instead, it uses \*(``bytes\*('' and \*(``codes\*('',
with occasional lapses to \*(``characters\*(''
where the meaning cannot be mistaken.
.LP
Controls (including the escape code 27) are processed once:
.bP
This means that a C1 control can be mistaken for badly-formed UTF-8
when the terminal runs
in UTF-8 mode because C1 controls are valid \fIcontinuation bytes\fP of
a UTF-8 encoded (multibyte) value.
.bP
It is not possible to use a C1 control obtained from decoding the UTF-8 text,
because that would require reprocessing the data.
Consequently there is no ambiguity in the way
this document uses the term \*(``character\*('' to refer to
bytes in a control sequence.
.LP
The order of processing is a necessary consequence of the way ECMA-48
is designed:
.bP
Each byte sent to the terminal can be unambiguously determined to
fall into one of a few categories (C0, C1 and graphic characters).
.bP
ECMA-48 is \fImodal\fP; once it starts processing a control sequence,
the terminal continues until the sequence is complete,
or some byte is found which is not allowed in the sequence.
.bP
Intermediate, parameter and final bytes may
use the same codes as graphic characters,
but they are processed as part of a control sequence and are not actually
graphic characters.
.bP
Eight-bit controls can have intermediate, etc., bytes in the range 160 to 255.
Those can be treated as their counterparts in the range 32 to 127.
.bP
Single-byte controls can be handled separately from multi-byte
control sequences because ECMA-48's rules are unambiguous.
.IP
As a special case, ECMA-48 (section 9) mentions that the control functions
shift-in and shift-out are allowed to occur within a 7-bit multibyte control
sequence because those cannot alter the meaning of the control sequence.
.bP
Some controls (such as \*(Os) introduce a string mode,
which is ended on a \*(ST (string terminator).
.IP
Again, the terminal should accept single-byte controls within the string.
However, \fIxterm\fP has a resource setting \fBbrokenLinuxOSC\fP to allow
recovery from applications which rely upon malformed palette sequences
used by the Linux console.
.
.Sh "C1 (8-Bit) Control Characters"
.LP
The \fIxterm\fP program recognizes both 8-bit and 7-bit control characters.
It generates 7-bit controls (by default) or 8-bit if S8C1T is enabled.
The following pairs of 7-bit and 8-bit control characters are equivalent:
.St
.IP \\*(Es\\*D
Index (\*(Id is 0x84).
.IP \\*(Es\\*E
Next Line (\*(Nl is 0x85).
.IP \\*(Es\\*H
Tab Set (\*(Ht is 0x88).
.IP \\*(Es\\*M
Reverse Index (\*(RI is 0x8d).
.IP \\*(Es\\*N
Single Shift Select of G2 Character Set (\*(S2 is 0x8e).  This affects next character only.
.IP \\*(Es\\*O
Single Shift Select of G3 Character Set (\*(S3 is 0x8f).  This affects next character only.
.IP \\*(Es\\*P
Device Control String (\*(Dc is 0x90).
.IP \\*(Es\\*V
Start of Guarded Area (\*(Sg is 0x96).
.IP \\*(Es\\*W
End of Guarded Area (\*(Eg is 0x97).
.IP \\*(Es\\*(XX
Start of String (\*(SS is 0x98).
.IP \\*(Es\\*Z
Return Terminal ID (DECID is 0x9a).
Obsolete form of \*(Cs\*c (DA).
.IP \\*(Es\\*([[
Control Sequence Introducer (\*(Cs is 0x9b).
.IP \\*(Es\\*(bS
String Terminator (\*(ST is 0x9c).
.IP \\*(Es\\*(]]
Operating System Command (\*(Os is 0x9d).
.IP \\*(Es\\*^
Privacy Message (\*(PM is 0x9e).
.IP \\*(Es\\*_
Application Program Command (\*(AP is 0x9f).
.Ed
.sp
.LP
These control characters are used in the vtXXX emulation.
.
.Sh "VT100 Mode"
.LP
Most of these control sequences are standard VT102 control sequences,
but there is support for later DEC VT terminals
(i.e., VT220, VT320, VT420, VT510),
as well as ISO 6429 and \fIaixterm\fP color controls.
The only VT102 feature not supported is auto-repeat,
since the only way X provides for this will affect all windows.
.LP
There are additional control sequences to provide
\fIxterm-\fPdependent functions, such as the scrollbar or window size.
Where the function is specified by DEC or ISO 6429, the code assigned
to it is given in parentheses.
.LP
The escape codes to designate and invoke
character sets are specified by ISO 2022 (see that document for a
discussion of character sets).
.LP
Many of the features are optional;
\fI\*(xt\fP can be configured and built without support for them.
.
.St
.\"
.Ss Single-character functions
.\"
.IP \\*(Be
Bell (Ctrl-G).
.
.IP \\*(Bs
Backspace (Ctrl-H).
.
.IP \\*(Cr
Carriage Return (Ctrl-M).
.
.IP \\*(En
Return Terminal Status (Ctrl-E).
Default response is an empty string, but may be overridden
by a resource \fBanswerbackString\fP.
.
.IP \\*(Ff
Form Feed or New Page (NP).
(\*(Ff is Ctrl-L).
\*(Ff is treated the same as \*(Lf.
.
.IP \\*(Lf
Line Feed or New Line (NL).
(\*(Lf is Ctrl-J).
.
.IP \\*(Si
Shift In (Ctrl-O) \(-> Switch to \fIStandard Character Set\fP.  This invokes the
G0 character set (the default).
.
.IP \\*(So
Shift Out (Ctrl-N) \(-> Switch to \fIAlternate Character Set\fP.  This invokes the
G1 character set.
.
.IP \\*(Sp
Space.
.
.IP \\*(Ta
Horizontal Tab (HT) (Ctrl-I).
.
.IP \\*(Vt
Vertical Tab (Ctrl-K).  This is treated the same as LF.
.Ed
.\"
.\"
.\"
.St
.Ss Controls beginning with \*(Es
.LP
This excludes controls where \*(Es is part of a 7-bit equivalent to 8-bit C1 controls, ordered by the final character(s).
.\"
.IP \\*(Es\\*(Sp\\*F
7-bit controls (S7C1T).
.
.IP \\*(Es\\*(Sp\\*G
8-bit controls (S8C1T).
.
.IP \\*(Es\\*(Sp\\*L
Set ANSI conformance level 1 (dpANS X3.134.1).
.
.IP \\*(Es\\*(Sp\\*M
Set ANSI conformance level 2 (dpANS X3.134.1).
.
.IP \\*(Es\\*(Sp\\*N
Set ANSI conformance level 3 (dpANS X3.134.1).
.
.IP \\*(Es\\*#\\*3
DEC double-height line, top half (DECDHL).
.
.IP \\*(Es\\*#\\*4
DEC double-height line, bottom half (DECDHL).
.
.IP \\*(Es\\*#\\*5
DEC single-width line (DECSWL).
.
.IP \\*(Es\\*#\\*6
DEC double-width line (DECDWL).
.
.IP \\*(Es\\*#\\*8
DEC Screen Alignment Test (DECALN).
.
.IP \\*(Es\\*%\\*@
Select default character set.  That is ISO 8859-1 (ISO 2022).
.
.IP \\*(Es\\*%\\*G
Select UTF-8 character set (ISO 2022).
.
.IP \\*(Es\\*(((\\*(Cc
Designate G0 Character Set (ISO 2022, VT100).
.br
Final character \*(Cc for designating 94-character sets.
In this list, \*0, \*A and \*(cB apply to VT100 and up, the remainder to VT220 and up.
The VT220 character sets, together with the Portuguese character set are activated by
the National Replacement Character controls.  The \*A is a special case, since it is
also activated by the VT300-control for British Latin-1 separately from the National
Replacement Character controls.
  \*(Cc = \*0 \(-> DEC Special Character and Line Drawing Set.
  \*(Cc = \*< \(-> DEC Supplementary (VT200).
  \*(Cc = \*%\*5 \(-> DEC Supplementary Graphics (VT300).
  \*(Cc = \*> \(-> DEC Technical (VT300).
  \*(Cc = \*A \(-> United Kingdom (UK).
  \*(Cc = \*(cB \(-> United States (USASCII).
  \*(Cc = \*4 \(-> Dutch.
  \*(Cc = \*C or \*5 \(-> Finnish.
  \*(Cc = \*R or \*f \(-> French.
  \*(Cc = \*Q or \*9 \(-> French Canadian (VT200, VT300).
  \*(Cc = \*K \(-> German.
  \*(Cc = \*Y \(-> Italian.
  \*(Cc = \*`, \*E or \*6 \(-> Norwegian/Danish.
  \*(Cc = \*%\*6 \(-> Portuguese (VT300).
  \*(Cc = \*Z \(-> Spanish.
  \*(Cc = \*H or \*7 \(-> Swedish.
  \*(Cc = \*= \(-> Swiss.
.\" VT3xx and VT5xx (see vttest) add more selections, not implemented here.
.IP \\*(Es\\*)\\*(Cc
Designate G1 Character Set (ISO 2022, VT100).
.br
The same character sets apply as for \*(Es\*(((\*(Cc.
.
.IP \\*(Es\\**\\*(Cc
Designate G2 Character Set (ISO 2022, VT220).
.br
The same character sets apply as for \*(Es\*(((\*(Cc.
.
.IP \\*(Es\\*+\\*(Cc
Designate G3 Character Set (ISO 2022, VT220).
.br
The same character sets apply as for \*(Es\*(((\*(Cc.
.
.IP \\*(Es\\*-\\*(Cc
Designate G1 Character Set (VT300).
.br
The same character sets apply as for \*(Es\*(((\*(Cc.
.
.IP \\*(Es\\*.\\*(Cc
Designate G2 Character Set (VT300).
.br
The same character sets apply as for \*(Es\*(((\*(Cc.
.
.IP \\*(Es\\*/\\*(Cc
Designate G3 Character Set (VT300).
.br
These work for 96-character sets only.
  \*(Cc = \*A \(-> ISO Latin-1 Supplemental.
.\" VT5xx would implement these:
.\"  \*(Cc = \*F \(-> ISO Greek Supplemental
.\"  \*(Cc = \*H \(-> ISO Hebrew Supplemental
.\"  \*(Cc = \*M \(-> ISO Latin-5 Supplemental
.\"  \*(Cc = \*L \(-> ISO Latin-Cyrillic
.
.IP \\*(Es\\*6
Back Index (DECBI), VT420 and up.
.
.IP \\*(Es\\*7
Save Cursor (DECSC).
.
.IP \\*(Es\\*8
Restore Cursor (DECRC).
.
.IP \\*(Es\\*9
Forward Index (DECFI), VT420 and up.
.
.IP \\*(Es\\*=
Application Keypad (DECKPAM).
.
.IP \\*(Es\\*>
Normal Keypad (DECKPNM).
.
.IP \\*(Es\\*F
Cursor to lower left corner of screen.  This is
enabled by the \fBhpLowerleftBugCompat\fP resource.
.
.IP \\*(Es\\*c
Full Reset (RIS).
.
.IP \\*(Es\\*l
Memory Lock (per HP terminals).
Locks memory above the cursor.
.
.IP \\*(Es\\*m
Memory Unlock (per HP terminals).
.
.IP \\*(Es\\*n
Invoke the G2 Character Set as GL (LS2).
.
.IP \\*(Es\\*o
Invoke the G3 Character Set as GL (LS3).
.
.IP \\*(Es\\*|
Invoke the G3 Character Set as GR (LS3R).
.
.IP \\*(Es\\*}
Invoke the G2 Character Set as GR (LS2R).
.
.IP \\*(Es\\*(c~
Invoke the G1 Character Set as GR (LS1R).
.Ed
.
.St
.Ss Application Program-Command functions
.IP \\*(AP\\*(Pt\\*s\\*(ST
None.  \fIxterm\fP implements no \*(AP functions; \*(Pt is ignored.
\*(Pt need not be printable characters.
.Ed
.
.St
.Ss Device-Control functions
.IP \\*(Dc\\*(Ps\\*;\\*(Ps\\*|\\*(Pt\\*s\\*(ST
User-Defined Keys (DECUDK).
The first parameter:
  \*(Ps = \*0 \(-> Clear all UDK definitions before starting (default).
  \*(Ps = \*1 \(-> Erase Below (default).
.br
The second parameter:
  \*(Ps = \*0 \(<- Lock the keys (default).
  \*(Ps = \*1 \(<- Do not lock.
.br
The third parameter is a ';'-separated list of strings denoting
the key-code separated by a '/' from the hex-encoded key value.
The key codes correspond to the DEC function-key codes (e.g., F6=17).
.
.IP \\*(Dc\\*$\\*q\\*(Pt\\*s\\*(ST
Request Status String (DECRQSS).
The string following the "q" is one of the following:
  \*(c"\*q	\(-> DECSCA
  \*(c"\*p	\(-> DECSCL
  \*r		\(-> DECSTBM
  \*(cs		\(-> DECSLRM
  \*m		\(-> SGR
  \*(Sp\*q	\(-> DECSCUSR
.br
\fIxterm\fP responds with
\*(Dc\*1\*$\*r\*(Pt\*s\*(ST
for valid requests, replacing the \*(Pt with the corresponding \*(Cs
string,
or
\*(Dc\*0\*$\*r\*(Pt\*s\*(ST
for invalid requests.
.
.IP \\*(Dc\\*+\\*p\\*(Pt\\*s\\*(ST
Set Termcap/Terminfo Data (xterm, experimental).
The string following the "p" is a name to use for retrieving data from
the terminal database.
The data will be used for the "tcap" keyboard
configuration's function- and special-keys, as well as by the
Request Termcap/Terminfo String control.
.
.IP \\*(Dc\\*+\\*q\\*(Pt\\*s\\*(ST
Request Termcap/Terminfo String (xterm, experimental).
The string following the "q" is a list of names
encoded in hexadecimal (2 digits per character)
separated by \*;
which correspond to termcap or terminfo key names.
.br
Two special features are also recognized, which are not key names:
\fICo\fP for termcap colors (or \fIcolors\fP for terminfo colors),
and
\fITN\fP for termcap name (or \fIname\fP for terminfo name).
.br
\fIxterm\fP responds with
\*(Dc\*1\*+\*r\*(Pt\*s\*(ST
for valid requests, adding to \*(Pt an \*=,
and the value of the corresponding string that \fI\*(xt\fP would send,
or
\*(Dc\*0\*+\*r\*(Pt\*s\*(ST
for invalid requests.
The strings are encoded in hexadecimal (2 digits per character).
.Ed
.\"
.St
.Ss Functions using \*(Cs, ordered by the final character(s)
.IP \\*(Cs\\*(Ps\\*s\\*@
Insert \*(Ps (Blank) Character(s) (default = 1) (ICH).
.
.IP \\*(Cs\\*(Ps\\*s\\*A
Cursor Up \*(Ps Times (default = 1) (CUU).
.
.IP \\*(Cs\\*(Ps\\*s\\*(cB
Cursor Down \*(Ps Times (default = 1) (CUD).
.
.IP \\*(Cs\\*(Ps\\*s\\*C
Cursor Forward \*(Ps Times (default = 1) (CUF).
.
.IP \\*(Cs\\*(Ps\\*s\\*D
Cursor Backward \*(Ps Times (default = 1) (CUB).
.
.IP \\*(Cs\\*(Ps\\*s\\*E
Cursor Next Line \*(Ps Times (default = 1) (CNL).
.
.IP \\*(Cs\\*(Ps\\*s\\*F
Cursor Preceding Line \*(Ps Times (default = 1) (CPL).
.
.IP \\*(Cs\\*(Ps\\*s\\*G
Cursor Character Absolute  [column] (default = [row,1]) (CHA).
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*H
Cursor Position [row;column] (default = [1,1]) (CUP).
.
.IP \\*(Cs\\*(Ps\\*s\\*I
Cursor Forward Tabulation \*(Ps tab stops (default = 1) (CHT).
.
.IP \\*(Cs\\*(Ps\\*s\\*J
Erase in Display (ED).
  \*(Ps = \*0 \(-> Erase Below (default).
  \*(Ps = \*1 \(-> Erase Above.
  \*(Ps = \*2 \(-> Erase All.
  \*(Ps = \*3 \(-> Erase Saved Lines (xterm).
.
.IP \\*(Cs\\*?\\*(Ps\\*s\\*J
Erase in Display (DECSED).
  \*(Ps = \*0 \(-> Selective Erase Below (default).
  \*(Ps = \*1 \(-> Selective Erase Above.
  \*(Ps = \*2 \(-> Selective Erase All.
  \*(Ps = \*3 \(-> Selective Erase Saved Lines (xterm).
.
.IP \\*(Cs\\*(Ps\\*s\\*K
Erase in Line (EL).
  \*(Ps = \*0 \(-> Erase to Right (default).
  \*(Ps = \*1 \(-> Erase to Left.
  \*(Ps = \*2 \(-> Erase All.
.
.IP \\*(Cs\\*?\\*(Ps\\*s\\*K
Erase in Line (DECSEL).
  \*(Ps = \*0 \(-> Selective Erase to Right (default).
  \*(Ps = \*1 \(-> Selective Erase to Left.
  \*(Ps = \*2 \(-> Selective Erase All.
.
.IP \\*(Cs\\*(Ps\\*s\\*L
Insert \*(Ps Line(s) (default = 1) (IL).
.
.IP \\*(Cs\\*(Ps\\*s\\*M
Delete \*(Ps Line(s) (default = 1) (DL).
.
.IP \\*(Cs\\*(Ps\\*s\\*P
Delete \*(Ps Character(s) (default = 1) (DCH).
.
.IP \\*(Cs\\*(Ps\\*s\\*S
Scroll up \*(Ps lines (default = 1) (SU).
.
.IP \\*(Cs\\*?\\*(Pi\\*;\\*(Pa\\*;\\*(Pv\\*s\\*S
If configured to support either \fBSixel Graphics\fP or \fBReGIS Graphics\fP,
\fIxterm\fP accepts a three-parameter control sequence, where \*(Pi,
\*(Pa and \*(Pv are the \fIitem\fP, \fIaction\fP and \fIvalue\fP:
 
  \*(Pi = \*1 \(-> item is number of color registers.
  \*(Pi = \*2 \(-> item is Sixel graphics geometry (in pixels).
  \*(Pi = \*3 \(-> item is ReGIS graphics geometry (in pixels).

  \*(Pa = \*1 \(-> read
  \*(Pa = \*2 \(-> reset to default
  \*(Pa = \*3 \(-> set to value in \*(Pv
  \*(Pa = \*4 \(-> read the maximum allowed value

  \*(Pv can be omitted except when setting (\*(Pa == \*3).
  \*(Pv = \fIn\fP \(<- A single integer is used for color registers.
  \*(Pv = \fIwidth\fP\*;\fIheight\fP \(<- Two integers for graphics geometry.
.sp
\fI\*(xt\fP replies with a control sequence of the same form:
.ID
\*(Cs\*?\*(Pi\*;\*(Ps\*;\*(Pv\*s\*S
.DE
.br
where \*(Ps is the status:
  \*(Ps = \*0 \(-> success.
  \*(Ps = \*1 \(-> error in \*(Pi.
  \*(Ps = \*2 \(-> error in \*(Pa.
  \*(Ps = \*3 \(-> failure.
.br

On success, \*(Pv represents the value read or set.
.sp
\fBNotes\fP:
.RS
.bP
The current implementation allows reading the graphics sizes,
but disallows modifying those sizes because that is done once,
using resource-values.
.bP
Graphics geometry is not necessarily the same as "window
size" (see the dtterm window manipulation extensions). For example,
\fIxterm\fP limits the maximum graphics geometry at compile time
(1000x1000 as of version 328)
although the window size can be larger.
.bP
While resizing a window will always change the current
graphics geometry, the reverse is not true.
Setting graphics geometry does not affect the window size.
.RE
.
.IP \\*(Cs\\*(Ps\\*s\\*T
Scroll down \*(Ps lines (default = 1) (SD).
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*T
Initiate highlight mouse tracking.
Parameters are [func;startx;starty;firstrow;lastrow].
See the section \fBMouse Tracking\fP.
.
.IP \\*(Cs\\*>\\*(Ps\\*;\\*(Ps\\*s\\*T
Reset one or more features of the title modes to the default value.
Normally, "reset" disables the feature.
It is possible to disable the ability to reset features
by compiling a different default for the title modes into \fIxterm\fP.
  \*(Ps = \*0 \(-> Do not set window/icon labels using hexadecimal.
  \*(Ps = \*1 \(-> Do not query window/icon labels using hexadecimal.
  \*(Ps = \*2 \(-> Do not set window/icon labels using UTF-8.
  \*(Ps = \*3 \(-> Do not query window/icon labels using UTF-8.
(See discussion of "Title Modes").
.
.IP \\*(Cs\\*(Ps\\*s\\*(XX
Erase \*(Ps Character(s) (default = 1) (ECH).
.
.IP \\*(Cs\\*(Ps\\*s\\*Z
Cursor Backward Tabulation \*(Ps tab stops (default = 1) (CBT).
.
.IP \\*(Cs\\*(Pm\\*s\\*`
Character Position Absolute  [column] (default = [row,1]) (HPA).
.
.IP \\*(Cs\\*(Pm\\*s\\*a
Character Position Relative  [columns] (default = [row,col+1]) (HPR).
.
.IP \\*(Cs\\*(Ps\\*s\\*b
Repeat the preceding graphic character \*(Ps times (REP).
.
.IP \\*(Cs\\*(Ps\\*s\\*c
Send Device Attributes (Primary DA).
  \*(Ps = \*0 or omitted \(-> request attributes from terminal.
The response depends on the \fBdecTerminalID\fP resource setting.
  \(->\ \*(Cs\*?\*1\*;\*2\*c (\*(``VT100 with Advanced Video Option\*('')
  \(->\ \*(Cs\*?\*1\*;\*0\*c (\*(``VT101 with No Options\*('')
  \(->\ \*(Cs\*?\*6\*c (\*(``VT102\*('')
  \(->\ \*(Cs\*?\*6\*2\*;\*(Ps\*c (\*(``VT220\*('')
  \(->\ \*(Cs\*?\*6\*3\*;\*(Ps\*c (\*(``VT320\*('')
  \(->\ \*(Cs\*?\*6\*4\*;\*(Ps\*c (\*(``VT420\*('')
.br
The VT100-style response parameters do not mean anything by themselves.
VT220 (and higher) parameters do, telling the host what features the terminal supports:
  \*(Ps = \*1 \(-> 132-columns.
  \*(Ps = \*2 \(-> Printer.
  \*(Ps = \*3 \(-> ReGIS graphics.
  \*(Ps = \*4 \(-> Sixel graphics.
  \*(Ps = \*6 \(-> Selective erase.
  \*(Ps = \*8 \(-> User-defined keys.
  \*(Ps = \*9 \(-> National Replacement Character sets.
  \*(Ps = \*1\*5 \(-> Technical characters.
  \*(Ps = \*1\*8 \(-> User windows.
  \*(Ps = \*2\*1 \(-> Horizontal scrolling.
  \*(Ps = \*2\*2 \(-> ANSI color, e.g., VT525.
  \*(Ps = \*2\*9 \(-> ANSI text locator (i.e., DEC Locator mode).
.
.IP \\*(Cs\\*>\\*(Ps\\*s\\*c
Send Device Attributes (Secondary DA).
  \*(Ps = \*0 or omitted \(-> request the terminal's identification code.
The response depends on the \fBdecTerminalID\fP resource setting.
It should apply only to VT220 and up, but \fIxterm\fP extends this to VT100.
  \(-> \*(Cs\*s\*>\*(Pp\*s\*;\*(Pv\*s\*;\*(Pc\*s\*c
.br
where \*(Pp denotes the terminal type
  \*(Pp = \*0 \(-> \*(``VT100\*(''.
  \*(Pp = \*1 \(-> \*(``VT220\*(''.
  \*(Pp = \*2 \(-> \*(``VT240\*(''.
  \*(Pp = \*18 \(-> \*(``VT330\*(''.
  \*(Pp = \*19 \(-> \*(``VT340\*(''.
  \*(Pp = \*24 \(-> \*(``VT320\*(''.
  \*(Pp = \*41 \(-> \*(``VT420\*(''.
  \*(Pp = \*61 \(-> \*(``VT510\*(''.
  \*(Pp = \*64 \(-> \*(``VT520\*(''.
  \*(Pp = \*65 \(-> \*(``VT525\*(''.
.br
and \*(Pv is the firmware version (for \fIxterm\fP, this was originally
the XFree86 patch number, starting with 95).
In a DEC terminal, \*(Pc indicates the ROM cartridge
registration number and is always zero.
.
.IP \\*(Cs\\*(Pm\\*s\\*d
Line Position Absolute  [row] (default = [1,column]) (VPA).
.
.IP \\*(Cs\\*(Pm\\*s\\*e
Line Position Relative  [rows] (default = [row+1,column]) (VPR).
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*f
Horizontal and Vertical Position [row;column] (default = [1,1]) (HVP).
.
.IP \\*(Cs\\*(Ps\\*s\\*g
Tab Clear (TBC).
  \*(Ps = \*0 \(-> Clear Current Column (default).
  \*(Ps = \*3 \(-> Clear All.
.
.IP \\*(Cs\\*(Pm\\*s\\*h
Set Mode (SM).
  \*(Ps = \*2 \(-> Keyboard Action Mode (AM).
  \*(Ps = \*4 \(-> Insert Mode (IRM).
  \*(Ps = \*1\*2 \(-> Send/receive (SRM).
  \*(Ps = \*2\*0 \(-> Automatic Newline (LNM).
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*h
DEC Private Mode Set (DECSET).
  \*(Ps = \*1 \(-> Application Cursor Keys (DECCKM).
  \*(Ps = \*2 \(-> Designate USASCII for character sets G0-G3 (DECANM),
and set VT100 mode.
  \*(Ps = \*3 \(-> 132 Column Mode (DECCOLM).
  \*(Ps = \*4 \(-> Smooth (Slow) Scroll (DECSCLM).
  \*(Ps = \*5 \(-> Reverse Video (DECSCNM).
  \*(Ps = \*6 \(-> Origin Mode (DECOM).
  \*(Ps = \*7 \(-> Wraparound Mode (DECAWM).
  \*(Ps = \*8 \(-> Auto-repeat Keys (DECARM).
  \*(Ps = \*9 \(-> Send Mouse X & Y on button press.
See the section \fBMouse Tracking\fP.  This is the X10 xterm mouse protocol.
  \*(Ps = \*1\*0 \(-> Show toolbar (rxvt).
  \*(Ps = \*1\*2 \(-> Start Blinking Cursor (AT&T 610).
  \*(Ps = \*1\*3 \(-> Start Blinking Cursor (set only via resource or menu).
  \*(Ps = \*1\*4 \(-> Enable XOR of Blinking Cursor control sequence and menu.
  \*(Ps = \*1\*8 \(-> Print form feed (DECPFF).
  \*(Ps = \*1\*9 \(-> Set print extent to full screen (DECPEX).
  \*(Ps = \*2\*5 \(-> Show Cursor (DECTCEM).
  \*(Ps = \*3\*0 \(-> Show scrollbar (rxvt).
  \*(Ps = \*3\*5 \(-> Enable font-shifting functions (rxvt).
  \*(Ps = \*3\*8 \(-> Enter Tektronix Mode (DECTEK).
  \*(Ps = \*4\*0 \(-> Allow 80 \z\(<-\(-> 132 Mode.
  \*(Ps = \*4\*1 \(-> \fBmore\fP(1) fix (see \fBcurses\fP(3) resource).
  \*(Ps = \*4\*2 \(-> Enable National Replacement Character sets (DECNRCM).
  \*(Ps = \*4\*4 \(-> Turn On Margin Bell.
  \*(Ps = \*4\*5 \(-> Reverse-wraparound Mode.
  \*(Ps = \*4\*6 \(-> Start Logging.
This is normally disabled by a compile-time option.
  \*(Ps = \*4\*7 \(-> Use \fIAlternate Screen Buffer\fP.
(This may be disabled by the \fBtiteInhibit\fP resource).
  \*(Ps = \*6\*6 \(-> Application keypad (DECNKM).
  \*(Ps = \*6\*7 \(-> Backarrow key sends backspace (DECBKM).
  \*(Ps = \*6\*9 \(-> Enable left and right margin mode (DECLRMM), VT420 and up.
  \*(Ps = \*9\*5 \(-> Do not clear screen when DECCOLM is set/reset (DECNCSM), VT510 and up.
  \*(Ps = \*1\*0\*0\*0 \(-> Send Mouse X & Y on button press and release.
See the section \fBMouse Tracking\fP.  This is the X11 xterm mouse protocol.
  \*(Ps = \*1\*0\*0\*1 \(-> Use Hilite Mouse Tracking.
  \*(Ps = \*1\*0\*0\*2 \(-> Use Cell Motion Mouse Tracking.
  \*(Ps = \*1\*0\*0\*3 \(-> Use All Motion Mouse Tracking.
  \*(Ps = \*1\*0\*0\*4 \(-> Send FocusIn/FocusOut events.
  \*(Ps = \*1\*0\*0\*5 \(-> Enable UTF-8 Mouse Mode.
  \*(Ps = \*1\*0\*0\*6 \(-> Enable SGR Mouse Mode.
  \*(Ps = \*1\*0\*0\*7 \(-> Enable \fIAlternate Scroll Mode\fP, i.e., the \fBalternateScroll\fP resource.
  \*(Ps = \*1\*0\*1\*0 \(-> Scroll to bottom on tty output (rxvt).
  \*(Ps = \*1\*0\*1\*1 \(-> Scroll to bottom on key press (rxvt).
  \*(Ps = \*1\*0\*1\*5 \(-> Enable urxvt Mouse Mode.
  \*(Ps = \*1\*0\*3\*4 \(-> Interpret "meta" key, sets eighth bit.
(enables the \fBeightBitInput\fP resource).
  \*(Ps = \*1\*0\*3\*5 \(-> Enable special modifiers for Alt and NumLock keys.
(This enables the \fBnumLock\fP resource).
  \*(Ps = \*1\*0\*3\*6 \(-> Send \*(Es  when Meta modifies a key.
(This enables the \fBmetaSendsEscape\fP resource).
  \*(Ps = \*1\*0\*3\*7 \(-> Send DEL from the editing-keypad Delete key.
  \*(Ps = \*1\*0\*3\*9 \(-> Send \*(Es when Alt modifies a key.
(This enables the \fBaltSendsEscape\fP resource).
  \*(Ps = \*1\*0\*4\*0 \(-> Keep selection even if not highlighted.
(This enables the \fBkeepSelection\fP resource).
  \*(Ps = \*1\*0\*4\*1 \(-> Use the CLIPBOARD selection.
(This enables the \fBselectToClipboard\fP resource).
  \*(Ps = \*1\*0\*4\*2 \(-> Enable Urgency window manager hint when Control-G is received.
(This enables the \fBbellIsUrgent\fP resource).
  \*(Ps = \*1\*0\*4\*3 \(-> Enable raising of the window when Control-G is received.
(enables the \fBpopOnBell\fP resource).
  \*(Ps = \*1\*0\*4\*4 \(-> Reuse the most recent data copied to CLIPBOARD.
(This enables the \fBkeepClipboard\fP resource).
  \*(Ps = \*1\*0\*4\*6 \(-> Enable switching to/from \fIAlternate Screen Buffer\fP.
This works for terminfo-based systems, updating the \fBtiteInhibit\fP resource.
  \*(Ps = \*1\*0\*4\*7 \(-> Use \fIAlternate Screen Buffer\fP.  (This may be
disabled by the \fBtiteInhibit\fP resource).
  \*(Ps = \*1\*0\*4\*8 \(-> Save cursor as in DECSC.  (This may be
disabled by the \fBtiteInhibit\fP resource).
  \*(Ps = \*1\*0\*4\*9 \(-> Save cursor as in DECSC
and use \fIAlternate Screen Buffer\fP, clearing it first.  (This may be
disabled by the \fBtiteInhibit\fP resource).
This combines the effects of the \*1\*0\*4\*7 and \*1\*0\*4\*8 modes.
Use this with terminfo-based applications rather than the \*4\*7 mode.
  \*(Ps = \*1\*0\*5\*0 \(-> Set terminfo/termcap function-key mode.
  \*(Ps = \*1\*0\*5\*1 \(-> Set Sun function-key mode.
  \*(Ps = \*1\*0\*5\*2 \(-> Set HP function-key mode.
  \*(Ps = \*1\*0\*5\*3 \(-> Set SCO function-key mode.
  \*(Ps = \*1\*0\*6\*0 \(-> Set legacy keyboard emulation (i.e, X11R6).
  \*(Ps = \*1\*0\*6\*1 \(-> Set VT220 keyboard emulation.
  \*(Ps = \*2\*0\*0\*4 \(-> Set bracketed paste mode.
.
.IP \\*(Cs\\*(Pm\\*s\\*i
Media Copy (MC).
  \*(Ps = \*0 \(-> Print screen (default).
  \*(Ps = \*4 \(-> Turn off printer controller mode.
  \*(Ps = \*5 \(-> Turn on printer controller mode.
  \*(Ps = \*1 \*0 \(-> HTML screen dump.
  \*(Ps = \*1 \*1 \(-> SVG screen dump.
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*i
Media Copy (MC, DEC-specific).
  \*(Ps = \*1 \(-> Print line containing cursor.
  \*(Ps = \*4 \(-> Turn off autoprint mode.
  \*(Ps = \*5 \(-> Turn on autoprint mode.
  \*(Ps = \*1 \*0 \(-> Print composed display, ignores DECPEX.
  \*(Ps = \*1 \*1 \(-> Print all pages.
.
.IP \\*(Cs\\*(Pm\\*s\\*l
Reset Mode (RM).
  \*(Ps = \*2 \(-> Keyboard Action Mode (AM).
  \*(Ps = \*4 \(-> Replace Mode (IRM).
  \*(Ps = \*1\*2 \(-> Send/receive (SRM).
  \*(Ps = \*2\*0 \(-> Normal Linefeed (LNM).
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*l
DEC Private Mode Reset (DECRST).
  \*(Ps = \*1 \(-> Normal Cursor Keys (DECCKM).
  \*(Ps = \*2 \(-> Designate VT52 mode (DECANM).
  \*(Ps = \*3 \(-> 80 Column Mode (DECCOLM).
  \*(Ps = \*4 \(-> Jump (Fast) Scroll (DECSCLM).
  \*(Ps = \*5 \(-> Normal Video (DECSCNM).
  \*(Ps = \*6 \(-> Normal Cursor Mode (DECOM).
  \*(Ps = \*7 \(-> No Wraparound Mode (DECAWM).
  \*(Ps = \*8 \(-> No Auto-repeat Keys (DECARM).
  \*(Ps = \*9 \(-> Don't send Mouse X & Y on button press.
  \*(Ps = \*1\*0 \(-> Hide toolbar (rxvt).
  \*(Ps = \*1\*2 \(-> Stop Blinking Cursor (AT&T 610).
  \*(Ps = \*1\*3 \(-> Disable Blinking Cursor (reset only via resource or menu).
  \*(Ps = \*1\*4 \(-> Disable XOR of Blinking Cursor control sequence and menu.
  \*(Ps = \*1\*8 \(-> Don't print form feed (DECPFF).
  \*(Ps = \*1\*9 \(-> Limit print to scrolling region (DECPEX).
  \*(Ps = \*2\*5 \(-> Hide Cursor (DECTCEM).
  \*(Ps = \*3\*0 \(-> Don't show scrollbar (rxvt).
  \*(Ps = \*3\*5 \(-> Disable font-shifting functions (rxvt).
  \*(Ps = \*4\*0 \(-> Disallow 80 \z\(<-\(-> 132 Mode.
  \*(Ps = \*4\*1 \(-> No \fBmore\fP(1) fix (see \fBcurses\fP(3) resource).
  \*(Ps = \*4\*2 \(-> Disable National Replacement Character sets (DECNRCM).
  \*(Ps = \*4\*4 \(-> Turn Off Margin Bell.
  \*(Ps = \*4\*5 \(-> No Reverse-wraparound Mode.
  \*(Ps = \*4\*6 \(-> Stop Logging.
(This is normally disabled by a compile-time option).
  \*(Ps = \*4\*7 \(-> Use Normal Screen Buffer.
  \*(Ps = \*6\*6 \(-> Numeric keypad (DECNKM).
  \*(Ps = \*6\*7 \(-> Backarrow key sends delete (DECBKM).
  \*(Ps = \*6\*9 \(-> Disable left and right margin mode (DECLRMM), VT420 and up.
  \*(Ps = \*9\*5 \(-> Clear screen when DECCOLM is set/reset (DECNCSM), VT510 and up.
  \*(Ps = \*1\*0\*0\*0 \(-> Don't send Mouse X & Y on button press and
release.
See the section \fBMouse Tracking\fP.
  \*(Ps = \*1\*0\*0\*1 \(-> Don't use Hilite Mouse Tracking.
  \*(Ps = \*1\*0\*0\*2 \(-> Don't use Cell Motion Mouse Tracking.
  \*(Ps = \*1\*0\*0\*3 \(-> Don't use All Motion Mouse Tracking.
  \*(Ps = \*1\*0\*0\*4 \(-> Don't send FocusIn/FocusOut events.
  \*(Ps = \*1\*0\*0\*5 \(-> Disable UTF-8 Mouse Mode.
  \*(Ps = \*1\*0\*0\*6 \(-> Disable SGR Mouse Mode.
  \*(Ps = \*1\*0\*0\*7 \(-> Disable \fIAlternate Scroll Mode\fP, i.e., the \fBalternateScroll\fP resource.
  \*(Ps = \*1\*0\*1\*0 \(-> Don't scroll to bottom on tty output (rxvt).
  \*(Ps = \*1\*0\*1\*1 \(-> Don't scroll to bottom on key press (rxvt).
  \*(Ps = \*1\*0\*1\*5 \(-> Disable urxvt Mouse Mode.
  \*(Ps = \*1\*0\*3\*4 \(-> Don't interpret "meta" key.
(This disables the \fBeightBitInput\fP resource).
  \*(Ps = \*1\*0\*3\*5 \(-> Disable special modifiers for Alt and NumLock keys.
(This disables the \fBnumLock\fP resource).
  \*(Ps = \*1\*0\*3\*6 \(-> Don't send \*(Es when Meta modifies a key.
(This disables the \fBmetaSendsEscape\fP resource).
  \*(Ps = \*1\*0\*3\*7 \(-> Send VT220 Remove from the editing-keypad Delete key.
  \*(Ps = \*1\*0\*3\*9 \(-> Don't send \*(Es when Alt modifies a key.
(This disables the \fBaltSendsEscape\fP resource).
  \*(Ps = \*1\*0\*4\*0 \(-> Do not keep selection when not highlighted.
(This disables the \fBkeepSelection\fP resource).
  \*(Ps = \*1\*0\*4\*1 \(-> Use the PRIMARY selection.
(This disables the \fBselectToClipboard\fP resource).
  \*(Ps = \*1\*0\*4\*2 \(-> Disable Urgency window manager hint when Control-G is received.
(This disables the \fBbellIsUrgent\fP resource).
  \*(Ps = \*1\*0\*4\*3 \(-> Disable raising of the window when Control-G is received.
(This disables the \fBpopOnBell\fP resource).
  \*(Ps = \*1\*0\*4\*6 \(-> Disable switching to/from \fIAlternate Screen Buffer\fP.
This works for terminfo-based systems, updating the \fBtiteInhibit\fP resource.
If currently using the \fIAlternate Screen Buffer\fP,
\fIxterm\fP switches to the Normal Screen Buffer.
  \*(Ps = \*1\*0\*4\*7 \(-> Use Normal Screen Buffer, clearing screen
first if in the \fIAlternate Screen Buffer\fP.  (This may be
disabled by the \fBtiteInhibit\fP resource).
  \*(Ps = \*1\*0\*4\*8 \(-> Restore cursor as in DECRC.  (This may be
disabled by the \fBtiteInhibit\fP resource).
  \*(Ps = \*1\*0\*4\*9 \(-> Use Normal Screen Buffer and restore cursor
as in DECRC.  (This may be disabled by the \fBtiteInhibit\fP resource).
This combines the effects of the \*1\*0\*4\*7 and \*1\*0\*4\*8 modes.
Use this with terminfo-based applications rather than the \*4\*7 mode.
  \*(Ps = \*1\*0\*5\*0 \(-> Reset terminfo/termcap function-key mode.
  \*(Ps = \*1\*0\*5\*1 \(-> Reset Sun function-key mode.
  \*(Ps = \*1\*0\*5\*2 \(-> Reset HP function-key mode.
  \*(Ps = \*1\*0\*5\*3 \(-> Reset SCO function-key mode.
  \*(Ps = \*1\*0\*6\*0 \(-> Reset legacy keyboard emulation (i.e, X11R6).
  \*(Ps = \*1\*0\*6\*1 \(-> Reset keyboard emulation to Sun/PC style.
  \*(Ps = \*2\*0\*0\*4 \(-> Reset bracketed paste mode.
.
.IP \\*(Cs\\*(Pm\\*s\\*m
Character Attributes (SGR).
  \*(Ps = \*0 \(-> Normal (default).
  \*(Ps = \*1 \(-> Bold.
  \*(Ps = \*2 \(-> Faint, decreased intensity (ISO 6429).
  \*(Ps = \*3 \(-> Italicized (ISO 6429).
  \*(Ps = \*4 \(-> Underlined.
  \*(Ps = \*5 \(-> Blink (appears as Bold in X11R6 xterm).
  \*(Ps = \*7 \(-> Inverse.
  \*(Ps = \*8 \(-> Invisible, i.e., hidden (VT300).
  \*(Ps = \*9 \(-> Crossed-out characters (ISO 6429).
  \*(Ps = \*2\*1 \(-> Doubly-underlined (ISO 6429).
  \*(Ps = \*2\*2 \(-> Normal (neither bold nor faint).
  \*(Ps = \*2\*3 \(-> Not italicized (ISO 6429).
  \*(Ps = \*2\*4 \(-> Not underlined.
  \*(Ps = \*2\*5 \(-> Steady (not blinking).
  \*(Ps = \*2\*7 \(-> Positive (not inverse).
  \*(Ps = \*2\*8 \(-> Visible, i.e., not hidden (VT300).
  \*(Ps = \*2\*9 \(-> Not crossed-out (ISO 6429).
  \*(Ps = \*3\*0 \(-> Set foreground color to Black.
  \*(Ps = \*3\*1 \(-> Set foreground color to Red.
  \*(Ps = \*3\*2 \(-> Set foreground color to Green.
  \*(Ps = \*3\*3 \(-> Set foreground color to Yellow.
  \*(Ps = \*3\*4 \(-> Set foreground color to Blue.
  \*(Ps = \*3\*5 \(-> Set foreground color to Magenta.
  \*(Ps = \*3\*6 \(-> Set foreground color to Cyan.
  \*(Ps = \*3\*7 \(-> Set foreground color to White.
  \*(Ps = \*3\*9 \(-> Set foreground color to default (original).
  \*(Ps = \*4\*0 \(-> Set background color to Black.
  \*(Ps = \*4\*1 \(-> Set background color to Red.
  \*(Ps = \*4\*2 \(-> Set background color to Green.
  \*(Ps = \*4\*3 \(-> Set background color to Yellow.
  \*(Ps = \*4\*4 \(-> Set background color to Blue.
  \*(Ps = \*4\*5 \(-> Set background color to Magenta.
  \*(Ps = \*4\*6 \(-> Set background color to Cyan.
  \*(Ps = \*4\*7 \(-> Set background color to White.
  \*(Ps = \*4\*9 \(-> Set background color to default (original).
.sP
If 16-color support is compiled, the following apply.
Assume that \fIxterm\fP's resources
are set so that the ISO color codes are the first 8 of a set of 16.
Then the \fIaixterm\fP colors are the bright versions of the ISO colors:
  \*(Ps = \*9\*0 \(-> Set foreground color to Black.
  \*(Ps = \*9\*1 \(-> Set foreground color to Red.
  \*(Ps = \*9\*2 \(-> Set foreground color to Green.
  \*(Ps = \*9\*3 \(-> Set foreground color to Yellow.
  \*(Ps = \*9\*4 \(-> Set foreground color to Blue.
  \*(Ps = \*9\*5 \(-> Set foreground color to Magenta.
  \*(Ps = \*9\*6 \(-> Set foreground color to Cyan.
  \*(Ps = \*9\*7 \(-> Set foreground color to White.
  \*(Ps = \*1\*0\*0 \(-> Set background color to Black.
  \*(Ps = \*1\*0\*1 \(-> Set background color to Red.
  \*(Ps = \*1\*0\*2 \(-> Set background color to Green.
  \*(Ps = \*1\*0\*3 \(-> Set background color to Yellow.
  \*(Ps = \*1\*0\*4 \(-> Set background color to Blue.
  \*(Ps = \*1\*0\*5 \(-> Set background color to Magenta.
  \*(Ps = \*1\*0\*6 \(-> Set background color to Cyan.
  \*(Ps = \*1\*0\*7 \(-> Set background color to White.
.sP
If \fIxterm\fP is compiled with the 16-color support disabled, it supports
the following, from \fIrxvt\fP:
  \*(Ps = \*1\*0\*0 \(-> Set foreground and background color to default.
.sP
\fIXterm\fP maintains a color palette whose entries are identified by an index beginning with zero.
If 88- or 256-color support is compiled, the following apply:
.RS
.bP
All parameters are decimal integers.
.bP
RGB values range from zero (0) to 255.
.bP
ISO-8613-6 has been interpreted in more than one way;
\fIxterm\fP allows the semicolons separating the subparameters
in this control to be replaced by colons
(but after the first colon, colons must be used).
.RE
.sP
These ISO-8613-6 controls are supported:
  \*(Pm = \*3\*8\*;\*2\*;\*(Pi\*;\*(Pr\*;\*(Pg\*;\*(Pb \(-> Set foreground color to the closest match in \fIxterm\fP's palette for the given RGB \*(Pr/\*(Pg/\*(Pb.
The color space identifier \*(Pi is ignored.
  \*(Pm = \*3\*8\*;\*5\*;\*(Ps \(-> Set foreground color to \*(Ps.
  \*(Pm = \*4\*8\*;\*2\*;\*(Pi\*;\*(Pr\*;\*(Pg\*;\*(Pb \(-> Set background color to the closest match in \fIxterm\fP's palette for the given RGB \*(Pr/\*(Pg/\*(Pb.
The color space identifier \*(Pi is ignored.
  \*(Pm = \*4\*8\*;\*5\*;\*(Ps \(-> Set background color to \*(Ps.
.sp
This variation on ISO-8613-6 is supported for compatibility with KDE konsole:
  \*(Pm = \*3\*8\*;\*2\*;\*(Pr\*;\*(Pg\*;\*(Pb \(-> Set foreground color to the closest match in \fIxterm\fP's palette for the given RGB \*(Pr/\*(Pg/\*(Pb.
  \*(Pm = \*4\*8\*;\*2\*;\*(Pr\*;\*(Pg\*;\*(Pb \(-> Set background color to the closest match in \fIxterm\fP's palette for the given RGB \*(Pr/\*(Pg/\*(Pb.
.sp
If \fIxterm\fP is compiled with direct-color support,
and the resource \fBdirectColor\fP is true, then
rather than choosing the closest match,
\fIxterm\fP asks the X server to directly render a given color.
.sP
.
.IP \\*(Cs\\*>\\*(Ps\\*;\\*(Ps\\*s\\*m
Set or reset resource-values used by \fIxterm\fP to decide whether to
construct escape sequences holding information about the modifiers
pressed with a given key.
The first parameter identifies the resource to set/reset.
The second parameter is the value to assign to the resource.
If the second parameter is omitted, the resource is reset to its initial value.
  \*(Ps = \*0 \(-> modifyKeyboard.
  \*(Ps = \*1 \(-> modifyCursorKeys.
  \*(Ps = \*2 \(-> modifyFunctionKeys.
  \*(Ps = \*4 \(-> modifyOtherKeys.
.br
If no parameters are given, all resources are reset to their initial values.
.
.IP \\*(Cs\\*(Ps\\*s\\*n
Device Status Report (DSR).
  \*(Ps = \*5 \(-> Status Report.
.br
Result (\*(``OK\*('') is
\*(Cs\*0\*n
  \*(Ps = \*6 \(-> Report Cursor Position (CPR) [row;column].
.br
Result is
\*(Cs\*(Ir\*s\*;\*(Ic\*s\*R
.sp
\fBNote\fP:
it is possible for this sequence to be sent by a function key.
For example, with the default keyboard configuration
the shifted F1 key may send (with shift-, control-, alt-modifiers)
.br
  \*(Cs\*1\*s\*;\*2\*s\*R, or
  \*(Cs\*1\*s\*;\*5\*s\*R, or
  \*(Cs\*1\*s\*;\*6\*s\*R, etc.
.br
The second parameter encodes the modifiers; values range from 2 to 16.
See the section \fBPC-Style Function Keys\fP for the codes.
The \fBmodifyFunctionKeys\fP and \fBmodifyKeyboard\fP resources
can change the form of the string sent from the modified F1 key.
.sp
.
.IP \\*(Cs\\*>\\*(Ps\\*s\\*n
Disable modifiers which may be enabled via the
\*(Cs\*>\*(Ps\*;\*(Ps\*s\*m
sequence.
This corresponds to a resource value of "-1", which cannot be set with
the other sequence.
The parameter identifies the resource to be disabled:
  \*(Ps = \*0 \(-> modifyKeyboard.
  \*(Ps = \*1 \(-> modifyCursorKeys.
  \*(Ps = \*2 \(-> modifyFunctionKeys.
  \*(Ps = \*4 \(-> modifyOtherKeys.
.br
If the parameter is omitted, \fBmodifyFunctionKeys\fP is disabled.
When \fBmodifyFunctionKeys\fP is disabled, \fIxterm\fP uses the
modifier keys to make an extended sequence of functions rather
than adding a parameter to each function key to denote the modifiers.
.
.IP \\*(Cs\\*?\\*(Ps\\*s\\*n
Device Status Report (DSR, DEC-specific).
  \*(Ps = \*6 \(-> Report Cursor Position (DECXCPR) [row;column] as
\*(Cs\*?\*(Ir\*s\*;\*(Ic\*s\*R
(assumes the default page, i.e., "1").
  \*(Ps = \*1\*5 \(-> Report Printer status as
\*(Cs\*?\*1\*0\*n (ready).
or
\*(Cs\*?\*1\*1\*n (not ready).
  \*(Ps = \*2\*5 \(-> Report UDK status as
\*(Cs\*?\*2\*0\*n (unlocked)
or
\*(Cs\*?\*2\*1\*n (locked).
  \*(Ps = \*2\*6 \(-> Report Keyboard status as
.br
\*(Cs\*?\*2\*7\*;\*1\*;\*0\*;\*0\*n (North American).
.br
The last two parameters apply to VT400 & up, and denote keyboard ready and LK01 respectively.
  \*(Ps = \*5\*3 \(-> Report Locator status as
\*(Cs\*?\*5\*3\*n Locator available, if compiled-in, or
\*(Cs\*?\*5\*0\*n No Locator, if not.
  \*(Ps = \*5\*5 \(-> Report Locator status as
\*(Cs\*?\*5\*3\*n Locator available, if compiled-in, or
\*(Cs\*?\*5\*0\*n No Locator, if not.
  \*(Ps = \*5\*6 \(-> Report Locator type as
\*(Cs\*?\*5\*7\*;\*1\*n Mouse, if compiled-in, or
\*(Cs\*?\*5\*7\*;\*0\*n Cannot identify, if not.
  \*(Ps = \*6\*2 \(-> Report macro space (DECMSR) as
\*(Cs\*(Pn\*s\**\*s\*{.
  \*(Ps = \*6\*3 \(-> Report memory checksum (DECCKSR) as
\*(Dc\*(Pt\*s\*!x\*sx\*sx\*sx\*s\*(ST.
.br
    \*(Pt is the request id (from an optional parameter to the request).
    The x's are hexadecimal digits 0-9 and A-F.
  \*(Ps = \*7\*5 \(-> Report data integrity as
\*(Cs\*?\*7\*0\*n (ready, no errors).
  \*(Ps = \*8\*5 \(-> Report multi-session configuration as
\*(Cs\*?\*8\*3\*n (not configured for multiple-session operation).
.
.IP \\*(Cs\\*>\\*(Ps\\*s\\*p
Set resource value \fIpointerMode\fP.
This is used by \fIxterm\fP to decide whether to
hide the pointer cursor as the user types.
Valid values for the parameter:
  \*(Ps = \*0 \(-> never hide the pointer.
  \*(Ps = \*1 \(-> hide if the mouse tracking mode is not enabled.
  \*(Ps = \*2 \(-> always hide the pointer, except when leaving the window.
  \*(Ps = \*3 \(-> always hide the pointer, even if leaving/entering the window.
If no parameter is given, \fIxterm\fP uses the default,
which is \*1.
.
.IP \\*(Cs\\*!\\*p
Soft terminal reset (DECSTR).
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*(c"\\*p
Set conformance level (DECSCL).
Valid values for the first parameter:
  \*(Ps = \*6\*1 \(-> VT100.
  \*(Ps = \*6\*2 \(-> VT200.
  \*(Ps = \*6\*3 \(-> VT300.
.br
Valid values for the second parameter:
  \*(Ps = \*0 \(-> 8-bit controls.
  \*(Ps = \*1 \(-> 7-bit controls (always set for VT100).
  \*(Ps = \*2 \(-> 8-bit controls.
.
.IP \\*(Cs\\*(Ps\\*s\\*$\\*p
.br
Request ANSI mode (DECRQM).
For VT300 and up, reply is
.br
  \*(Cs\*(Ps\*;\*(Pm\*$\*y
.br
where \*(Ps is the mode number as in SM/RM,
and \*(Pm is the mode value:
.br
  0 - not recognized
  1 - set
  2 - reset
  3 - permanently set
  4 - permanently reset
.
.IP \\*(Cs\\*?\\*(Ps\\*$\\*p
Request DEC private mode (DECRQM).
For VT300 and up, reply is
.br
  \*(Cs\*?\*(Ps\*;\*(Pm\*$\*y
.br
where \*(Ps is the mode number as in DECSET/DECSET,
\*(Pm is the mode value as in the ANSI DECRQM.
.br
Two private modes are read-only (i.e., \*1\*3 and \*1\*4),
provided only for reporting their values using this control sequence.
They correspond to the resources \fBcursorBlink\fP and \fBcursorBlinkXOR\fP.
.
.IP \\*(Cs\\*(Ps\\*s\\*q
Load LEDs (DECLL).
  \*(Ps = \*0 \(-> Clear all LEDS (default).
  \*(Ps = \*1 \(-> Light Num Lock.
  \*(Ps = \*2 \(-> Light Caps Lock.
  \*(Ps = \*3 \(-> Light Scroll Lock.
  \*(Ps = \*2 \*1 \(-> Extinguish Num Lock.
  \*(Ps = \*2 \*2 \(-> Extinguish Caps Lock.
  \*(Ps = \*2 \*3 \(-> Extinguish Scroll Lock.
.
.IP \\*(Cs\\*(Ps\\*s\\*(Sp\\*q
Set cursor style (DECSCUSR, VT520).
  \*(Ps = \*0 \(-> blinking block.
  \*(Ps = \*1 \(-> blinking block (default).
  \*(Ps = \*2 \(-> steady block.
  \*(Ps = \*3 \(-> blinking underline.
  \*(Ps = \*4 \(-> steady underline.
  \*(Ps = \*5 \(-> blinking bar (xterm).
  \*(Ps = \*6 \(-> steady bar (xterm).
.
.IP \\*(Cs\\*(Ps\\*s\\*(c"\\*q
Select character protection attribute (DECSCA).
Valid values for the parameter:
  \*(Ps = \*0 \(-> DECSED and DECSEL can erase (default).
  \*(Ps = \*1 \(-> DECSED and DECSEL cannot erase.
  \*(Ps = \*2 \(-> DECSED and DECSEL can erase.
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*r
Set Scrolling Region [top;bottom] (default = full size of window) (DECSTBM).
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*r
Restore DEC Private Mode Values.
The value of \*(Ps previously saved is restored.
\*(Ps values are the same as for DECSET.
.
.IP \\*(Cs\\*(Pt\\*;\\*(Pl\\*;\\*(Pb\\*;\\*(Pr\\*;\\*(Ps\\*$\\*r
Change Attributes in Rectangular Area (DECCARA), VT400 and up.
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
  \*(Ps denotes the SGR attributes to change: 0, 1, 4, 5, 7.
.
.IP \\*(Cs\\*(cs
Save cursor, available only when DECLRMM is disabled (SCOSC, also ANSI.SYS).
.
.IP \\*(Cs\\*(Pl\\*;\\*(Pr\\*s\\*(cs
Set left and right margins (DECSLRM), available only when DECLRMM is enabled (VT420 and up).
.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*(cs
Save DEC Private Mode Values.
\*(Ps values are the same as for DECSET.
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Ps\\*s\\*;\\*(Ps\\*s\\*t
Window manipulation (from \fIdtterm\fP, as well as extensions).
These controls may be disabled using the \fIallowWindowOps\fP resource.
Valid values for the first (and any additional parameters) are:
  \*(Ps = \*1 \(-> De-iconify window.
  \*(Ps = \*2 \(-> Iconify window.
  \*(Ps = \*3\*s\*;\*s\fIx\*s\*;\*sy\fP \(-> Move window to [x, y].
  \*(Ps = \*4\*s\*;\*s\fIheight\*s\*;\*swidth\fP \(-> Resize the \fIxterm\fP window to given height and width in pixels.
Omitted parameters reuse the current height or width.
Zero parameters use the display's height or width.
  \*(Ps = \*5 \(-> Raise the \fIxterm\fP window to the front of the stacking order.
  \*(Ps = \*6 \(-> Lower the \fIxterm\fP window to the bottom of the stacking order.
  \*(Ps = \*7 \(-> Refresh the \fIxterm\fP window.
  \*(Ps = \*8\*s\*;\*s\fIheight\*s\*;\*swidth\fP \(-> Resize the text area to given height and width in characters.
Omitted parameters reuse the current height or width.
Zero parameters use the display's height or width.
  \*(Ps = \*9\*s\*;\*s\*0 \(-> Restore maximized window.
  \*(Ps = \*9\*s\*;\*s\*1 \(-> Maximize window (i.e., resize to screen size).
  \*(Ps = \*9\*s\*;\*s\*2 \(-> Maximize window vertically.
  \*(Ps = \*9\*s\*;\*s\*3 \(-> Maximize window horizontally.
  \*(Ps = \*1\*0\*s\*;\*s\*0 \(-> Undo full-screen mode.
  \*(Ps = \*1\*0\*s\*;\*s\*1 \(-> Change to full-screen.
  \*(Ps = \*1\*0\*s\*;\*s\*2 \(-> Toggle full-screen.
  \*(Ps = \*1\*1 \(-> Report \fIxterm\fP window state.
If the \fIxterm\fP window is open (non-iconified), it returns \*(Cs\*1\*t.
If the \fIxterm\fP window is iconified, it returns \*(Cs\*2\*t.
  \*(Ps = \*1\*3 \(-> Report \fIxterm\fP window position.
.br
Result is
\*(Cs\*3\*;\*(Ix\*s\*;\*(Iy\*s\*t
  \*(Ps = \*1\*4 \(-> Report \fIxterm\fP window in pixels.
.br
Result is
\*(Cs\*s\*4\*s\*;\*s\fIheight\fP\*s\*;\*s\fIwidth\fP\*s\*t
  \*(Ps = \*1\*8 \(-> Report the size of the text area in characters.
.br
Result is
\*(Cs\*s\*8\*s\*;\*s\fIheight\fP\*s\*;\*s\fIwidth\fP\*s\*t
  \*(Ps = \*1\*9 \(-> Report the size of the screen in characters.
.br
Result is
\*(Cs\*s\*9\*s\*;\*s\fIheight\fP\*s\*;\*s\fIwidth\fP\*s\*t
  \*(Ps = \*2\*0 \(-> Report \fIxterm\fP window's icon label.
.br
Result is
\*(Os\*s\*L\*s\fIlabel\fP\*s\*(ST
  \*(Ps = \*2\*1 \(-> Report \fIxterm\fP window's title.
.br
Result is
\*(Os\*s\*l\*s\fIlabel\fP\*s\*(ST
  \*(Ps = \*2\*2\*s\*;\*s\*0 \(-> Save \fIxterm\fP icon and window title on stack.
  \*(Ps = \*2\*2\*s\*;\*s\*1 \(-> Save \fIxterm\fP icon title on stack.
  \*(Ps = \*2\*2\*s\*;\*s\*2 \(-> Save \fIxterm\fP window title on stack.
  \*(Ps = \*2\*3\*s\*;\*s\*0 \(-> Restore \fIxterm\fP icon and window title from stack.
  \*(Ps = \*2\*3\*s\*;\*s\*1 \(-> Restore \fIxterm\fP icon title from stack.
  \*(Ps = \*2\*3\*s\*;\*s\*2 \(-> Restore \fIxterm\fP window title from stack.
  \*(Ps >= \*2\*4 \(-> Resize to \*(Ps lines (DECSLPP).
.
.IP \\*(Cs\\*>\\*(Ps\\*;\\*(Ps\\*s\\*t
Set one or more features of the title modes.
Each parameter enables a single feature.
  \*(Ps = \*0 \(-> Set window/icon labels using hexadecimal.
  \*(Ps = \*1 \(-> Query window/icon labels using hexadecimal.
  \*(Ps = \*2 \(-> Set window/icon labels using UTF-8.
  \*(Ps = \*3 \(-> Query window/icon labels using UTF-8.
(See discussion of "Title Modes")
.
.IP \\*(Cs\\*(Ps\\*s\\*(Sp\\*t
Set warning-bell volume (DECSWBV, VT520).
  \*(Ps = \*0 or \*1 \(-> off.
  \*(Ps = \*2, \*3 or \*4 \(-> low.
  \*(Ps = \*5, \*6, \*7, or \*8 \(-> high.
.
.IP \\*(Cs\\*(Pt\\*;\\*(Pl\\*;\\*(Pb\\*;\\*(Pr\\*;\\*(Ps\\*$\\*t
Reverse Attributes in Rectangular Area (DECRARA), VT400 and up.
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
  \*(Ps denotes the attributes to reverse, i.e.,  1, 4, 5, 7.
.
.IP \\*(Cs\\*u
Restore cursor (SCORC, also ANSI.SYS).
.
.IP \\*(Cs\\*(Ps\\*s\\*(Sp\\*u
Set margin-bell volume (DECSMBV, VT520).
  \*(Ps = \*1 \(-> off.
  \*(Ps = \*2, \*3 or \*4 \(-> low.
  \*(Ps = \*0, \*5, \*6, \*7, or \*8 \(-> high.
.
.IP \\*(Cs\\*(Pt\\*;\\*(Pl\\*;\\*(Pb\\*;\\*(Pr\\*;\\*(Pp\\*;\\*(Pt\\*;\\*(Pl\\*;\\*(Pp\\*$\\*v
Copy Rectangular Area (DECCRA, VT400 and up).
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
  \*(Pp denotes the source page.
  \*(Pt\*;\*(Pl denotes the target location.
  \*(Pp denotes the target page.
.
.IP \\*(Cs\\*(Pt\\*s\\*;\\*(Pl\\*s\\*;\\*(Pb\\*s\\*;\\*(Pr\\*s\\*(qu\\*w
Enable Filter Rectangle (DECEFR), VT420 and up.
.br
Parameters are [top;left;bottom;right].
.br
Defines the coordinates of a filter rectangle and activates it.
Anytime the locator is detected outside of the filter rectangle,
an outside rectangle event is generated and the rectangle is disabled.
Filter rectangles are always treated as "one-shot" events.
Any parameters that are omitted default to the current locator position.
If all parameters are omitted, any locator motion will be reported.
DECELR always cancels any prevous rectangle definition.
.
.IP \\*(Cs\\*(Ps\\*s\\*x
Request Terminal Parameters (DECREQTPARM).
.br
if \*(Ps is a "0" (default) or "1", and \fIxterm\fR is emulating VT100,
the control sequence elicits a response of the same form
whose parameters describe the terminal:
  \*(Ps \(-> the given \*(Ps incremented by 2.
  \*(Pn = \*1 \(<- no parity.
  \*(Pn = \*1 \(<- eight bits.
  \*(Pn = \*1 \(<- \*2 \*8 transmit 38.4k baud.
  \*(Pn = \*1 \(<- \*2 \*8 receive 38.4k baud.
  \*(Pn = \*1 \(<- clock multiplier.
  \*(Pn = \*0 \(<- STP flags.
.
.IP \\*(Cs\\*(Ps\\*s\\**\\*x
Select Attribute Change Extent (DECSACE).
  \*(Ps = \*0 \(-> from start to end position, wrapped.
  \*(Ps = \*1 \(-> from start to end position, wrapped.
  \*(Ps = \*2 \(-> rectangle (exact).
.
.IP \\*(Cs\\*(Pc\\*s\\*;\\*(Pt\\*s\\*;\\*(Pl\\*s\\*;\\*(Pb\\*s\\*;\\*(Pr\\*s\\*$\\*x
Fill Rectangular Area (DECFRA), VT420 and up.
  \*(Pc is the character to use.
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
.
.IP \\*(Cs\\*(Pi\\*s\\*;\\*(Pg\\*s\\*;\\*(Pt\\*;\\*(Pl\\*;\\*(Pb\\*;\\*(Pr\\*s\\**\\*y
Request Checksum of Rectangular Area (DECRQCRA), VT420 and up.
Response is
.br
\*(Dc\*(Pi\*s\*!x\*sx\*sx\*sx\*s\*(ST
.br
  \*(Pi is the request id.
  \*(Pg is the page number.
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
  The x's are hexadecimal digits 0-9 and A-F.
.
.IP \\*(Cs\\*(Ps\\*s\\*;\\*(Pu\\*s\\*(qu\\*z
Enable Locator Reporting (DECELR).
.br
Valid values for the first parameter:
  \*(Ps = \*0 \(-> Locator disabled (default).
  \*(Ps = \*1 \(-> Locator enabled.
  \*(Ps = \*2 \(-> Locator enabled for one report, then disabled.
.br
The second parameter specifies the coordinate unit for locator reports.
.br
Valid values for the second parameter:
  \*(Pu = \*0 \(<- or omitted \(-> default to character cells.
  \*(Pu = \*1 \(<- device physical pixels.
  \*(Pu = \*2 \(<- character cells.
.
.IP \\*(Cs\\*(Pt\\*;\\*(Pl\\*;\\*(Pb\\*;\\*(Pr\\*$\\*z
Erase Rectangular Area (DECERA), VT400 and up.
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
.
.IP \\*(Cs\\*(Pm\\*s\\*(qu\\*{
Select Locator Events (DECSLE).
.br
Valid values for the first (and any additional parameters) are:
  \*(Ps = \*0 \(-> only respond to explicit host requests (DECRQLP).
             (This is default).  It also cancels any filter
             rectangle.
  \*(Ps = \*1 \(-> report button down transitions.
  \*(Ps = \*2 \(-> do not report button down transitions.
  \*(Ps = \*3 \(-> report button up transitions.
  \*(Ps = \*4 \(-> do not report button up transitions.
.
.IP \\*(Cs\\*(Pt\\*;\\*(Pl\\*;\\*(Pb\\*;\\*(Pr\\*s\\*$\\*{
Selective Erase Rectangular Area (DECSERA), VT400 and up.
  \*(Pt\*;\*(Pl\*;\*(Pb\*;\*(Pr denotes the rectangle.
.
.IP \\*(Cs\\*(Ps\\*s\\*(qu\\*|
Request Locator Position (DECRQLP).
.br
Valid values for the parameter are:
  \*(Ps = \*0, 1 or omitted \(-> transmit a single DECLRP locator report.
.sP
If Locator Reporting has been enabled by a DECELR, \fI\*(xt\fP will respond
with a DECLRP Locator Report.
This report is also generated on button
up and down events if they have been enabled with a DECSLE, or when
the locator is detected outside of a filter rectangle, if filter rectangles
have been enabled with a DECEFR.
.sP
  \(-> \*(Cs\*(Pe\*s\*;\*(Pb\*s\*;\*(Pr\*s\*;\*(Pc\*s\*;\*(Pp\*s\*&\*s\*w
.sP
Parameters are [\fIevent\fP;\fIbutton\fP;\fIrow\fP;\fIcolumn\fP;\fIpage\fP].
.br
Valid values for the event:
  \*(Pe = \*0 \(-> locator unavailable - no other parameters sent.
  \*(Pe = \*1 \(-> request - \fI\*(xt\fP received a DECRQLP.
  \*(Pe = \*2 \(-> left button down.
  \*(Pe = \*3 \(-> left button up.
  \*(Pe = \*4 \(-> middle button down.
  \*(Pe = \*5 \(-> middle button up.
  \*(Pe = \*6 \(-> right button down.
  \*(Pe = \*7 \(-> right button up.
  \*(Pe = \*8 \(-> M4 button down.
  \*(Pe = \*9 \(-> M4 button up.
  \*(Pe = \*1\*0 \(-> locator outside filter rectangle.
.br
The \*(``\fIbutton\fP\*('' parameter is a bitmask indicating which buttons are pressed:
  \*(Pb = \*0 \(<- no buttons down.
  \*(Pb & \*1 \(<- right button down.
  \*(Pb & \*2 \(<- middle button down.
  \*(Pb & \*4 \(<- left button down.
  \*(Pb & \*8 \(<- M4 button down.
.br
The \*(``\fIrow\fP\*('' and \*(``\fIcolumn\fP\*('' parameters are the coordinates of the locator
position in the \fI\*(xt\fP window, encoded as ASCII decimal.
.br
The \*(``\fIpage\fP\*('' parameter is not used by \*(xt.
.
.IP \\*(Cs\\*(Pm\\*s\\*(qu\\*}
Insert \*(Ps Column(s) (default = 1) (DECIC), VT420 and up.
.
.IP \\*(Cs\\*(Pm\\*s\\*(qu\\*~
Delete \*(Ps Column(s) (default = 1) (DECDC), VT420 and up.
.
.Ed
.
.St
.Ss Operating System Commands
.
.IP \\*(Os\\*(Ps\\*s\\*;\\*(Pt\\*s\\*(Be
.IP \\*(Os\\*(Ps\\*s\\*;\\*(Pt\\*s\\*(ST
Set Text Parameters.
For colors and font,
if \*(Pt is a "?", the control sequence elicits a response which consists
of the control sequence which would set the corresponding value.
The \fIdtterm\fP control sequences allow you to determine the icon name
and window title.
  \*(Ps = \*0 \(-> Change Icon Name and Window Title to \*(Pt.
  \*(Ps = \*1 \(-> Change Icon Name to \*(Pt.
  \*(Ps = \*2 \(-> Change Window Title to \*(Pt.
  \*(Ps = \*3 \(-> Set X property on top-level window.
\*(Pt should be in the form
"\fIprop=value\fP", or just "\fIprop\fP" to delete the property
  \*(Ps = \*4\*;\fIc\fP\*;\fIspec\fP \(-> Change Color Number \fIc\fP to
the color specified by \fIspec\fP.
This can be a name or RGB specification as per \fIXParseColor\fP.
Any number of \fIc\fP/\fIspec\fP pairs may be given.
The color numbers correspond to the ANSI colors 0-7,
their bright versions 8-15,
and if supported, the remainder of the 88-color or 256-color table.
.sP
If a "?" is given rather than a name or RGB specification,
\fI\*(xt\fP replies with a control sequence of the same form which can be used
to set the corresponding color.
Because more than one pair of color number and specification can
be given in one control sequence, \fIxterm\fR can make more than one reply.
.sP
  \*(Ps = \*5\*;\fIc\fP\*;\fIspec\fP \(-> Change Special Color Number \fIc\fP to
the color specified by \fIspec\fP.
This can be a name or RGB specification as per \fIXParseColor\fP.
Any number of \fIc\fP/\fIspec\fP pairs may be given.
The special colors can also be set by adding the maximum number of colors
to these codes in an \*(Os\*4 control:
.sP
.in +2n
  \*(Pc = \*0 \(<- resource \fBcolorBD\fP (BOLD).
  \*(Pc = \*1 \(<- resource \fBcolorUL\fP (UNDERLINE).
  \*(Pc = \*2 \(<- resource \fBcolorBL\fP (BLINK).
  \*(Pc = \*3 \(<- resource \fBcolorRV\fP (REVERSE).
  \*(Pc = \*4 \(<- resource \fBcolorIT\fP (ITALIC).
.in -2n
.sP
  \*(Ps = \*6\*;\fIc\fP\*;\fIf\fP \(-> Enable/disable Special Color Number \fIc\fP.
\*(Os\*6 is the same as \*(Os\*1\*0\*6.
.sP
The 10 colors (below) which may be set or queried
using \*1\*0 through \*1\*9 are
denoted \fIdynamic colors\fR, since the corresponding control sequences
were the first means for setting \fIxterm\fR's colors dynamically,
i.e., after it was started.
They are not the same as the ANSI colors.
These controls may be disabled using the \fIallowColorOps\fP resource.
At least one parameter is expected for \*(Pt.
Each successive parameter changes the next color in the list.
The value of \*(Ps tells the starting point in the list.
The colors are specified by name or RGB specification as per \fIXParseColor\fP.
.sP
If a "?" is given rather than a name or RGB specification,
\fI\*(xt\fP replies with a control sequence of the same form which can be used
to set the corresponding dynamic color.
Because more than one pair of color number and specification can
be given in one control sequence, \fIxterm\fR can make more than one reply.
.sP
  \*(Ps = \*1\*0 \(-> Change VT100 text foreground color to \*(Pt.
  \*(Ps = \*1\*1 \(-> Change VT100 text background color to \*(Pt.
  \*(Ps = \*1\*2 \(-> Change text cursor color to \*(Pt.
  \*(Ps = \*1\*3 \(-> Change mouse foreground color to \*(Pt.
  \*(Ps = \*1\*4 \(-> Change mouse background color to \*(Pt.
  \*(Ps = \*1\*5 \(-> Change Tektronix foreground color to \*(Pt.
  \*(Ps = \*1\*6 \(-> Change Tektronix background color to \*(Pt.
  \*(Ps = \*1\*7 \(-> Change highlight background color to \*(Pt.
  \*(Ps = \*1\*8 \(-> Change Tektronix cursor color to \*(Pt.
  \*(Ps = \*1\*9 \(-> Change highlight foreground color to \*(Pt.
.sP
  \*(Ps = \*4\*6 \(-> Change Log File to \*(Pt.  (This is normally disabled by a
compile-time option).
.sP
  \*(Ps = \*5\*0 \(-> Set Font to \*(Pt.
These controls may be disabled using the \fIallowFontOps\fP resource.
If \*(Pt begins with a "#", index in the font menu, relative (if the
next character is a plus or minus sign) or absolute.
A number is
expected but not required after the sign (the default is the current
entry for relative, zero for absolute indexing).
.br
The same rule (plus or minus sign, optional number) is used when
querying the font.
The remainder of \*(Pt is ignored.
.br
A font can be specified after a "#" index expression,
by adding a space and then the font specifier.
.br
If the "TrueType Fonts" menu entry is set (the \fBrenderFont\fP resource),
then this control sets/queries the \fBfaceName\fP resource.
.sP
  \*(Ps = \*5\*1 \(-> reserved for Emacs shell.
.sP
  \*(Ps = \*5\*2 \(-> Manipulate Selection Data.
These controls may be disabled using the \fIallowWindowOps\fP resource.
The parameter \*(Pt is parsed as
.br
	\*(Pc\*;\*(Pd
.br
The first, \*(Pc, may contain zero or more characters from the
set \*c \*p \*(cs \*0 \*1 \*2 \*3 \*4 \*5 \*6 \*7.
It is used to construct a list of selection parameters for
clipboard,
primary,
select,
or cut buffers 0 through 7 respectively,
in the order given.
If the parameter is empty, \fIxterm\fP uses \*(cs\*0,
to specify the configurable primary/clipboard selection and cut buffer 0.
.br
The second parameter, \*(Pd, gives the selection data.
Normally this is a string encoded in base64.
The data becomes the new selection,
which is then available for pasting by other applications.
.br
If the second parameter is a \*?,
\fIxterm\fP replies to the host with the selection
data encoded using the same protocol.
.br
If the second parameter is neither a base64 string nor \*?,
then the selection is cleared.
.Ed
.sP
  \*(Ps = \*1\*0\*4\*;\fIc\fP \(-> Reset Color Number \fIc\fP.  It is reset to
the color specified by the corresponding X resource.
Any number of \fIc\fP parameters may be given.
These parameters correspond to the ANSI colors 0-7,
their bright versions 8-15,
and if supported, the remainder of the 88-color or 256-color table.
If no parameters are given, the entire table will be reset.
.sP
  \*(Ps = \*1\*0\*5\*;\fIc\fP \(-> Reset Special Color Number \fIc\fP.  It is reset to
the color specified by the corresponding X resource.
Any number of \fIc\fP parameters may be given.
These parameters correspond to the special colors which can be set
using an \*(Os\*5 control (or by adding the maximum number of colors
using an \*(Os\*4 control).
.sP
  \*(Ps = \*1\*0\*6\*;\fIc\fP\*;\fIf\fP \(-> Enable/disable Special Color Number \fIc\fP.
The second parameter tells \fIxterm\fP to enable the corresponding color
mode if nonzero, disable it if zero.
.sP
.in +2n
  \*(Pc = \*0 \(<- resource \fBcolorBDMode\fP (BOLD).
  \*(Pc = \*1 \(<- resource \fBcolorULMode\fP (UNDERLINE).
  \*(Pc = \*2 \(<- resource \fBcolorBLMode\fP (BLINK).
  \*(Pc = \*3 \(<- resource \fBcolorRVMode\fP (REVERSE).
  \*(Pc = \*4 \(<- resource \fBcolorITMode\fP (ITALIC).
  \*(Pc = \*5 \(<- resource \fBcolorAttrMode\fP (Override ANSI).
.in -2n
.sP
The \fIdynamic colors\fR can also be reset to their default (resource) values:
  \*(Ps = \*1\*1\*0 \(-> Reset VT100 text foreground color.
  \*(Ps = \*1\*1\*1 \(-> Reset VT100 text background color.
  \*(Ps = \*1\*1\*2 \(-> Reset text cursor color.
  \*(Ps = \*1\*1\*3 \(-> Reset mouse foreground color.
  \*(Ps = \*1\*1\*4 \(-> Reset mouse background color.
  \*(Ps = \*1\*1\*5 \(-> Reset Tektronix foreground color.
  \*(Ps = \*1\*1\*6 \(-> Reset Tektronix background color.
  \*(Ps = \*1\*1\*7 \(-> Reset highlight color.
  \*(Ps = \*1\*1\*8 \(-> Reset Tektronix cursor color.
  \*(Ps = \*1\*1\*9 \(-> Reset highlight foreground color.
.
.St
.Ss Privacy Message
.IP \\*(PM\\*(Pt\\*s\\*(ST
\fIxterm\fP implements no \*(PM functions; \*(Pt is ignored.
\*(Pt need not be printable characters.
.Ed
.
.Sh "Alt and Meta Keys"
.LP
Many keyboards have keys labeled "Alt".
Few have keys labeled "Meta".
However, \fIxterm\fP's default translations use the \fIMeta\fP modifier.
Common keyboard configurations assign the \fIMeta\fP modifier to an "Alt" key.
By using \fIxmodmap\fP one may have the modifier assigned to a different key,
and have "real" alt and meta keys.
Here is an example:
.ID
.ft CW
! put meta on mod3 to distinguish it from alt
keycode 64 = Alt_L
clear mod1
add mod1 = Alt_L
keycode 115 = Meta_L
clear mod3
add mod3 = Meta_L
.ft 1
.DE
.lP
The \fBmetaSendsEscape\fP resource
(and \fBaltSendsEscape\fP if \fBaltIsNotMeta\fP is set)
can be used to control the way the \fIMeta\fP modifier applies to ordinary
keys unless the \fBmodifyOtherKeys\fP resource is set:
.IP
-
prefix a key with the \*(Es character.
.IP
-
shift the key from codes 0-127 to 128-255 by adding 128.
.lP
The table shows the result for a given character "x" with modifiers
according to the default translations with the resources set on or off.
This assumes \fBaltIsNotMeta\fP is set:
.\" page-eject to work around grohtml bugs
.if t .bp
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c)	lf3w(2c) .
_
.TH
.T&
l | l | l | l .
key	altSendsEscape	metaSendsEscape	result
_
x	off	off	x
Meta-x	off	off	shift
Alt-x	off	off	shift
Alt+Meta-x	off	off	shift
x	ON	off	x
Meta-x	ON	off	shift
Alt-x	ON	off	\*(Es x
Alt+Meta-x	ON	off	\*(Es shift
x	off	ON	x
Meta-x	off	ON	\*(Es x
Alt-x	off	ON	shift
Alt+Meta-x	off	ON	\*(Es shift
x	ON	ON	x
Meta-x	ON	ON	\*(Es x
Alt-x	ON	ON	\*(Es x
Alt+Meta-x	ON	ON	\*(Es x
_
.TE
.Sh "PC-Style Function Keys"
.LP
If \fIxterm\fP does minimal translation of the function keys, it usually does this
with a PC-style keyboard, so PC-style function keys result.
Sun keyboards are similar to PC keyboards.
Both have cursor and scrolling operations printed on the keypad,
which duplicate the smaller cursor and scrolling keypads.
.lP
X does not predefine NumLock (used for VT220 keyboards) or Alt (used as
an extension for the Sun/PC keyboards) as modifiers.
These keys are recognized as modifiers when enabled
by the \fBnumLock\fP resource,
or by the "DECSET \*1\*0\*3\*5" control sequence.
.lP
The cursor keys transmit the following escape sequences depending on the
mode specified via the \fBDECCKM\fP escape sequence.
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Normal	Application
_
.TH
.T&
l | l | l .
Cursor Up	\*(Cs\*A	\*(S3\*A
Cursor Down	\*(Cs\*(cB	\*(S3\*(cB
Cursor Right	\*(Cs\*C	\*(S3\*C
Cursor Left	\*(Cs\*D	\*(S3\*D
_
.TE
The home- and end-keys
(unlike PageUp and other keys also on the 6-key editing keypad)
are considered "cursor keys" by \fIxterm\fP.
Their mode is also controlled by the \fBDECCKM\fP escape sequence:
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Normal	Application
_
.TH
.T&
l | l | l .
Home	\*(Cs\*H	\*(S3\*H
End	\*(Cs\*F	\*(S3\*F
_
.TE
.lP
The application keypad transmits the following escape sequences depending on the
mode specified via the \fBDECKPNM\fP and \fBDECKPAM\fP escape sequences.
Use the NumLock key to override the application mode.
.lP
Not all keys are present on the Sun/PC keypad (e.g., PF1, Tab), but are supported by
the program.
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Numeric	Application	Terminfo	Termcap
_
.TH
.T&
l | l | l | l | l .
Space	\*(Sp	\*(S3\*(Sp	-	-
Tab	\*(Ta	\*(S3\*I	-	-
Enter	\*(Cr	\*(S3\*M	kent	@8
PF1	\*(S3\*P	\*(S3\*P	kf1	k1
PF2	\*(S3\*Q	\*(S3\*Q	kf2	k2
PF3	\*(S3\*R	\*(S3\*R	kf3	k3
PF4	\*(S3\*S	\*(S3\*S	kf4	k4
* \f1(multiply)\fP	\**	\*(S3\*j	-	-
+ \f1(add)\fP	\*+	\*(S3\*k	-	-
, \f1(comma)\fP	\*,	\*(S3\*l	-	-
- \f1(minus)\fP	\*-	\*(S3\*m	-	-
\&. \f1(Delete)\fP	\*.	\*(Cs\*3\*(c~	-	-
/ \f1(divide)\fP	\*/	\*(S3\*o	-	-
0 \f1(Insert)\fP	\*0	\*(Cs\*2\*(c~	-	-
1 \f1(End)\fP	\*1	\*(S3\*F	kc1	K4
2 \f1(DownArrow)\fP	\*2	\*(Cs\*(cB	-	-
3 \f1(PageDown)\fP	\*3	\*(Cs\*6\*(c~	kc3	K5
4 \f1(LeftArrow)\fP	\*4	\*(Cs\*D	-	-
5 \f1(Begin)\fP	\*5	\*(Cs\*E	kb2	K2
6 \f1(RightArrow)\fP	\*6	\*(Cs\*C	-	-
7 \f1(Home)\fP	\*7	\*(S3\*H	ka1	K1
8 \f1(UpArrow)\fP	\*8	\*(Cs\*A	-	-
9 \f1(PageUp)\fP	\*9	\*(Cs\*5\*(c~	ka3	K3
= (equal)	\*=	\*(S3\*(XX	-	-
_
.TE
.br
They also provide 12 function keys, as well as a few other special-purpose keys:
.TS H
center;
lf3w(2c)	lf3w(2c) .
Key	Escape Sequence
_
.TH
.T&
l | l .
F1	\*(S3\*P
F2	\*(S3\*Q
F3	\*(S3\*R
F4	\*(S3\*S
F5	\*(Cs\*1\*5\*(c~
F6	\*(Cs\*1\*7\*(c~
F7	\*(Cs\*1\*8\*(c~
F8	\*(Cs\*1\*9\*(c~
F9	\*(Cs\*2\*0\*(c~
F10	\*(Cs\*2\*1\*(c~
F11	\*(Cs\*2\*3\*(c~
F12	\*(Cs\*2\*4\*(c~
_
.TE
.sP
Note that F1 through F4 are prefixed with \*(S3,
while the other keys are prefixed with \*(Cs.
Older versions of \fIxterm\fP implement different escape sequences
for F1 through F4, with a \*(Cs prefix.
These can be activated by setting the \fBoldXtermFKeys\fP resource.
However, since they do not correspond to any hardware terminal,
they have been deprecated.
(The DEC VT220 reserves F1 through F5 for local functions such as \fBSetup\fP).
.TS H
center;
lf3w(2c)	lf3w(2c) .
Key	Escape Sequence
_
.TH
.T&
l | l .
F1	\*(Cs\*1\*1\*(c~
F2	\*(Cs\*1\*2\*(c~
F3	\*(Cs\*1\*3\*(c~
F4	\*(Cs\*1\*4\*(c~
_
.TE
In normal mode, i.e., a Sun/PC keyboard
when the \fBsunKeyboard\fP resource is false
(and none of the other keyboard resources
such as \fBoldXtermFKeys\fP resource is set),
\fIxterm\fP encodes function key modifiers
as parameters appended before the \fIfinal\fP character of the control sequence.
As a special case,
the \*(S3 sent before F1 through F4 is altered to \*(Cs when sending
a function key modifier as a parameter.
.TS H
center;
cf3w(2c)	lf3w(2c) .
Code	Modifiers
_
.TH
.T&
c | l .
2	Shift
3	Alt
4	Shift + Alt
5	Control
6	Shift + Control
7	Alt + Control
8	Shift + Alt + Control
9	Meta
10	Meta + Shift
11	Meta + Alt
12	Meta + Alt + Shift
13	Meta + Ctrl
14	Meta + Ctrl + Shift
15	Meta + Ctrl + Alt
16	Meta + Ctrl + Alt + Shift
_
.TE
For example, shift-F5 would be sent as
\*(Cs\*1\*5\*;\*2\*(c~
.lP
If the \fBalwaysUseMods\fP resource is set, the Meta modifier also is
recognized, making parameters 9 through 16.
.Sh "VT220-Style Function Keys"
.LP
However, \fIxterm\fP is most useful as a DEC VT102 or VT220 emulator.
Set the \fBsunKeyboard\fP resource to true to force a Sun/PC keyboard
to act like a VT220 keyboard.
.lP
The VT102/VT220 application keypad transmits unique escape sequences in
application mode, which are distinct from the cursor and scrolling keypad:
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Numeric	Application
_
.TH
.T&
l | l | l .
Space	\*(Sp	\*(S3\*(Sp
Tab	\*(Ta	\*(S3\*I
Enter	\*(Cr	\*(S3\*M
PF1	\*(S3\*P	\*(S3\*P
PF2	\*(S3\*Q	\*(S3\*Q
PF3	\*(S3\*R	\*(S3\*R
PF4	\*(S3\*S	\*(S3\*S
* \f1(multiply)\fP	\**	\*(S3\*j
+ \f1(add)\fP	\*+	\*(S3\*k
, \f1(comma)\fP	\*,	\*(S3\*l
- \f1(minus)\fP	\*-	\*(S3\*m
\&. \f1(period)\fP	\*.	\*(S3\*n
/ \f1(divide)\fP	\*/	\*(S3\*o
0	\*0	\*(S3\*p
1	\*1	\*(S3\*q
2	\*2	\*(S3\*r
3	\*3	\*(S3\*(cs
4	\*4	\*(S3\*t
5	\*5	\*(S3\*u
6	\*6	\*(S3\*v
7	\*7	\*(S3\*w
8	\*8	\*(S3\*x
9	\*9	\*(S3\*y
= (equal)	\*=	\*(S3\*(XX
_
.TE
The VT220 provides a 6-key editing keypad,
which is analogous to that on the PC keyboard.
It is not affected by \fBDECCKM\fP or \fBDECKPNM\fP/\fBDECKPAM\fP:
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Normal	Application
_
.TH
.T&
l | l | l .
\f1Insert\fP	\*(Cs\*2\*(c~	\*(Cs\*2\*(c~
\f1Delete\fP	\*(Cs\*3\*(c~	\*(Cs\*3\*(c~
\f1Home\fP	\*(Cs\*1\*(c~	\*(Cs\*1\*(c~
\f1End\fP	\*(Cs\*4\*(c~	\*(Cs\*4\*(c~
\f1PageUp\fP	\*(Cs\*5\*(c~	\*(Cs\*5\*(c~
\f1PageDown\fP	\*(Cs\*6\*(c~	\*(Cs\*6\*(c~
_
.TE
.LP
The VT220 provides 8 additional function keys.
With a Sun/PC keyboard, access these keys by Control/F1 for F13, etc.
.TS H
center;
lf3w(2c)	lf3w(2c) .
Key	Escape Sequence
_
.TH
.T&
l | l .
F13	\*(Cs\*2\*5\*(c~
F14	\*(Cs\*2\*6\*(c~
F15	\*(Cs\*2\*8\*(c~
F16	\*(Cs\*2\*9\*(c~
F17	\*(Cs\*3\*1\*(c~
F18	\*(Cs\*3\*2\*(c~
F19	\*(Cs\*3\*3\*(c~
F20	\*(Cs\*3\*4\*(c~
_
.TE
.Sh "VT52-Style Function Keys"
.LP
A VT52 does not have function keys, but it does have a numeric keypad and cursor keys.
They differ from the other emulations by the prefix.
Also, the cursor keys do not change:
.TS H
center;
lf3w(2c)	lf3w(2c) .
Key	Normal/Application
_
.TH
.T&
l | l .
Cursor Up	\*(Es\*A
Cursor Down	\*(Es\*(cB
Cursor Right	\*(Es\*C
Cursor Left	\*(Es\*D
_
.TE
The keypad is similar:
.TS H
center;
lf3w(2c)	lf3w(2c)	lf3w(2c) .
Key	Numeric	Application
_
.TH
.T&
l | l | l .
Space	\*(Sp	\*(Es\*?\*(Sp
Tab	\*(Ta	\*(Es\*?\*I
Enter	\*(Cr	\*(Es\*?\*M
PF1	\*(Es\*P	\*(Es\*P
PF2	\*(Es\*Q	\*(Es\*Q
PF3	\*(Es\*R	\*(Es\*R
PF4	\*(Es\*S	\*(Es\*S
* \f1(multiply)\fP	\**	\*(Es\*?\*j
+ \f1(add)\fP	\*+	\*(Es\*?\*k
, \f1(comma)\fP	\*,	\*(Es\*?\*l
- \f1(minus)\fP	\*-	\*(Es\*?\*m
\&. \f1(period)\fP	\*.	\*(Es\*?\*n
/ \f1(divide)\fP	\*/	\*(Es\*?\*o
0	\*0	\*(Es\*?\*p
1	\*1	\*(Es\*?\*q
2	\*2	\*(Es\*?\*r
3	\*3	\*(Es\*?\*(cs
4	\*4	\*(Es\*?\*t
5	\*5	\*(Es\*?\*u
6	\*6	\*(Es\*?\*v
7	\*7	\*(Es\*?\*w
8	\*8	\*(Es\*?\*x
9	\*9	\*(Es\*?\*y
= (equal)	\*=	\*(Es\*?\*(XX
_
.TE
.Sh "Sun-Style Function Keys"
.LP
The \fIxterm\fP program provides support for Sun keyboards more directly, by
a menu toggle that causes it to send Sun-style function key codes rather than VT220.
Note, however, that the \fIsun\fP and \fIVT100\fP emulations are not really
compatible.
For example, their wrap-margin behavior differs.
.lP
Only function keys are altered; keypad and cursor keys are the same.
The emulation responds identically.
See the xterm-sun terminfo entry for details.
.Sh "HP-Style Function Keys"
.LP
Similarly, \fIxterm\fP can be compiled to support HP keyboards.
See the xterm-hp terminfo entry for details.
.Sh "The Alternate Screen Buffer"
.LP
\fIXterm\fP maintains two screen buffers.
The Normal Screen Buffer allows you to scroll back to view saved lines
of output up to the maximum set by the \fBsaveLines\fP resource.
The \fIAlternate Screen Buffer\fP is exactly as large as the display,
contains no additional saved lines.
When the \fIAlternate Screen Buffer\fP is active,
you cannot scroll back to view saved lines.
\fIXterm\fP provides control sequences and menu entries
for switching between the two.
.lP
Most full-screen applications use terminfo or termcap to obtain
strings used to start/stop full-screen mode,
i.e., \fIsmcup\fP and \fIrmcup\fP for terminfo,
or the corresponding \fIti\fP and \fIte\fP for termcap.
The \fBtiteInhibit\fP resource removes the \fIti\fP and \fIte\fP strings
from the TERMCAP string which is set in the environment for some platforms.
That is not done when \fIxterm\fP is built with terminfo libraries because
terminfo does not provide the whole text of the termcap data in one piece.
It would not work for terminfo anyway, since terminfo data is not passed
in environment variables;
setting an environment variable in this manner would have no effect on
the application's ability to switch
between \fINormal\fP and \fIAlternate Screen\fP buffers.
Instead, the newer private mode controls
(such as \*1\*0\*4\*9)
for switching between \fINormal\fP
and \fIAlternate Screen\fP buffers simply disable the switching.
They add other features such as clearing the display for the same reason:
to make the details of switching independent of the application that
requests the switch.
.
.Sh "Bracketed Paste Mode"
.LP
When bracketed paste mode is set,
pasted text is bracketed with control sequences
so that the program can differentiate pasted text from typed-in text.
When bracketed paste mode is set,
the program will receive:
   \*(Es\*([[\*2\*0\*0\*~,
.br
followed by the pasted text, followed by
   \*(Es\*([[\*2\*0\*1\*~.
.
.Sh "Title Modes"
.LP
The window- and icon-labels can be set or queried using control sequences.
As a VT220-emulator, \fIxterm\fP "should" limit the character encoding for
the corresponding strings to ISO-8859-1.
Indeed, it used to be the case (and was documented) that
window titles had to be ISO-8859-1.
This is no longer the case.
However, there are many applications which still assume that titles are
set using ISO-8859-1.
So that is the default behavior.
.lP
If \fIxterm\fP is running with UTF-8 encoding,
it is possible to use window- and icon-labels encoded using UTF-8.
That is because the underlying X libraries (and many, but not all)
window managers support this feature.
.lP
The \fButf8Title\fP X resource setting tells \fIxterm\fP to disable
a reconversion of the title string back to ISO-8859-1,
allowing the title strings to be interpreted as UTF-8.
The same feature can be enabled using the title mode control sequence
described in this summary.
.lP
Separate from the ability to set the titles,
\fIxterm\fP provides the ability to query the titles,
returning them either in ISO-8859-1 or UTF-8.
This choice is available only while \fIxterm\fP is using UTF-8 encoding.
.lP
Finally, the characters sent to, or returned by a title control
are less constrained than the rest of the control sequences.
To make them more manageable (and constrained), for use in shell scripts,
\fIxterm\fP has an optional feature which decodes the string from hexadecimal
(for setting titles) or for encoding the title into hexadecimal when querying
the value.
.
.Sh "Mouse Tracking"
.LP
The VT widget can be set to send the mouse position and other
information on button presses.
These modes are typically used by
editors and other full-screen applications that want to make use of
the mouse.
.lP
There are two sets of mutually exclusive modes:
.bP
mouse protocol
.bP
protocol encoding
.lP
The mouse protocols include
DEC Locator mode, enabled by the
DECELR \*(Cs\*(Ps\*s\*;\*(Ps\*s\*s\*(qu\*s\*z
control sequence, and is not described here
(control sequences are summarized above).
The remaining five modes of the mouse protocols
are each enabled (or disabled) by a different parameter in
the "DECSET \*(Cs\*?\*(Pm\*s\*h"
or
"DECRST \*(Cs\*?\*(Pm\*s\*l"
control sequence.
.lP
Manifest constants for the parameter values
are defined in \fBxcharmouse.h\fP as follows:
.
.ID
.ft CW
#define SET_X10_MOUSE               9
#define SET_VT200_MOUSE             1000
#define SET_VT200_HIGHLIGHT_MOUSE   1001
#define SET_BTN_EVENT_MOUSE         1002
#define SET_ANY_EVENT_MOUSE         1003

#define SET_FOCUS_EVENT_MOUSE       1004

#define SET_EXT_MODE_MOUSE          1005
#define SET_SGR_EXT_MODE_MOUSE      1006
#define SET_URXVT_EXT_MODE_MOUSE    1015

#define SET_ALTERNATE_SCROLL        1007
.ft 1
.DE
.br
The motion reporting modes are strictly \fIxterm\fP extensions, and are not
part of any standard, though they are analogous to the DEC VT200 DECELR
locator reports.
.lP
Normally,
parameters (such as pointer position and button number) for all mouse
tracking escape sequences generated by \fIxterm\fP
encode numeric parameters in a single character as
\fIvalue\fP+32.
For example, \*! specifies the value 1.
The upper left character position on the terminal is denoted as 1,1.
This scheme dates back to X10,
though the normal mouse-tracking (from X11) is more elaborate.
.Ss X10 compatibility mode
.lP
X10 compatibility mode sends an escape sequence only on button press,
encoding the location and the mouse button pressed.
It is enabled by specifying parameter 9 to DECSET.
On button press, \fIxterm\fP sends
\*(Cs\*M\*(Cb\*(Cx\*(Cy (6 characters).
.bP
\*(Cb is button\-1.
.bP
\*(Cx and \*(Cy are the x and y coordinates of the mouse when the
button was pressed.
.Ss Normal tracking mode
.lP
Normal tracking mode sends an escape sequence on both button press and
release.
Modifier key (shift, ctrl, meta) information is also sent.
It is enabled by specifying parameter 1000 to DECSET.
On button press or release, \fIxterm\fP sends
\*(Cs\*M\*(Cb\*(Cx\*(Cy.
.bP
The low two bits of \*(Cb encode button information:
0=MB1 pressed, 1=MB2 pressed, 2=MB3 pressed, 3=release.
.bP
The next three bits encode the modifiers which were down when the button was
pressed and are added together:  4=Shift, 8=Meta, 16=Control.
Note however that the shift and control bits are normally unavailable
because \fIxterm\fP uses the control modifier with mouse for popup menus,
and the shift modifier is used in the default translations for button events.
The \fIMeta\fP modifier recognized by \fIxterm\fP is the \fImod1\fP mask,
and is not necessarily the "Meta" key (see \fBxmodmap\fP(1)).
.bP
\*(Cx and \*(Cy are the x and y coordinates of the mouse event, encoded as
in X10 mode.
.Ss Wheel mice
.lP
Wheel mice may return buttons 4 and 5.
Those buttons are represented by the same event codes
as buttons 1 and 2 respectively,
except that 64 is added to the event code.
Release events for the wheel buttons are not reported.
By default,
the wheel mouse events are translated to \fIscroll-back\fP
and \fIscroll-forw\fP actions.
Those actions normally scroll the whole window,
as if the scrollbar was used.
However if \fIAlternate Scroll\fP mode is set,
then cursor up/down controls are sent when the terminal is displaying
the \fIAlternate Screen Buffer\fP.
The initial state of \fIAlternate Scroll\fP mode is set
using the \fBalternateScroll\fP resource.
.Ss Highlight tracking
.lP
Mouse highlight tracking notifies a program of a button press, receives a
range of lines from the program, highlights the region covered by
the mouse within that range until button release, and then sends the
program the release coordinates.
It is enabled by specifying parameter 1001 to DECSET.
Highlighting is performed only for button 1, though other button events
can be received.
.sP
\fBWarning:\fP use of this mode requires a cooperating program or it will
hang \fIxterm.\fP
.sP
On button press, the same information as for normal tracking is
generated; \fIxterm\fP then waits for the program to send mouse
tracking information.
\fIAll X events are ignored until the proper escape sequence is
received from the pty:\fP
\*(Cs\*(Ps\*s\*;\*(Ps\*s\*;\*(Ps\*s\*;\*(Ps\*s\*;\*(Ps\*s\*T.
The parameters are \fIfunc, startx, starty, firstrow,\fP and \fIlastrow.\fP
\fIfunc\fP is non-zero to initiate highlight tracking and zero to abort.
\fIstartx\fP and \fIstarty\fP give the starting x and y location for
the highlighted region.
The ending location tracks the mouse, but
will never be above row \fIfirstrow\fP and will always be above row
\fIlastrow.\fP
(The top of the screen is row 1.)
When the button is released, \fIxterm\fP reports the ending position
one of two ways:
.bP
if the start and end coordinates are the same locations:
.br
\*(Cs\*t\*(Cx\*(Cy.
.bP
otherwise:
.br
\*(Cs\*T\*(Cx\*(Cy\*(Cx\*(Cy\*(Cx\*(Cy.
.br
The parameters are \fIstartx, starty, endx, endy, mousex,\fP and \fImousey.\fP
.RS
.IP \- 0.2i
\fIstartx, starty, endx, \fPand\fI endy\fP give the starting and
ending character positions of the region.
.IP \- 0.2i
\fImousex\fP and \fImousey\fP
give the location of the mouse at button up, which may not be over a
character.
.RE
.Ss Button-event tracking
.lP
Button-event tracking is essentially the same as normal tracking, but
\fIxterm\fP also reports button-motion events.
Motion events
are reported only if the mouse pointer has moved to a different character
cell.
It is enabled by specifying parameter 1002 to DECSET.
On button press or release, \fIxterm\fP sends the same codes used by normal
tracking mode.
.bP
On button-motion events, \fIxterm\fP adds 32 to the event code
(the third character, \*(Cb).
.bP
The other bits of the event code specify button and modifier keys as in normal mode.
For example, motion into cell x,y with button 1
down is reported as \*(Cs\*M\*@\*(Cx\*(Cy.
(\ \*@ = 32 + 0 (button 1) + 32 (motion indicator)\ ).
Similarly, motion with button 3
down is reported as \*(Cs\*M\*(cB\*(Cx\*(Cy.
(\ \*(cB = 32 + 2 (button 3) + 32 (motion indicator)\ ).
.Ss Any-event tracking
.lP
Any-event mode is the same as button-event mode, except that all motion
events are reported, even if no mouse button is down.
It is enabled by specifying 1003 to DECSET.
.Ss FocusIn/FocusOut
.lP
FocusIn/FocusOut can be combined with any of the mouse events since
it uses a different protocol.
When set, it causes \fIxterm\fP to send
\*(Cs\*I when the terminal gains focus, and
\*(Cs\*O when it loses focus.
.Ss Extended coordinates
.lP
The original X10 mouse protocol limits the \*(Cx and \*(Cy ordinates
to 223 (=255\ -\ 32).
\fIXterm\fP supports more than one scheme for extending this range,
by changing the protocol encoding:
.IP "UTF-8 (1005)"
This enables UTF-8 encoding for \*(Cx and \*(Cy under
all tracking modes, expanding the maximum encodable position from 223 to 2015.
For positions less than 95, the resulting output is identical under both modes.
Under extended mouse mode, positions greater than 95
generate "extra" bytes which will confuse applications which do not treat
their input as a UTF-8 stream.
Likewise, \*(Cb will be UTF-8 encoded,
to reduce confusion with wheel mouse events.
.IP
Under normal mouse mode, positions outside (160,94) result in
byte pairs which can be interpreted as a single UTF-8 character;
applications
which do treat their input as UTF-8 will almost certainly be confused
unless extended mouse mode is active.
.IP
This scheme has the drawback that the encoded coordinates will not
pass through \fIluit\fP unchanged, e.g., for locales using non-UTF-8 encoding.
.IP "SGR (1006)"
The normal mouse response is altered to use \*(Cs\*<
followed by semicolon-separated
encoded button value,
the \*(Cx and \*(Cy ordinates and a final character which
is \*M for button press
and \*m for button release.
.RS
.bP
The encoded button value in this case does not add 32 since
that was useful only in the X10 scheme for ensuring that the
byte containing the button value is a printable code.
.bP
The modifiers are encoded in the same way.
.bP
A different final character is used for button release
to resolve the X10 ambiguity regarding which button was released.
.RE
.IP
The highlight tracking responses are also modified to an SGR-like format,
using the same SGR-style scheme and button-encodings.
.IP "URXVT (1015)"
The normal mouse response is altered to use \*(Cs
followed by semicolon-separated
encoded button value,
the \*(Cx and \*(Cy ordinates and final character \*M.
.IP
This uses the same button encoding as X10, but printing it as
a decimal integer rather than as a single byte.
.IP
However, \*(Cs\*M can be mistaken for DL (delete lines),
while the highlight tracking \*(Cs\*T can be mistaken for SD (scroll down),
and the Window manipulation controls.
For these reasons, the 1015 control is not recommended;
it is not an improvement over 1005.
.
.Sh "Sixel Graphics"
.LP
If \fI\*(xt\fP is configured as
VT240,
VT241,
VT330,
VT340 or
VT382
using the \fBdecTerminalID\fP resource,
it supports Sixel Graphics controls, a palleted bitmap graphics system
using sets of six vertical pixels as the basic element.
.St
.IP \\*(Cs\\*(Ps\\*s\\*c
\fI\*(xt\fP responds to Send Device Attributes (Primary DA) with these
additional codes:
  \*(Ps = \*4 \(-> Sixel graphics.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*h
\fI\*(xt\fP has these additional private Set Mode values:
  \*(Ps = \*8\*0 \(-> Sixel scrolling.
  \*(Ps = \*1\*0\*7\*0 \(-> use private color registers for each graphic.
  \*(Ps = \*8\*4\*5\*2 \(-> Sixel scrolling leaves cursor to right of graphic.
.IP \\*(Dc\\*(Pa\\*;\\*(Pb\\*;\\*(Ph\\*s\\*q\\*s\\*(Ps..\\*(Ps\fP\\*s\\*(ST
See:
.ID
http://vt100.net/docs/vt3xx-gp/chapter14.html
.DE
The sixel data device control string has three positional parameters,
following the \*q with sixel data.
  \*(Pa \(-> pixel aspect ratio
  \*(Pb \(-> background color option
  \*(Ph \(-> horizontal grid size (ignored).
  \*(Ps \(-> sixel data
.Ed
.
.Sh "ReGIS Graphics"
.LP
If \fI\*(xt\fP is configured as
VT125,
VT240,
VT241,
VT330 or
VT340
using the \fBdecTerminalID\fP resource,
it supports Remote Graphic Instruction Set, a graphics description language.
.St
.IP \\*(Cs\\*(Ps\\*s\\*c
\fI\*(xt\fP responds to Send Device Attributes (Primary DA) with these
additional codes:
  \*(Ps = \*3 \(-> ReGIS graphics.
.IP \\*(Cs\\*?\\*(Pm\\*s\\*h
\fI\*(xt\fP has these additional private Set Mode values:
  \*(Ps = \*1\*0\*7\*0 \(-> use private color registers for each graphic.
.IP \\*(Dc\\*(Pm\\*s\\*p\\*(Pr..\\*(Pr\fP\\*s\\*(ST
See:
.ID
http://vt100.net/docs/vt3xx-gp/chapter1.html
.DE
The ReGIS data device control string has one positional parameter
with four possible values:
  \*(Pm = 0 \(-> resume command, use fullscreen mode.
  \*(Pm = 1 \(-> start new command, use fullscreen mode.
  \*(Pm = 2 \(-> resume command, use command display mode.
  \*(Pm = 3 \(-> start new command, use command display mode.
.Ed
.
.Sh "Tektronix 4014 Mode"
.LP
Most of these sequences are standard Tektronix 4014 control sequences.
Graph mode supports the 12-bit addressing of the Tektronix 4014.
The major features missing are
the write-through and defocused modes.
This document does not describe the commands used in the various
Tektronix plotting modes but does describe the commands to switch modes.
.St
.IP \\*(Be
Bell (Ctrl-G).
.IP \\*(Bs
Backspace (Ctrl-H).
.IP \\*(Ta
Horizontal Tab (Ctrl-I).
.IP \\*(Lf
Line Feed or New Line (Ctrl-J).
.IP \\*(Vt
Cursor up (Ctrl-K).
.IP \\*(Ff
Form Feed or New Page (Ctrl-L).
.IP \\*(Cr
Carriage Return (Ctrl-M).
.IP \\*(Es\\*(Et
Switch to VT100 Mode (\*(Es Ctrl-C).
.IP \\*(Es\\*(En
Return Terminal Status (\*(Es Ctrl-E).
.IP \\*(Es\\*(Ff
PAGE (Clear Screen) (\*(Es Ctrl-L).
.IP \\*(Es\\*(So
Begin 4015 APL mode (\*(Es Ctrl-N).  (This is ignored by \fIxterm\fP).
.IP \\*(Es\\*(Si
End 4015 APL mode (\*(Es Ctrl-O).  (This is ignored by \fIxterm\fP).
.IP \\*(Es\\*(Eb
COPY (Save Tektronix Codes to file COPY\fIyyyy-mm-dd.hh:mm:ss\fP).
  \*(Eb (end transmission block) is the same as Ctrl-W.
.IP \\*(Es\\*(Ca
Bypass Condition (\*(Es Ctrl-X).
.IP \\*(Es\\*(Su
GIN mode (\*(Es Ctrl-Z).
.IP \\*(Es\\*(Fs
Special Point Plot Mode (\*(Es Ctrl-\e).
.IP \\*(Es\\*8
Select Large Character Set.
.IP \\*(Es\\*9
Select #2 Character Set.
.IP \\*(Es\\*:
Select #3 Character Set.
.IP \\*(Es\\*;
Select Small Character Set.
.IP \\*(Os\\*(Ps\\*s\\*;\\*(Pt\\*s\\*(Be
Set Text Parameters of VT window.
  \*(Ps = \*0 \(-> Change Icon Name and Window Title to \*(Pt.
  \*(Ps = \*1 \(-> Change Icon Name to \*(Pt.
  \*(Ps = \*2 \(-> Change Window Title to \*(Pt.
  \*(Ps = \*4\*6 \(-> Change Log File to \*(Pt.  (This is normally disabled by a
compile-time option).
.IP \\*(Es\\*`
Normal Z Axis and Normal (solid) Vectors.
.IP \\*(Es\\*a
Normal Z Axis and Dotted Line Vectors.
.IP \\*(Es\\*b
Normal Z Axis and Dot-Dashed Vectors.
.IP \\*(Es\\*c
Normal Z Axis and Short-Dashed Vectors.
.IP \\*(Es\\*d
Normal Z Axis and Long-Dashed Vectors.
.IP \\*(Es\\*h
Defocused Z Axis and Normal (solid) Vectors.
.IP \\*(Es\\*i
Defocused Z Axis and Dotted Line Vectors.
.IP \\*(Es\\*j
Defocused Z Axis and Dot-Dashed Vectors.
.IP \\*(Es\\*k
Defocused Z Axis and Short-Dashed Vectors.
.IP \\*(Es\\*l
Defocused Z Axis and Long-Dashed Vectors.
.IP \\*(Es\\*p
Write-Thru Mode and Normal (solid) Vectors.
.IP \\*(Es\\*q
Write-Thru Mode and Dotted Line Vectors.
.IP \\*(Es\\*r
Write-Thru Mode and Dot-Dashed Vectors.
.IP \\*(Es\\*(cs
Write-Thru Mode and Short-Dashed Vectors.
.IP \\*(Es\\*t
Write-Thru Mode and Long-Dashed Vectors.
.IP \\*(Fs
Point Plot Mode (Ctrl-\e).
.IP \\*(Gs
Graph Mode (Ctrl-]).
.IP \\*(Rs
Incremental Plot Mode (Ctrl-^).
.IP \\*(Us
Alpha Mode (Ctrl-_).
.Ed
.
.
.Sh "VT52 Mode"
.LP
Parameters for cursor movement are at the end of the \*(Es\*Y escape sequence.
Each ordinate is encoded in a single character as \fIvalue\fP+32.
For example, \*! is 1.
The screen coordinate system is 0-based.
.St
.IP \\*(Es\\*<
Exit VT52 mode (Enter VT100 mode).
.IP \\*(Es\\*=
Enter alternate keypad mode.
.IP \\*(Es\\*>
Exit alternate keypad mode.
.IP \\*(Es\\*A
Cursor up.
.IP \\*(Es\\*(cB
Cursor down.
.IP \\*(Es\\*C
Cursor right.
.IP \\*(Es\\*D
Cursor left.
.IP \\*(Es\\*F
Enter graphics mode.
.IP \\*(Es\\*G
Exit graphics mode.
.IP \\*(Es\\*H
Move the cursor to the home position.
.IP \\*(Es\\*I
Reverse line feed.
.IP \\*(Es\\*J
Erase from the cursor to the end of the screen.
.IP \\*(Es\\*K
Erase from the cursor to the end of the line.
.IP \\*(Es\\*Y\\*(Ps\\*s\\*(Ps
Move the cursor to given row and column.
.IP \\*(Es\\*Z
Identify.
  \(-> \*(Es\*s\*/\*s\*Z (\*(``I am a VT52.\*('').
.Ed
.
.if n .pl \n(nlu+1v
