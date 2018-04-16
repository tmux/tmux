http://dickey.his.com/xterm/xterm.html

Downloaded 1.22 variant (current Linux version) on 18-JAN-2000.

Port stalled for a few days because OpenVMS X11 lacks XtGravity.

X11KIT shared libraries almost work, but missing _XA_ symbols
for some reason.  

Copied X11KIT [.xaw3d] and [.xmu] into [.lib], put together simplified
build procedures.  Made a few mods.  Merged in some changes from
Patrick Young.  Now these build mostly ok except for tons of bcopy
related warnings and problems with LAYOUT.C. 

Merged changes from Xterm021 into here.

Made changes here and there to get it all to work.

25-JAN-2000, more or less done.  Logging doesn't work but PRINT
does, as does regular VT emulation, TEK emulation, 80 and 132 wide
modes.  The resource file needs work.  Cleaned up a really nasty problem 
with infinite loops on copy/paste in button.c (see tt_pasting).

To build this, if you have DECC, DW 1.2-5 and VMS 7.2-1 (the latter
probably doesn't matter) do:

$ @make

in the top directory.  Expect a bunch of I and W warnings, but nothing 
worse.  Then define a foreign symbol for xterm for the resulting .exe.

26-JAN-2000.  Enabled logging.  When this is turned on from the menu
it creates a new file SYS$SCRATCH:XTERM_LOG.TXT and writes everything that
goes to the screen into it.  This may slow down output a bit as each block
of data read must be copied to disk.  The log file has RMS format stream-lf
and typically has a <CR> at the end of each line. 

27-JAN-2000.  Discovered a bug when doing an X11 paste into an EDT session,
had to add a tt_start_read() in button.c after the paste to reenable the
read AST.  Rearranged code in VMS.C to make the compiler happy and 
eliminate warnings.

David Mathog
mathog@seqaxp.bio.caltech.edu
Manager, sequence analysis facility, biology division, Caltech 


$XFree86: xc/programs/xterm/AAA_README_VMS.txt,v 1.2 2000/06/13 02:28:37 dawes Exp $
