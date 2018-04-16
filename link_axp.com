$! $XFree86: xc/programs/xterm/link_axp.com,v 1.1 2000/02/08 17:19:35 dawes Exp $
$ SAVE_VERIFY='F$VERIFY(0)
$ if p1 .Eqs. "CLEAN" then goto clean
$ if p1 .Eqs. "CLOBBER" then goto clobber
$ if p1 .Eqs. "INSTALL" then goto install
$!
$!	Compile the X11R4 Xterm application
$!
$ Set Symbol/Scope=NoGlobal
$!
$!  Define logicals pointing to the needed directories
$!
$ x11lib_device = f$parse("[.lib]",,,"DEVICE")
$ x11lib_directory = f$parse("[.lib]",,,"DIRECTORY")
$ define/nolog x11lib 'x11lib_device''x11lib_directory'
$!
$ x11inc_device = f$parse("[]",,,"DEVICE")
$ x11inc_directory = f$parse("[]",,,"DIRECTORY")
$ define/nolog x11inc 'x11inc_device''x11inc_directory'
$!
$ xmu_device = f$parse("[.lib.xmu]",,,"DEVICE")
$ xmu_directory = f$parse("[.lib.xmu]",,,"DIRECTORY")
$ define/nolog x11xmu 'xmu_device''xmu_directory'
$!
$ xbm_device = f$parse("[.lib.x11]",,,"DEVICE")
$ xbm_directory = f$parse("[.lib.x11]",,,"DIRECTORY")
$ define/nolog x11xbm 'xbm_device''xbm_directory'
$!
$ xaw_device = f$parse("[.lib.xaw]",,,"DEVICE")
$ xaw_directory = f$parse("[.lib.xaw]",,,"DIRECTORY")
$ define/nolog x11xaw 'xaw_device''xaw_directory'
$!
$ x11vms_device = f$parse("[.lib.misc]",,,"DEVICE")
$ x11vms_directory = f$parse("[.lib.misc]",,,"DIRECTORY")
$ define/nolog x11vms 'x11vms_device''x11vms_directory'
$!
$!  Get the compiler options via the logical name COPTS
$!
$ cc_options = f$trnlnm("COPTS")
$!
$!  Get the linker options via the logical name LOPTS
$!
$ link_options = f$trnlnm("LOPTS")
$!
$ write sys$output "Building XTERM Image"
$ CALL MAKE XTERM.EXE	"LINK ''link_options' /EXE=XTERM.EXE_AXP/CROSS/FULL/MAP=XTERM.MAP XTERM_AXP/OPT" *.OBJ
$!
$ deassign x11lib
$ deassign x11vms
$ deassign x11xmu
$ deassign x11xbm
$ deassign x11xaw
$!
$ exit
$!
$ Clobber:	! Delete executables, Purge directory and clean up object files and listings
$ Delete/noconfirm/log *.exe;*
$!
$ Clean:	! Purge directory, clean up object files and listings
$ Purge
$ Delete/noconfirm/log *.lis;*
$ Delete/noconfirm/log *.obj;*
$!
$ exit
$!
$ Install:
$ Copy/log *.exe x11bin:
$ exit
$!
$MAKE: SUBROUTINE   !SUBROUTINE TO CHECK DEPENDENCIES
$ V = 'F$Verify(0)
$! P1 = What we are trying to make
$! P2 = Command to make it
$! P3 - P8  What it depends on
$
$ If F$Search(P1) .Eqs. "" Then Goto Makeit
$ Time = F$CvTime(F$File(P1,"RDT"))
$arg=3
$Loop:
$	Argument = P'arg
$	If Argument .Eqs. "" Then Goto Exit
$	El=0
$Loop2:
$	File = F$Element(El," ",Argument)
$	If File .Eqs. " " Then Goto Endl
$	AFile = ""
$Loop3:
$	OFile = AFile
$	AFile = F$Search(File)
$	If AFile .Eqs. "" .Or. AFile .Eqs. OFile Then Goto NextEl
$	If F$CvTime(F$File(AFile,"RDT")) .Ges. Time Then Goto Makeit
$	Goto Loop3
$NextEL:
$	El = El + 1
$	Goto Loop2
$EndL:
$ arg=arg+1
$ If arg .Le. 8 Then Goto Loop
$ Goto Exit
$
$Makeit:
$ Set Verify
$ 'P2
$ VV='F$Verify(0)
$Exit:
$ If V Then Set Verify
$ENDSUBROUTINE
