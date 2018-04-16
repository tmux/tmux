$! make.com
$! 25-JAN-2000, David Mathog
$! builds xaw3d, xmu, and then xterm.
$!
$! $XFree86: xc/programs/xterm/make.com,v 1.1 2000/02/08 17:19:37 dawes Exp $
$!
$!************************************************************
$!
$! set up logicals for XAW, XMU and so forth
$!
$ define x11_directory decw$include
$ thisfile = f$environment("PROCEDURE")
$ thisis   = f$parse(thisfile,,,"DEVICE") + f$parse(thisfile,,,"DIRECTORY")
$ thisis = thisis - "]"
$ define xaw_directory "''thisis'.lib.xaw3d]"
$ define xmu_directory "''thisis'.lib.xmu]"
$ define bitmap_directory "''thisis'.lib.bitmaps]"
$ define/trans=(concealed) thisX11 "''thisis.lib.]"
$ define X11 thisx11,decw$include
$!
$! note, ansi doesn't work with this variant of X11R5.
$!
$! don't build libs in debug mode
$ if(P1 .eqs. "" .AND. P2 .eqs. "")
$ then
$!
$! build XMU
$!
$ set ver
$ set def [.lib.xmu]
$ @make
$!
$! build XAW3D
$!
$ set def [-.xaw3d]
$ @make
$ set def [--]
$ set nover
$!
$! move the two libraries to this level
$!
$ rename [.lib...]*.olb []
$ endif
$!
$ if(P1 .nes. "")
$ then
$   ccstub := cc/standard=vaxc/include=[]/debug/noopt
$   mylink :== link/debug
$ else
$   ccstub := cc/standard=vaxc/include=[]
$   mylink :== link
$ endif
$ mycc :== 'ccstub' -
/define=(VMS,OPT_TEK4014,ALLOWLOGGING,OPT_NUM_LOCK)
$!
$! OPT_TOOLBAR doesn't work - it pulls in calls through Xaw3d and Xmu for
$! XSHAPECOMBINEMASK and XSHAPEQUERYEXTENSION
$! which seem not to exist in DW MOtif 1.2-5
$!
$!
$ set ver
$ mycc BUTTON.C
$ mycc CHARPROC.C
$ mycc CHARSETS.C
$ mycc CURSOR.C
$ mycc DATA.C
$ mycc DOUBLECHR.C
$ mycc FONTUTILS.C
$ mycc INPUT.C
$ mycc KEYSYM2UCS.C
$ mycc MAIN.C
$ mycc MENU.C
$ mycc MISC.C
$ mycc PRINT.C
$ mycc PTYDATA.C
$! mycc RESIZE.C
$ mycc SCREEN.C
$ mycc SCROLLBAR.C
$ mycc TABS.C
$ mycc TEKPROC.C
$ mycc TEKPRSTBL.C
$ mycc TRACE.C
$ mycc TTYSVR.C
$ mycc UTIL.C
$ mycc VMS.C
$ mycc VTPRSTBL.C
$!
$ mylink/exe=xterm.exe xterm_axp.opt/option
$ set nover
$ exit
