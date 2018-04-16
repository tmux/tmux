/* $XTermId: charproc.c,v 1.1517 2017/12/28 18:43:58 tom Exp $ */

/*
 * Copyright 1999-2016,2017 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 *
 *
 * Copyright 1988  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from The Open Group.
 *
 */
/*
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* charproc.c */

#include <version.h>
#include <xterm.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xmu/Atoms.h>
#include <X11/Xmu/CharSet.h>
#include <X11/Xmu/Converters.h>

#if OPT_INPUT_METHOD

#if defined(HAVE_LIB_XAW)
#include <X11/Xaw/XawImP.h>
#elif defined(HAVE_LIB_XAW3D)
#include <X11/Xaw3d/XawImP.h>
#elif defined(HAVE_LIB_XAW3DXFT)
#include <X11/Xaw3dxft/XawImP.h>
#elif defined(HAVE_LIB_NEXTAW)
#include <X11/neXtaw/XawImP.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include <X11/XawPlus/XawImP.h>
#endif

#endif

#if OPT_DOUBLE_BUFFER
#include <X11/extensions/Xdbe.h>
#endif

#if OPT_WIDE_CHARS
#include <xutf8.h>
#include <wcwidth.h>
#include <precompose.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif
#endif

#if OPT_INPUT_METHOD
#include <X11/Xlocale.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#if defined(HAVE_SCHED_YIELD)
#include <sched.h>
#endif

#include <VTparse.h>
#include <data.h>
#include <error.h>
#include <menu.h>
#include <main.h>
#include <fontutils.h>
#include <charclass.h>
#include <xstrings.h>
#include <graphics.h>

typedef int (*BitFunc) (unsigned * /* p */ ,
			unsigned /* mask */ );

static IChar doinput(void);
static int set_character_class(char * /*s */ );
static void FromAlternate(XtermWidget /* xw */ );
static void ReallyReset(XtermWidget /* xw */ ,
			Bool /* full */ ,
			Bool /* saved */ );
static void RequestResize(XtermWidget /* xw */ ,
			  int /* rows */ ,
			  int /* cols */ ,
			  Bool /* text */ );
static void SwitchBufs(XtermWidget /* xw */ ,
		       int /* toBuf */ ,
		       Bool /* clearFirst */ );
static void ToAlternate(XtermWidget /* xw */ ,
			Bool /* clearFirst */ );
static void ansi_modes(XtermWidget termw,
		       BitFunc /* func */ );
static int bitclr(unsigned *p, unsigned mask);
static int bitcpy(unsigned *p, unsigned q, unsigned mask);
static int bitset(unsigned *p, unsigned mask);
static void dpmodes(XtermWidget /* xw */ ,
		    BitFunc /* func */ );
static void restoremodes(XtermWidget /* xw */ );
static void savemodes(XtermWidget /* xw */ );
static void window_ops(XtermWidget /* xw */ );

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
#define SettableCursorBlink(screen) \
	(((screen)->cursor_blink != cbAlways) && \
	 ((screen)->cursor_blink != cbNever))
#define UpdateCursorBlink(screen) \
	 SetCursorBlink(screen, screen->cursor_blink)
static void SetCursorBlink(TScreen * /* screen */ ,
			   BlinkOps /* enable */ );
static void HandleBlinking(XtPointer /* closure */ ,
			   XtIntervalId * /* id */ );
static void StartBlinking(TScreen * /* screen */ );
static void StopBlinking(TScreen * /* screen */ );
#else
#define StartBlinking(screen)	/* nothing */
#define StopBlinking(screen)	/* nothing */
#endif

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
static void PreeditPosition(XtermWidget /* xw */ );
#endif

#define	DEFAULT		-1
#define BELLSUPPRESSMSEC 200

static ANSI reply;
static PARAMS parms;

#define nparam parms.count

#define InitParams()  parms.count = parms.is_sub[0] = parms.has_subparams = 0
#define GetParam(n)   parms.params[(n)]
#define SetParam(n,v) parms.params[(n)] = v
#define ParamPair(n)  nparam - (n), parms.params + (n)
#define ParamsDone()  InitParams()

static jmp_buf vtjmpbuf;

/* event handlers */
static void HandleBell PROTO_XT_ACTIONS_ARGS;
static void HandleIgnore PROTO_XT_ACTIONS_ARGS;
static void HandleKeymapChange PROTO_XT_ACTIONS_ARGS;
static void HandleVisualBell PROTO_XT_ACTIONS_ARGS;
#if HANDLE_STRUCT_NOTIFY
static void HandleStructNotify PROTO_XT_EV_HANDLER_ARGS;
#endif

/*
 * NOTE: VTInitialize zeros out the entire ".screen" component of the
 * XtermWidget, so make sure to add an assignment statement in VTInitialize()
 * for each new ".screen" field added to this resource list.
 */

/* Defaults */
#if OPT_ISO_COLORS

/*
 * If we default to colorMode enabled, compile-in defaults for the ANSI colors.
 */
#if DFT_COLORMODE
#define DFT_COLOR(name) name
#else
#define DFT_COLOR(name) XtDefaultForeground
#endif
#endif

static char _Font_Selected_[] = "yes";	/* string is arbitrary */

static const char *defaultTranslations;
/* *INDENT-OFF* */
static XtActionsRec actionsList[] = {
    { "allow-bold-fonts",	HandleAllowBoldFonts },
    { "allow-send-events",	HandleAllowSends },
    { "bell",			HandleBell },
    { "clear-saved-lines",	HandleClearSavedLines },
    { "copy-selection",		HandleCopySelection },
    { "create-menu",		HandleCreateMenu },
    { "delete-is-del",		HandleDeleteIsDEL },
    { "dired-button",		DiredButton },
    { "hard-reset",		HandleHardReset },
    { "ignore",			HandleIgnore },
    { "insert",			HandleKeyPressed },  /* alias for insert-seven-bit */
    { "insert-eight-bit",	HandleEightBitKeyPressed },
    { "insert-selection",	HandleInsertSelection },
    { "insert-seven-bit",	HandleKeyPressed },
    { "interpret",		HandleInterpret },
    { "keymap",			HandleKeymapChange },
    { "popup-menu",		HandlePopupMenu },
    { "print",			HandlePrintScreen },
    { "print-everything",	HandlePrintEverything },
    { "print-redir",		HandlePrintControlMode },
    { "quit",			HandleQuit },
    { "redraw",			HandleRedraw },
    { "scroll-back",		HandleScrollBack },
    { "scroll-forw",		HandleScrollForward },
    { "scroll-to",		HandleScrollTo },
    { "secure",			HandleSecure },
    { "select-cursor-end",	HandleKeyboardSelectEnd },
    { "select-cursor-extend",   HandleKeyboardSelectExtend },
    { "select-cursor-start",	HandleKeyboardSelectStart },
    { "select-end",		HandleSelectEnd },
    { "select-extend",		HandleSelectExtend },
    { "select-set",		HandleSelectSet },
    { "select-start",		HandleSelectStart },
    { "send-signal",		HandleSendSignal },
    { "set-8-bit-control",	Handle8BitControl },
    { "set-allow132",		HandleAllow132 },
    { "set-altscreen",		HandleAltScreen },
    { "set-appcursor",		HandleAppCursor },
    { "set-appkeypad",		HandleAppKeypad },
    { "set-autolinefeed",	HandleAutoLineFeed },
    { "set-autowrap",		HandleAutoWrap },
    { "set-backarrow",		HandleBackarrow },
    { "set-bellIsUrgent",	HandleBellIsUrgent },
    { "set-cursesemul",		HandleCursesEmul },
    { "set-jumpscroll",		HandleJumpscroll },
    { "set-keep-clipboard",	HandleKeepClipboard },
    { "set-keep-selection",	HandleKeepSelection },
    { "set-marginbell",		HandleMarginBell },
    { "set-old-function-keys",	HandleOldFunctionKeys },
    { "set-pop-on-bell",	HandleSetPopOnBell },
    { "set-reverse-video",	HandleReverseVideo },
    { "set-reversewrap",	HandleReverseWrap },
    { "set-scroll-on-key",	HandleScrollKey },
    { "set-scroll-on-tty-output", HandleScrollTtyOutput },
    { "set-scrollbar",		HandleScrollbar },
    { "set-select",		HandleSetSelect },
    { "set-sun-keyboard",	HandleSunKeyboard },
    { "set-titeInhibit",	HandleTiteInhibit },
    { "set-visual-bell",	HandleSetVisualBell },
    { "set-vt-font",		HandleSetFont },
    { "soft-reset",		HandleSoftReset },
    { "start-cursor-extend",	HandleKeyboardStartExtend },
    { "start-extend",		HandleStartExtend },
    { "string",			HandleStringEvent },
    { "vi-button",		ViButton },
    { "visual-bell",		HandleVisualBell },
#ifdef ALLOWLOGGING
    { "set-logging",		HandleLogging },
#endif
#if OPT_ALLOW_XXX_OPS
    { "allow-color-ops",	HandleAllowColorOps },
    { "allow-font-ops",		HandleAllowFontOps },
    { "allow-mouse-ops",	HandleAllowMouseOps },
    { "allow-tcap-ops",		HandleAllowTcapOps },
    { "allow-title-ops",	HandleAllowTitleOps },
    { "allow-window-ops",	HandleAllowWindowOps },
#endif
#if OPT_BLINK_CURS
    { "set-cursorblink",	HandleCursorBlink },
#endif
#if OPT_BOX_CHARS
    { "set-font-linedrawing",	HandleFontBoxChars },
    { "set-font-packed",	HandleFontPacked },
#endif
#if OPT_DABBREV
    { "dabbrev-expand",		HandleDabbrevExpand },
#endif
#if OPT_DEC_CHRSET
    { "set-font-doublesize",	HandleFontDoublesize },
#endif
#if OPT_DEC_SOFTFONT
    { "set-font-loading",	HandleFontLoading },
#endif
#if OPT_SCREEN_DUMPS
    { "dump-html",	        HandleDumpHtml },
    { "dump-svg",	        HandleDumpSvg },
#endif
#if OPT_EXEC_XTERM
    { "spawn-new-terminal",	HandleSpawnTerminal },
#endif
#if OPT_HP_FUNC_KEYS
    { "set-hp-function-keys",	HandleHpFunctionKeys },
#endif
#if OPT_LOAD_VTFONTS
    { "load-vt-fonts",		HandleLoadVTFonts },
#endif
#if OPT_MAXIMIZE
    { "deiconify",		HandleDeIconify },
    { "fullscreen",		HandleFullscreen },
    { "iconify",		HandleIconify },
    { "maximize",		HandleMaximize },
    { "restore",		HandleRestoreSize },
#endif
#if OPT_NUM_LOCK
    { "alt-sends-escape",	HandleAltEsc },
    { "meta-sends-escape",	HandleMetaEsc },
    { "set-num-lock",		HandleNumLock },
#endif
#if OPT_READLINE
    { "readline-button",	ReadLineButton },
#endif
#if OPT_RENDERFONT
    { "set-render-font",	HandleRenderFont },
#endif
#if OPT_SCO_FUNC_KEYS
    { "set-sco-function-keys",	HandleScoFunctionKeys },
#endif
#if OPT_SCROLL_LOCK
    { "scroll-lock",		HandleScrollLock },
#endif
#if OPT_SELECTION_OPS
    { "exec-formatted",		HandleExecFormatted },
    { "exec-selectable",	HandleExecSelectable },
    { "insert-formatted",	HandleInsertFormatted },
    { "insert-selectable",	HandleInsertSelectable },
#endif
#if OPT_SHIFT_FONTS
    { "larger-vt-font",		HandleLargerFont },
    { "smaller-vt-font",	HandleSmallerFont },
#endif
#if OPT_SIXEL_GRAPHICS
    { "set-sixel-scrolling",	HandleSixelScrolling },
#endif
#if OPT_GRAPHICS
    { "set-private-colors",	HandleSetPrivateColorRegisters },
#endif
#if OPT_SUN_FUNC_KEYS
    { "set-sun-function-keys",	HandleSunFunctionKeys },
#endif
#if OPT_TEK4014
    { "set-terminal-type",	HandleSetTerminalType },
    { "set-visibility",		HandleVisibility },
    { "set-tek-text",		HandleSetTekText },
    { "tek-page",		HandleTekPage },
    { "tek-reset",		HandleTekReset },
    { "tek-copy",		HandleTekCopy },
#endif
#if OPT_TOOLBAR
    { "set-toolbar",		HandleToolbar },
#endif
#if OPT_WIDE_CHARS
    { "set-utf8-mode",		HandleUTF8Mode },
    { "set-utf8-fonts",		HandleUTF8Fonts },
    { "set-utf8-title",		HandleUTF8Title },
#endif
};
/* *INDENT-ON* */

#define SPS screen.printer_state

static XtResource xterm_resources[] =
{
    Bres(XtNallowPasteControls, XtCAllowPasteControls,
	 screen.allowPasteControl0, False),
    Bres(XtNallowSendEvents, XtCAllowSendEvents, screen.allowSendEvent0, False),
    Bres(XtNallowColorOps, XtCAllowColorOps, screen.allowColorOp0, DEF_ALLOW_COLOR),
    Bres(XtNallowFontOps, XtCAllowFontOps, screen.allowFontOp0, DEF_ALLOW_FONT),
    Bres(XtNallowMouseOps, XtCAllowMouseOps, screen.allowMouseOp0, DEF_ALLOW_MOUSE),
    Bres(XtNallowTcapOps, XtCAllowTcapOps, screen.allowTcapOp0, DEF_ALLOW_TCAP),
    Bres(XtNallowTitleOps, XtCAllowTitleOps, screen.allowTitleOp0, DEF_ALLOW_TITLE),
    Bres(XtNallowWindowOps, XtCAllowWindowOps, screen.allowWindowOp0, DEF_ALLOW_WINDOW),
    Bres(XtNaltIsNotMeta, XtCAltIsNotMeta, screen.alt_is_not_meta, False),
    Bres(XtNaltSendsEscape, XtCAltSendsEscape, screen.alt_sends_esc, DEF_ALT_SENDS_ESC),
    Bres(XtNallowBoldFonts, XtCAllowBoldFonts, screen.allowBoldFonts, True),
    Bres(XtNalwaysBoldMode, XtCAlwaysBoldMode, screen.always_bold_mode, False),
    Bres(XtNalwaysHighlight, XtCAlwaysHighlight, screen.always_highlight, False),
    Bres(XtNappcursorDefault, XtCAppcursorDefault, misc.appcursorDefault, False),
    Bres(XtNappkeypadDefault, XtCAppkeypadDefault, misc.appkeypadDefault, False),
    Bres(XtNalternateScroll, XtCScrollCond, screen.alternateScroll, False),
    Bres(XtNautoWrap, XtCAutoWrap, misc.autoWrap, True),
    Bres(XtNawaitInput, XtCAwaitInput, screen.awaitInput, False),
    Bres(XtNfreeBoldBox, XtCFreeBoldBox, screen.free_bold_box, False),
    Bres(XtNbackarrowKey, XtCBackarrowKey, screen.backarrow_key, DEF_BACKARO_BS),
    Bres(XtNbellIsUrgent, XtCBellIsUrgent, screen.bellIsUrgent, False),
    Bres(XtNbellOnReset, XtCBellOnReset, screen.bellOnReset, True),
    Bres(XtNboldMode, XtCBoldMode, screen.bold_mode, True),
    Bres(XtNbrokenSelections, XtCBrokenSelections, screen.brokenSelections, False),
    Bres(XtNc132, XtCC132, screen.c132, False),
    Bres(XtNcdXtraScroll, XtCCdXtraScroll, misc.cdXtraScroll, False),
    Bres(XtNcurses, XtCCurses, screen.curses, False),
    Bres(XtNcutNewline, XtCCutNewline, screen.cutNewline, True),
    Bres(XtNcutToBeginningOfLine, XtCCutToBeginningOfLine,
	 screen.cutToBeginningOfLine, True),
    Bres(XtNdeleteIsDEL, XtCDeleteIsDEL, screen.delete_is_del, DEFDELETE_DEL),
    Bres(XtNdynamicColors, XtCDynamicColors, misc.dynamicColors, True),
    Bres(XtNeightBitControl, XtCEightBitControl, screen.control_eight_bits, False),
    Bres(XtNeightBitInput, XtCEightBitInput, screen.input_eight_bits, True),
    Bres(XtNeightBitOutput, XtCEightBitOutput, screen.output_eight_bits, True),
    Bres(XtNeraseSavedLines, XtCEraseSavedLines, screen.eraseSavedLines0, True),
    Bres(XtNhighlightSelection, XtCHighlightSelection,
	 screen.highlight_selection, False),
    Bres(XtNshowWrapMarks, XtCShowWrapMarks, screen.show_wrap_marks, False),
    Bres(XtNhpLowerleftBugCompat, XtCHpLowerleftBugCompat, screen.hp_ll_bc, False),
    Bres(XtNi18nSelections, XtCI18nSelections, screen.i18nSelections, True),
    Bres(XtNfastScroll, XtCFastScroll, screen.fastscroll, False),
    Bres(XtNjumpScroll, XtCJumpScroll, screen.jumpscroll, True),
    Bres(XtNkeepClipboard, XtCKeepClipboard, screen.keepClipboard, False),
    Bres(XtNkeepSelection, XtCKeepSelection, screen.keepSelection, True),
    Bres(XtNloginShell, XtCLoginShell, misc.login_shell, False),
    Bres(XtNmarginBell, XtCMarginBell, screen.marginbell, False),
    Bres(XtNmetaSendsEscape, XtCMetaSendsEscape, screen.meta_sends_esc, DEF_META_SENDS_ESC),
    Bres(XtNmultiScroll, XtCMultiScroll, screen.multiscroll, False),
    Bres(XtNoldXtermFKeys, XtCOldXtermFKeys, screen.old_fkeys, False),
    Bres(XtNpopOnBell, XtCPopOnBell, screen.poponbell, False),
    Bres(XtNprinterAutoClose, XtCPrinterAutoClose, SPS.printer_autoclose, False),
    Bres(XtNprinterExtent, XtCPrinterExtent, SPS.printer_extent, False),
    Bres(XtNprinterFormFeed, XtCPrinterFormFeed, SPS.printer_formfeed, False),
    Bres(XtNprinterNewLine, XtCPrinterNewLine, SPS.printer_newline, True),
    Bres(XtNquietGrab, XtCQuietGrab, screen.quiet_grab, False),
    Bres(XtNreverseVideo, XtCReverseVideo, misc.re_verse, False),
    Bres(XtNreverseWrap, XtCReverseWrap, misc.reverseWrap, False),
    Bres(XtNscrollBar, XtCScrollBar, misc.scrollbar, False),
    Bres(XtNscrollKey, XtCScrollCond, screen.scrollkey, False),
    Bres(XtNscrollTtyOutput, XtCScrollCond, screen.scrollttyoutput, True),
    Bres(XtNselectToClipboard, XtCSelectToClipboard,
	 screen.selectToClipboard, False),
    Bres(XtNsignalInhibit, XtCSignalInhibit, misc.signalInhibit, False),
    Bres(XtNtiteInhibit, XtCTiteInhibit, misc.titeInhibit, False),
    Bres(XtNtiXtraScroll, XtCTiXtraScroll, misc.tiXtraScroll, False),
    Bres(XtNtrimSelection, XtCTrimSelection, screen.trim_selection, False),
    Bres(XtNunderLine, XtCUnderLine, screen.underline, True),
    Bres(XtNvisualBell, XtCVisualBell, screen.visualbell, False),
    Bres(XtNvisualBellLine, XtCVisualBellLine, screen.flash_line, False),

    Dres(XtNscaleHeight, XtCScaleHeight, screen.scale_height, "1.0"),

    Ires(XtNbellSuppressTime, XtCBellSuppressTime, screen.bellSuppressTime, BELLSUPPRESSMSEC),
    Ires(XtNfontWarnings, XtCFontWarnings, misc.fontWarnings, fwResource),
    Ires(XtNinternalBorder, XtCBorderWidth, screen.border, DEFBORDER),
    Ires(XtNlimitResize, XtCLimitResize, misc.limit_resize, 1),
    Ires(XtNmultiClickTime, XtCMultiClickTime, screen.multiClickTime, MULTICLICKTIME),
    Ires(XtNnMarginBell, XtCColumn, screen.nmarginbell, N_MARGINBELL),
    Ires(XtNpointerMode, XtCPointerMode, screen.pointer_mode, DEF_POINTER_MODE),
    Ires(XtNprinterControlMode, XtCPrinterControlMode,
	 SPS.printer_controlmode, 0),
    Ires(XtNtitleModes, XtCTitleModes, screen.title_modes, DEF_TITLE_MODES),
    Ires(XtNnextEventDelay, XtCNextEventDelay, screen.nextEventDelay, 1),
    Ires(XtNvisualBellDelay, XtCVisualBellDelay, screen.visualBellDelay, 100),
    Ires(XtNsaveLines, XtCSaveLines, screen.savelines, SAVELINES),
    Ires(XtNscrollBarBorder, XtCScrollBarBorder, screen.scrollBarBorder, 1),
    Ires(XtNscrollLines, XtCScrollLines, screen.scrolllines, SCROLLLINES),

    Sres(XtNinitialFont, XtCInitialFont, screen.initial_font, NULL),
    Sres(XtNfont1, XtCFont1, screen.MenuFontName(fontMenu_font1), NULL),
    Sres(XtNfont2, XtCFont2, screen.MenuFontName(fontMenu_font2), NULL),
    Sres(XtNfont3, XtCFont3, screen.MenuFontName(fontMenu_font3), NULL),
    Sres(XtNfont4, XtCFont4, screen.MenuFontName(fontMenu_font4), NULL),
    Sres(XtNfont5, XtCFont5, screen.MenuFontName(fontMenu_font5), NULL),
    Sres(XtNfont6, XtCFont6, screen.MenuFontName(fontMenu_font6), NULL),

    Sres(XtNanswerbackString, XtCAnswerbackString, screen.answer_back, ""),
    Sres(XtNboldFont, XtCBoldFont, misc.default_font.f_b, DEFBOLDFONT),
    Sres(XtNcharClass, XtCCharClass, screen.charClass, NULL),
    Sres(XtNdecTerminalID, XtCDecTerminalID, screen.term_id, DFT_DECID),
    Sres(XtNdefaultString, XtCDefaultString, screen.default_string, "#"),
    Sres(XtNdisallowedColorOps, XtCDisallowedColorOps,
	 screen.disallowedColorOps, DEF_DISALLOWED_COLOR),
    Sres(XtNdisallowedFontOps, XtCDisallowedFontOps,
	 screen.disallowedFontOps, DEF_DISALLOWED_FONT),
    Sres(XtNdisallowedMouseOps, XtCDisallowedMouseOps,
	 screen.disallowedMouseOps, DEF_DISALLOWED_MOUSE),
    Sres(XtNdisallowedTcapOps, XtCDisallowedTcapOps,
	 screen.disallowedTcapOps, DEF_DISALLOWED_TCAP),
    Sres(XtNdisallowedWindowOps, XtCDisallowedWindowOps,
	 screen.disallowedWinOps, DEF_DISALLOWED_WINDOW),
    Sres(XtNeightBitMeta, XtCEightBitMeta, screen.eight_bit_meta_s, DEF_8BIT_META),
    Sres(XtNeightBitSelectTypes, XtCEightBitSelectTypes,
	 screen.eightbit_select_types, NULL),
    Sres(XtNfont, XtCFont, misc.default_font.f_n, DEFFONT),
    Sres(XtNgeometry, XtCGeometry, misc.geo_metry, NULL),
    Sres(XtNkeyboardDialect, XtCKeyboardDialect, screen.keyboard_dialect, DFT_KBD_DIALECT),
    Sres(XtNprinterCommand, XtCPrinterCommand, SPS.printer_command, ""),
    Sres(XtNtekGeometry, XtCGeometry, misc.T_geometry, NULL),

    Tres(XtNcursorColor, XtCCursorColor, TEXT_CURSOR, XtDefaultForeground),
    Tres(XtNforeground, XtCForeground, TEXT_FG, XtDefaultForeground),
    Tres(XtNpointerColor, XtCPointerColor, MOUSE_FG, XtDefaultForeground),
    Tres(XtNbackground, XtCBackground, TEXT_BG, XtDefaultBackground),
    Tres(XtNpointerColorBackground, XtCBackground, MOUSE_BG, XtDefaultBackground),

    {XtNresizeGravity, XtCResizeGravity, XtRGravity, sizeof(XtGravity),
     XtOffsetOf(XtermWidgetRec, misc.resizeGravity),
     XtRImmediate, (XtPointer) SouthWestGravity},

    {XtNpointerShape, XtCCursor, XtRCursor, sizeof(Cursor),
     XtOffsetOf(XtermWidgetRec, screen.pointer_cursor),
     XtRString, (XtPointer) "xterm"},

#ifdef ALLOWLOGGING
    Bres(XtNlogInhibit, XtCLogInhibit, misc.logInhibit, False),
    Bres(XtNlogging, XtCLogging, misc.log_on, False),
    Sres(XtNlogFile, XtCLogfile, screen.logfile, NULL),
#endif

#ifndef NO_ACTIVE_ICON
    Sres("activeIcon", "ActiveIcon", misc.active_icon_s, "default"),
    Ires("iconBorderWidth", XtCBorderWidth, misc.icon_border_width, 2),
    Sres("iconFont", "IconFont", screen.icon_fontname, "nil2"),
    Cres("iconBorderColor", XtCBorderColor, misc.icon_border_pixel, XtDefaultBackground),
#endif				/* NO_ACTIVE_ICON */

#if OPT_BLINK_CURS
    Bres(XtNcursorBlinkXOR, XtCCursorBlinkXOR, screen.cursor_blink_xor, True),
    Sres(XtNcursorBlink, XtCCursorBlink, screen.cursor_blink_s, "false"),
#endif
    Bres(XtNcursorUnderLine, XtCCursorUnderLine, screen.cursor_underline, False),

#if OPT_BLINK_TEXT
    Bres(XtNshowBlinkAsBold, XtCCursorBlink, screen.blink_as_bold, DEFBLINKASBOLD),
#endif

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
    Ires(XtNcursorOnTime, XtCCursorOnTime, screen.blink_on, 600),
    Ires(XtNcursorOffTime, XtCCursorOffTime, screen.blink_off, 300),
#endif

#if OPT_BOX_CHARS
    Bres(XtNforceBoxChars, XtCForceBoxChars, screen.force_box_chars, False),
    Bres(XtNforcePackedFont, XtCForcePackedFont, screen.force_packed, True),
    Bres(XtNshowMissingGlyphs, XtCShowMissingGlyphs, screen.force_all_chars, False),
    Bres(XtNassumeAllChars, XtCAssumeAllChars, screen.assume_all_chars, True),
#endif

#if OPT_BROKEN_OSC
    Bres(XtNbrokenLinuxOSC, XtCBrokenLinuxOSC, screen.brokenLinuxOSC, True),
#endif

#if OPT_BROKEN_ST
    Bres(XtNbrokenStringTerm, XtCBrokenStringTerm, screen.brokenStringTerm, False),
#endif

#if OPT_C1_PRINT
    Bres(XtNallowC1Printable, XtCAllowC1Printable, screen.c1_printable, False),
#endif

#if OPT_CLIP_BOLD
    Bres(XtNuseClipping, XtCUseClipping, screen.use_clipping, True),
#endif

#if OPT_DEC_CHRSET
    Bres(XtNfontDoublesize, XtCFontDoublesize, screen.font_doublesize, True),
    Ires(XtNcacheDoublesize, XtCCacheDoublesize, screen.cache_doublesize, NUM_CHRSET),
#endif

#if OPT_HIGHLIGHT_COLOR
    Tres(XtNhighlightColor, XtCHighlightColor, HIGHLIGHT_BG, XtDefaultForeground),
    Tres(XtNhighlightTextColor, XtCHighlightTextColor, HIGHLIGHT_FG, XtDefaultBackground),
    Bres(XtNhighlightReverse, XtCHighlightReverse, screen.hilite_reverse, True),
    Bres(XtNhighlightColorMode, XtCHighlightColorMode, screen.hilite_color, Maybe),
#endif				/* OPT_HIGHLIGHT_COLOR */

#if OPT_INPUT_METHOD
    Bres(XtNopenIm, XtCOpenIm, misc.open_im, True),
    Sres(XtNinputMethod, XtCInputMethod, misc.input_method, NULL),
    Sres(XtNpreeditType, XtCPreeditType, misc.preedit_type,
	 "OverTheSpot,Root"),
    Ires(XtNretryInputMethod, XtCRetryInputMethod, misc.retry_im, 3),
#endif

#if OPT_ISO_COLORS
    Bres(XtNboldColors, XtCColorMode, screen.boldColors, True),
    Ires(XtNveryBoldColors, XtCVeryBoldColors, screen.veryBoldColors, 0),
    Bres(XtNcolorMode, XtCColorMode, screen.colorMode, DFT_COLORMODE),

    Bres(XtNcolorAttrMode, XtCColorAttrMode, screen.colorAttrMode, False),
    Bres(XtNcolorBDMode, XtCColorAttrMode, screen.colorBDMode, False),
    Bres(XtNcolorBLMode, XtCColorAttrMode, screen.colorBLMode, False),
    Bres(XtNcolorRVMode, XtCColorAttrMode, screen.colorRVMode, False),
    Bres(XtNcolorULMode, XtCColorAttrMode, screen.colorULMode, False),
    Bres(XtNitalicULMode, XtCColorAttrMode, screen.italicULMode, False),
#if OPT_WIDE_ATTRS
    Bres(XtNcolorITMode, XtCColorAttrMode, screen.colorITMode, False),
#endif
#if OPT_DIRECT_COLOR
    Bres(XtNdirectColor, XtCDirectColor, screen.direct_color, True),
#endif

    COLOR_RES("0", screen.Acolors[COLOR_0], DFT_COLOR("black")),
    COLOR_RES("1", screen.Acolors[COLOR_1], DFT_COLOR("red3")),
    COLOR_RES("2", screen.Acolors[COLOR_2], DFT_COLOR("green3")),
    COLOR_RES("3", screen.Acolors[COLOR_3], DFT_COLOR("yellow3")),
    COLOR_RES("4", screen.Acolors[COLOR_4], DFT_COLOR(DEF_COLOR4)),
    COLOR_RES("5", screen.Acolors[COLOR_5], DFT_COLOR("magenta3")),
    COLOR_RES("6", screen.Acolors[COLOR_6], DFT_COLOR("cyan3")),
    COLOR_RES("7", screen.Acolors[COLOR_7], DFT_COLOR("gray90")),
    COLOR_RES("8", screen.Acolors[COLOR_8], DFT_COLOR("gray50")),
    COLOR_RES("9", screen.Acolors[COLOR_9], DFT_COLOR("red")),
    COLOR_RES("10", screen.Acolors[COLOR_10], DFT_COLOR("green")),
    COLOR_RES("11", screen.Acolors[COLOR_11], DFT_COLOR("yellow")),
    COLOR_RES("12", screen.Acolors[COLOR_12], DFT_COLOR(DEF_COLOR12)),
    COLOR_RES("13", screen.Acolors[COLOR_13], DFT_COLOR("magenta")),
    COLOR_RES("14", screen.Acolors[COLOR_14], DFT_COLOR("cyan")),
    COLOR_RES("15", screen.Acolors[COLOR_15], DFT_COLOR("white")),
    COLOR_RES("BD", screen.Acolors[COLOR_BD], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("BL", screen.Acolors[COLOR_BL], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("UL", screen.Acolors[COLOR_UL], DFT_COLOR(XtDefaultForeground)),
    COLOR_RES("RV", screen.Acolors[COLOR_RV], DFT_COLOR(XtDefaultForeground)),

#if OPT_WIDE_ATTRS
    COLOR_RES("IT", screen.Acolors[COLOR_IT], DFT_COLOR(XtDefaultForeground)),
#endif

#if !OPT_COLOR_RES2
#if OPT_256_COLORS
# include <256colres.h>
#elif OPT_88_COLORS
# include <88colres.h>
#endif
#endif				/* !OPT_COLOR_RES2 */

#endif				/* OPT_ISO_COLORS */

    CLICK_RES("2", screen.onClick[1], "word"),
    CLICK_RES("3", screen.onClick[2], "line"),
    CLICK_RES("4", screen.onClick[3], 0),
    CLICK_RES("5", screen.onClick[4], 0),

#if OPT_MOD_FKEYS
    Ires(XtNmodifyKeyboard, XtCModifyKeyboard,
	 keyboard.modify_1st.allow_keys, 0),
    Ires(XtNmodifyCursorKeys, XtCModifyCursorKeys,
	 keyboard.modify_1st.cursor_keys, 2),
    Ires(XtNmodifyFunctionKeys, XtCModifyFunctionKeys,
	 keyboard.modify_1st.function_keys, 2),
    Ires(XtNmodifyKeypadKeys, XtCModifyKeypadKeys,
	 keyboard.modify_1st.keypad_keys, 0),
    Ires(XtNmodifyOtherKeys, XtCModifyOtherKeys,
	 keyboard.modify_1st.other_keys, 0),
    Ires(XtNmodifyStringKeys, XtCModifyStringKeys,
	 keyboard.modify_1st.string_keys, 0),
    Ires(XtNformatOtherKeys, XtCFormatOtherKeys,
	 keyboard.format_keys, 0),
#endif

#if OPT_NUM_LOCK
    Bres(XtNalwaysUseMods, XtCAlwaysUseMods, misc.alwaysUseMods, False),
    Bres(XtNnumLock, XtCNumLock, misc.real_NumLock, True),
#endif

#if OPT_PRINT_COLORS
    Ires(XtNprintAttributes, XtCPrintAttributes, SPS.print_attributes, 1),
#endif

#if OPT_REGIS_GRAPHICS
    Sres(XtNregisDefaultFont, XtCRegisDefaultFont,
	 screen.graphics_regis_default_font, ""),
    Sres(XtNregisScreenSize, XtCRegisScreenSize,
	 screen.graphics_regis_screensize, "auto"),
#endif

#if OPT_GRAPHICS
    Sres(XtNmaxGraphicSize, XtCMaxGraphicSize, screen.graphics_max_size,
	 "1000x1000"),
#endif

#if OPT_SHIFT_FONTS
    Bres(XtNshiftFonts, XtCShiftFonts, misc.shift_fonts, True),
#endif

#if OPT_SIXEL_GRAPHICS
    Bres(XtNsixelScrolling, XtCSixelScrolling, screen.sixel_scrolling, True),
    Bres(XtNsixelScrollsRight, XtCSixelScrollsRight,
	 screen.sixel_scrolls_right, False),
#endif

#if OPT_GRAPHICS
    Ires(XtNnumColorRegisters, XtCNumColorRegisters,
	 screen.numcolorregisters, 0),
    Bres(XtNprivateColorRegisters, XtCPrivateColorRegisters,
	 screen.privatecolorregisters, True),
#endif

#if OPT_SUNPC_KBD
    Ires(XtNctrlFKeys, XtCCtrlFKeys, misc.ctrl_fkeys, 10),
#endif

#if OPT_TEK4014
    Bres(XtNtekInhibit, XtCTekInhibit, misc.tekInhibit, False),
    Bres(XtNtekSmall, XtCTekSmall, misc.tekSmall, False),
    Bres(XtNtekStartup, XtCTekStartup, misc.TekEmu, False),
#endif

#if OPT_TOOLBAR
    Wres(XtNmenuBar, XtCMenuBar, VT100_TB_INFO(menu_bar), 0),
    Ires(XtNmenuHeight, XtCMenuHeight, VT100_TB_INFO(menu_height), 25),
#endif

#if OPT_WIDE_CHARS
    Bres(XtNcjkWidth, XtCCjkWidth, misc.cjk_width, False),
    Bres(XtNmkWidth, XtCMkWidth, misc.mk_width, False),
    Bres(XtNprecompose, XtCPrecompose, screen.normalized_c, True),
    Bres(XtNutf8Latin1, XtCUtf8Latin1, screen.utf8_latin1, False),
    Bres(XtNutf8Title, XtCUtf8Title, screen.utf8_title, False),
    Bres(XtNvt100Graphics, XtCVT100Graphics, screen.vt100_graphics, True),
    Bres(XtNwideChars, XtCWideChars, screen.wide_chars, False),
    Ires(XtNcombiningChars, XtCCombiningChars, screen.max_combining, 2),
    Ires(XtNmkSamplePass, XtCMkSamplePass, misc.mk_samplepass, 655),
    Ires(XtNmkSampleSize, XtCMkSampleSize, misc.mk_samplesize, 65536),
    Sres(XtNutf8, XtCUtf8, screen.utf8_mode_s, "default"),
    Sres(XtNutf8Fonts, XtCUtf8Fonts, screen.utf8_fonts_s, "default"),
    Sres(XtNwideBoldFont, XtCWideBoldFont, misc.default_font.f_wb, DEFWIDEBOLDFONT),
    Sres(XtNwideFont, XtCWideFont, misc.default_font.f_w, DEFWIDEFONT),
    Sres(XtNutf8SelectTypes, XtCUtf8SelectTypes, screen.utf8_select_types, NULL),
#endif

#if OPT_LUIT_PROG
    Sres(XtNlocale, XtCLocale, misc.locale_str, "medium"),
    Sres(XtNlocaleFilter, XtCLocaleFilter, misc.localefilter, DEFLOCALEFILTER),
#endif

#if OPT_INPUT_METHOD
    Sres(XtNximFont, XtCXimFont, misc.f_x, DEFXIMFONT),
#endif

#if OPT_SCROLL_LOCK
    Bres(XtNallowScrollLock, XtCAllowScrollLock, screen.allowScrollLock0, False),
#endif

    /* these are used only for testing ncurses, not in the manual page */
#if OPT_XMC_GLITCH
    Bres(XtNxmcInline, XtCXmcInline, screen.xmc_inline, False),
    Bres(XtNxmcMoveSGR, XtCXmcMoveSGR, screen.move_sgr_ok, True),
    Ires(XtNxmcAttributes, XtCXmcAttributes, screen.xmc_attributes, 1),
    Ires(XtNxmcGlitch, XtCXmcGlitch, screen.xmc_glitch, 0),
#endif

#ifdef SCROLLBAR_RIGHT
    Bres(XtNrightScrollBar, XtCRightScrollBar, misc.useRight, False),
#endif

#if OPT_RENDERFONT
#define RES_FACESIZE(n) Dres(XtNfaceSize #n, XtCFaceSize #n, misc.face_size[n], "0.0")
    RES_FACESIZE(1),
    RES_FACESIZE(2),
    RES_FACESIZE(3),
    RES_FACESIZE(4),
    RES_FACESIZE(5),
    RES_FACESIZE(6),
    Dres(XtNfaceSize, XtCFaceSize, misc.face_size[0], DEFFACESIZE),
    Sres(XtNfaceName, XtCFaceName, misc.default_xft.f_n, DEFFACENAME),
    Sres(XtNrenderFont, XtCRenderFont, misc.render_font_s, "default"),
#if OPT_RENDERWIDE
    Sres(XtNfaceNameDoublesize, XtCFaceNameDoublesize, misc.default_xft.f_w, DEFFACENAME),
#endif
#endif
};

static Boolean VTSetValues(Widget cur, Widget request, Widget new_arg,
			   ArgList args, Cardinal *num_args);
static void VTClassInit(void);
static void VTDestroy(Widget w);
static void VTExpose(Widget w, XEvent *event, Region region);
static void VTInitialize(Widget wrequest, Widget new_arg, ArgList args,
			 Cardinal *num_args);
static void VTRealize(Widget w, XtValueMask * valuemask,
		      XSetWindowAttributes * values);
static void VTResize(Widget w);

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
static void VTInitI18N(XtermWidget);
#endif

#ifdef VMS
globaldef {
    "xtermclassrec"
} noshare

#else
static
#endif				/* VMS */
WidgetClassRec xtermClassRec =
{
    {
	/* core_class fields */
	(WidgetClass) & widgetClassRec,		/* superclass   */
	"VT100",		/* class_name                   */
	sizeof(XtermWidgetRec),	/* widget_size                  */
	VTClassInit,		/* class_initialize             */
	NULL,			/* class_part_initialize        */
	False,			/* class_inited                 */
	VTInitialize,		/* initialize                   */
	NULL,			/* initialize_hook              */
	VTRealize,		/* realize                      */
	actionsList,		/* actions                      */
	XtNumber(actionsList),	/* num_actions                  */
	xterm_resources,	/* resources                    */
	XtNumber(xterm_resources),	/* num_resources        */
	NULLQUARK,		/* xrm_class                    */
	True,			/* compress_motion              */
	False,			/* compress_exposure            */
	True,			/* compress_enterleave          */
	False,			/* visible_interest             */
	VTDestroy,		/* destroy                      */
	VTResize,		/* resize                       */
	VTExpose,		/* expose                       */
	VTSetValues,		/* set_values                   */
	NULL,			/* set_values_hook              */
	XtInheritSetValuesAlmost,	/* set_values_almost    */
	NULL,			/* get_values_hook              */
	NULL,			/* accept_focus                 */
	XtVersion,		/* version                      */
	NULL,			/* callback_offsets             */
	0,			/* tm_table                     */
	XtInheritQueryGeometry,	/* query_geometry               */
	XtInheritDisplayAccelerator,	/* display_accelerator  */
	NULL			/* extension                    */
    }
};

#ifdef VMS
globaldef {
    "xtermwidgetclass"
}
noshare
#endif /* VMS */
WidgetClass xtermWidgetClass = (WidgetClass) & xtermClassRec;

/*
 * Add input-actions for widgets that are overlooked (scrollbar and toolbar):
 *
 *	a) Sometimes the scrollbar passes through translations, sometimes it
 *	   doesn't.  We add the KeyPress translations here, just to be sure.
 *	b) In the normal (non-toolbar) configuration, the xterm widget covers
 *	   almost all of the window.  With a toolbar, there's a relatively
 *	   large area that the user would expect to enter keystrokes since the
 *	   program can get the focus.
 */
void
xtermAddInput(Widget w)
{
    /* *INDENT-OFF* */
    XtActionsRec input_actions[] = {
	{ "insert",		    HandleKeyPressed }, /* alias */
	{ "insert-eight-bit",	    HandleEightBitKeyPressed },
	{ "insert-seven-bit",	    HandleKeyPressed },
	{ "secure",		    HandleSecure },
	{ "string",		    HandleStringEvent },
	{ "scroll-back",	    HandleScrollBack },
	{ "scroll-forw",	    HandleScrollForward },
	{ "scroll-to",		    HandleScrollTo },
	{ "select-cursor-end",	    HandleKeyboardSelectEnd },
	{ "select-cursor-extend",   HandleKeyboardSelectExtend },
	{ "select-cursor-start",    HandleKeyboardSelectStart },
	{ "insert-selection",	    HandleInsertSelection },
	{ "select-start",	    HandleSelectStart },
	{ "select-extend",	    HandleSelectExtend },
	{ "start-extend",	    HandleStartExtend },
	{ "select-end",		    HandleSelectEnd },
	{ "clear-saved-lines",	    HandleClearSavedLines },
	{ "popup-menu",		    HandlePopupMenu },
	{ "bell",		    HandleBell },
	{ "ignore",		    HandleIgnore },
#if OPT_DABBREV
	{ "dabbrev-expand",	    HandleDabbrevExpand },
#endif
#if OPT_MAXIMIZE
	{ "fullscreen",		    HandleFullscreen },
#endif
#if OPT_SCROLL_LOCK
	{ "scroll-lock",	    HandleScrollLock },
#endif
#if OPT_SHIFT_FONTS
	{ "larger-vt-font",	    HandleLargerFont },
	{ "smaller-vt-font",	    HandleSmallerFont },
#endif
    };
    /* *INDENT-ON* */

    TRACE_TRANS("BEFORE", w);
    XtAppAddActions(app_con, input_actions, XtNumber(input_actions));
    XtAugmentTranslations(w, XtParseTranslationTable(defaultTranslations));
    TRACE_TRANS("AFTER:", w);

#if OPT_EXTRA_PASTE
    if (term && term->keyboard.extra_translations)
	XtOverrideTranslations((Widget) term, XtParseTranslationTable(term->keyboard.extra_translations));
#endif
}

#if OPT_ISO_COLORS
#ifdef EXP_BOGUS_FG
static Bool
CheckBogusForeground(TScreen *screen, const char *tag)
{
    int row = -1, col = -1, pass;
    Bool isClear = True;

    (void) tag;
    for (pass = 0; pass < 2; ++pass) {
	row = screen->cur_row;
	for (; isClear && (row <= screen->max_row); ++row) {
	    CLineData *ld = getLineData(screen, row);

	    if (ld != 0) {
		IAttr *attribs = ld->attribs;

		col = (row == screen->cur_row) ? screen->cur_col : 0;
		for (; isClear && (col <= screen->max_col); ++col) {
		    unsigned flags = attribs[col];
		    if (pass) {
			flags &= ~FG_COLOR;
			attribs[col] = (IAttr) flags;
		    } else if ((flags & BG_COLOR)) {
			isClear = False;
		    } else if ((flags & FG_COLOR)) {
			unsigned ch = ld->charData[col];
			isClear = ((ch == ' ') || (ch == 0));
		    } else {
			isClear = False;
		    }
		}
	    }
	}
    }
    TRACE(("%s checked %d,%d to %d,%d %s pass %d\n",
	   tag, screen->cur_row, screen->cur_col,
	   row, col,
	   isClear && pass ? "cleared" : "unchanged",
	   pass));

    return isClear;
}
#endif

/*
 * The terminal's foreground and background colors are set via two mechanisms:
 *	text (cur_foreground, cur_background values that are passed down to
 *		XDrawImageString and XDrawString)
 *	area (X11 graphics context used in XClearArea and XFillRectangle)
 */
void
SGR_Foreground(XtermWidget xw, int color)
{
    TScreen *screen = TScreenOf(xw);
    Pixel fg;

    if (color >= 0) {
	UIntSet(xw->flags, FG_COLOR);
    } else {
	UIntClr(xw->flags, FG_COLOR);
    }
    fg = getXtermFG(xw, xw->flags, color);
    xw->cur_foreground = color;

    setCgsFore(xw, WhichVWin(screen), gcNorm, fg);
    setCgsBack(xw, WhichVWin(screen), gcNormReverse, fg);

    setCgsFore(xw, WhichVWin(screen), gcBold, fg);
    setCgsBack(xw, WhichVWin(screen), gcBoldReverse, fg);

#ifdef EXP_BOGUS_FG
    /*
     * If we've just turned off the foreground color, check for blank cells
     * which have no background color, but do have foreground color.  This
     * could happen due to setting the foreground color just before scrolling.
     *
     * Those cells look uncolored, but will confuse ShowCursor(), which looks
     * for the colors in the current cell, and will see the foreground color.
     * In that case, remove the foreground color from the blank cells.
     */
    if (color < 0) {
	CheckBogusForeground(screen, "SGR_Foreground");
    }
#endif
}

void
SGR_Background(XtermWidget xw, int color)
{
    TScreen *screen = TScreenOf(xw);
    Pixel bg;

    /*
     * An indexing operation may have set screen->scroll_amt, which would
     * normally result in calling FlushScroll() in WriteText().  However,
     * if we're changing the background color now, then the new value
     * should not apply to the pending blank lines.
     */
    if (screen->scroll_amt && (color != xw->cur_background))
	FlushScroll(xw);

    if (color >= 0) {
	UIntSet(xw->flags, BG_COLOR);
    } else {
	UIntClr(xw->flags, BG_COLOR);
    }
    bg = getXtermBG(xw, xw->flags, color);
    xw->cur_background = color;

    setCgsBack(xw, WhichVWin(screen), gcNorm, bg);
    setCgsFore(xw, WhichVWin(screen), gcNormReverse, bg);

    setCgsBack(xw, WhichVWin(screen), gcBold, bg);
    setCgsFore(xw, WhichVWin(screen), gcBoldReverse, bg);
}

/* Invoked after updating bold/underline flags, computes the extended color
 * index to use for foreground.  (See also 'extract_fg()').
 */
static void
setExtendedFG(XtermWidget xw)
{
    int fg = xw->sgr_foreground;

    if (TScreenOf(xw)->colorAttrMode
	|| (fg < 0)) {
	fg = MapToColorMode(fg, TScreenOf(xw), xw->flags);
    }

    /* This implements the IBM PC-style convention of 8-colors, with one
     * bit for bold, thus mapping the 0-7 codes to 8-15.  It won't make
     * much sense for 16-color applications, but we keep it to retain
     * compatiblity with ANSI-color applications.
     */
#if OPT_PC_COLORS		/* XXXJTL should be settable at runtime (resource or OSC?) */
    if (TScreenOf(xw)->boldColors
	&& (!hasDirectFG(xw->flags))
	&& (fg >= 0)
	&& (fg < 8)
	&& (xw->flags & BOLD))
	fg |= 8;
#endif

    SGR_Foreground(xw, fg);
}

/* Invoked after updating inverse flag, computes the extended color
 * index to use for background.  (See also 'extract_bg()').
 */
static void
setExtendedBG(XtermWidget xw)
{
    int bg = xw->sgr_background;

    if (TScreenOf(xw)->colorAttrMode
	|| (bg < 0)) {
	if (TScreenOf(xw)->colorRVMode && (xw->flags & INVERSE))
	    bg = COLOR_RV;
    }

    SGR_Background(xw, bg);
}

static void
reset_SGR_Foreground(XtermWidget xw)
{
    xw->sgr_foreground = -1;
    clrDirectFG(xw->flags);
    setExtendedFG(xw);
}

static void
reset_SGR_Background(XtermWidget xw)
{
    xw->sgr_background = -1;
    clrDirectBG(xw->flags);
    setExtendedBG(xw);
}

static void
reset_SGR_Colors(XtermWidget xw)
{
    reset_SGR_Foreground(xw);
    reset_SGR_Background(xw);
}
#endif /* OPT_ISO_COLORS */

#if OPT_WIDE_ATTRS
/*
 * Call this before changing the state of ATR_ITALIC, to update the GC fonts.
 */
static void
setItalicFont(XtermWidget xw, Bool enable)
{
    if (enable) {
	if ((xw->flags & ATR_ITALIC) == 0) {
	    xtermLoadItalics(xw);
	    TRACE(("setItalicFont: enabling Italics\n"));
	    xtermUpdateFontGCs(xw, True);
	}
    } else if ((xw->flags & ATR_ITALIC) != 0) {
	TRACE(("setItalicFont: disabling Italics\n"));
	xtermUpdateFontGCs(xw, False);
    }
}
#endif

void
resetCharsets(TScreen *screen)
{
    TRACE(("resetCharsets\n"));

    screen->gsets[0] = nrc_ASCII;
    screen->gsets[1] = nrc_ASCII;
    screen->gsets[2] = nrc_ASCII;
    screen->gsets[3] = nrc_ASCII;

    screen->curgl = 0;		/* G0 => GL.            */
    screen->curgr = 2;		/* G2 => GR.            */
    screen->curss = 0;		/* No single shift.     */

#if OPT_VT52_MODE
    if (screen->vtXX_level == 0)
	screen->gsets[1] = nrc_DEC_Spec_Graphic;	/* Graphics */
#endif
}

static void
modified_DECNRCM(XtermWidget xw)
{
#if OPT_WIDE_CHARS
    TScreen *screen = TScreenOf(xw);
    if (screen->wide_chars && (screen->utf8_mode || screen->utf8_nrc_mode)) {
	int enabled = ((xw->flags & NATIONAL) != 0);
	int modefix;
	EXCHANGE(screen->utf8_nrc_mode, screen->utf8_mode, modefix);
	switchPtyData(screen, !enabled);
	TRACE(("UTF8 mode temporarily %s\n", enabled ? "ON" : "OFF"));
    }
#endif
}

/*
 * VT300 and up support three ANSI conformance levels, defined according to
 * the dpANSI X3.134.1 standard.  DEC's manuals equate levels 1 and 2, and
 * are unclear.  This code is written based on the manuals.
 */
static void
set_ansi_conformance(TScreen *screen, int level)
{
    TRACE(("set_ansi_conformance(%d) dec_level %d:%d, ansi_level %d\n",
	   level,
	   screen->vtXX_level * 100,
	   screen->terminal_id,
	   screen->ansi_level));
    if (screen->vtXX_level >= 3) {
	switch (screen->ansi_level = level) {
	case 1:
	    /* FALLTHRU */
	case 2:
	    screen->gsets[0] = nrc_ASCII;	/* G0 is ASCII */
	    screen->gsets[1] = nrc_ASCII;	/* G1 is ISO Latin-1 */
	    screen->curgl = 0;
	    screen->curgr = 1;
	    break;
	case 3:
	    screen->gsets[0] = nrc_ASCII;	/* G0 is ASCII */
	    screen->curgl = 0;
	    break;
	}
    }
}

/*
 * Set scrolling margins.  VTxxx terminals require that the top/bottom are
 * different, so we have at least two lines in the scrolling region.
 */
void
set_tb_margins(TScreen *screen, int top, int bottom)
{
    TRACE(("set_tb_margins %d..%d, prior %d..%d\n",
	   top, bottom,
	   screen->top_marg,
	   screen->bot_marg));
    if (bottom > top) {
	screen->top_marg = top;
	screen->bot_marg = bottom;
    }
    if (screen->top_marg > screen->max_row)
	screen->top_marg = screen->max_row;
    if (screen->bot_marg > screen->max_row)
	screen->bot_marg = screen->max_row;
}

void
set_lr_margins(TScreen *screen, int left, int right)
{
    TRACE(("set_lr_margins %d..%d, prior %d..%d\n",
	   left, right,
	   screen->lft_marg,
	   screen->rgt_marg));
    if (right > left) {
	screen->lft_marg = left;
	screen->rgt_marg = right;
    }
    if (screen->lft_marg > screen->max_col)
	screen->lft_marg = screen->max_col;
    if (screen->rgt_marg > screen->max_col)
	screen->rgt_marg = screen->max_col;
}

#define reset_tb_margins(screen) set_tb_margins(screen, 0, screen->max_row)
#define reset_lr_margins(screen) set_lr_margins(screen, 0, screen->max_col)

static void
reset_margins(TScreen *screen)
{
    reset_tb_margins(screen);
    reset_lr_margins(screen);
}

void
set_max_col(TScreen *screen, int cols)
{
    TRACE(("set_max_col %d, prior %d\n", cols, screen->max_col));
    if (cols < 0)
	cols = 0;
    screen->max_col = cols;
}

void
set_max_row(TScreen *screen, int rows)
{
    TRACE(("set_max_row %d, prior %d\n", rows, screen->max_row));
    if (rows < 0)
	rows = 0;
    screen->max_row = rows;
}

#if OPT_MOD_FKEYS
static void
set_mod_fkeys(XtermWidget xw, int which, int what, Bool enabled)
{
#define SET_MOD_FKEYS(field) \
    xw->keyboard.modify_now.field = ((what == DEFAULT) && enabled) \
				     ? xw->keyboard.modify_1st.field \
				     : what; \
    TRACE(("set modify_now.%s to %d\n", #field, \
	   xw->keyboard.modify_now.field));

    switch (which) {
    case 0:
	SET_MOD_FKEYS(allow_keys);
	break;
    case 1:
	SET_MOD_FKEYS(cursor_keys);
	break;
    case 2:
	SET_MOD_FKEYS(function_keys);
	break;
    case 3:
	SET_MOD_FKEYS(keypad_keys);
	break;
    case 4:
	SET_MOD_FKEYS(other_keys);
	break;
    case 5:
	SET_MOD_FKEYS(string_keys);
	break;
    }
}
#endif /* OPT_MOD_FKEYS */

#if OPT_TRACE
#define DATA(name) { name, #name }
static const struct {
    Const PARSE_T *table;
    const char *name;
} all_tables[] = {

    DATA(ansi_table)
	,DATA(cigtable)
	,DATA(csi2_table)
	,DATA(csi_ex_table)
	,DATA(csi_quo_table)
	,DATA(csi_table)
	,DATA(dec2_table)
	,DATA(dec3_table)
	,DATA(dec_table)
	,DATA(eigtable)
	,DATA(esc_sp_table)
	,DATA(esc_table)
	,DATA(scrtable)
	,DATA(scs96table)
	,DATA(scstable)
	,DATA(sos_table)
#if OPT_BLINK_CURS
	,DATA(csi_sp_table)
#endif
#if OPT_DEC_LOCATOR
	,DATA(csi_tick_table)
#endif
#if OPT_DEC_RECTOPS
	,DATA(csi_dollar_table)
	,DATA(csi_star_table)
	,DATA(csi_dec_dollar_table)
#endif
#if OPT_WIDE_CHARS
	,DATA(esc_pct_table)
	,DATA(scs_pct_table)
#endif
#if OPT_VT52_MODE
	,DATA(vt52_table)
	,DATA(vt52_esc_table)
	,DATA(vt52_ignore_table)
#endif
#undef DATA
};

#define WHICH_TABLE(name) if (table == name) result = #name
static const char *
which_table(Const PARSE_T * table)
{
    const char *result = "?";
    Cardinal n;
    for (n = 0; n < XtNumber(all_tables); ++n) {
	if (table == all_tables[n].table) {
	    result = all_tables[n].name;
	    break;
	}
    }

    return result;
}

static void
check_tables(void)
{
    Cardinal n;
    int ch;

    TRACE(("** check_tables\n"));
    for (n = 0; n < XtNumber(all_tables); ++n) {
	Const PARSE_T *table = all_tables[n].table;
	TRACE(("*** %s\n", all_tables[n].name));
	/*
	 * Most of the tables should use the same codes in 0..31, 128..159
	 * as the "ansi" table.
	 */
	if (strncmp(all_tables[n].name, "ansi", 4) &&
	    strncmp(all_tables[n].name, "sos_", 4) &&
	    strncmp(all_tables[n].name, "vt52", 4)) {
	    for (ch = 0; ch < 32; ++ch) {
		int c1 = ch + 128;
		PARSE_T st_l = table[ch];
		PARSE_T st_r = table[c1];
		if (st_l != ansi_table[ch]) {
		    TRACE(("  %3d: %d vs %d\n", ch, st_l, ansi_table[ch]));
		}
		if (st_r != ansi_table[c1]) {
		    TRACE(("  %3d: %d vs %d\n", c1, st_r, ansi_table[c1]));
		}
	    }
	}
	/*
	 * All of the tables should have their GL/GR parts encoded the same.
	 */
	for (ch = 32; ch < 127; ++ch) {
	    PARSE_T st_l = table[ch];
	    PARSE_T st_r = table[ch + 128];
	    if (st_l != st_r) {
		if (st_r == CASE_IGNORE &&
		    !strncmp(all_tables[n].name, "vt52", 4)) {
		    ;
		} else {
		    TRACE(("  %3d: %d vs %d\n", ch, st_l, st_r));
		}
	    }
	}
    }
}
#endif

#if OPT_TRACE > 0
static void
dump_params(void)
{
    int n;
    int arg;
    TRACE(("params %d (%d)\n", nparam, parms.has_subparams));
    for (arg = 1, n = 0; n < nparam; ++n) {
	TRACE(("%3d.%d %d\n", arg, parms.is_sub[n], parms.params[n]));
	if (!parms.is_sub[n])
	    ++arg;
    }
}
#define DumpParams() dump_params()
#else
#define DumpParams()		/* nothing */
#endif

	/* allocate larger buffer if needed/possible */
#define SafeAlloc(type, area, used, size) \
		type *new_string = area; \
		size_t new_length = size; \
		if (new_length == 0) { \
		    new_length = 256; \
		    new_string = TypeMallocN(type, new_length); \
		} else if (used+1 >= new_length) { \
		    new_length = size * 2; \
		    new_string = TypeMallocN(type, new_length); \
		    if (new_string != 0 \
		     && area != 0 \
		     && used != 0) { \
			memcpy(new_string, area, used * sizeof(type)); \
		     } \
		}
#define SafeFree(area, size) \
		if (area != new_string) { \
		    free(area); \
		    area = new_string; \
		} \
		size = new_length

#define WriteNow() {						\
	    unsigned single = 0;				\
								\
	    if (screen->curss) {				\
		dotext(xw,					\
		       screen->gsets[(int) (screen->curss)],	\
		       sp->print_area,				\
		       (Cardinal) 1);				\
		screen->curss = 0;				\
		single++;					\
	    }							\
	    if (sp->print_used > single) {			\
		dotext(xw,					\
		       screen->gsets[(int) (screen->curgl)],	\
		       sp->print_area + single,			\
		       (Cardinal) (sp->print_used - single));	\
	    }							\
	    sp->print_used = 0;					\
	}							\

struct ParseState {
#if OPT_VT52_MODE
    Bool vt52_cup;
#endif
    Const PARSE_T *groundtable;
    Const PARSE_T *parsestate;
    int scstype;
    int scssize;
    Bool private_function;	/* distinguish private-mode from standard */
    int string_mode;		/* nonzero iff we're processing a string */
    int lastchar;		/* positive iff we had a graphic character */
    int nextstate;
#if OPT_WIDE_CHARS
    int last_was_wide;
#endif
    /* Buffer for processing printable text */
    IChar *print_area;
    size_t print_size;
    size_t print_used;
    /* Buffer for processing strings (e.g., OSC ... ST) */
    Char *string_area;
    size_t string_size;
    size_t string_used;
};

static struct ParseState myState;

static void
init_groundtable(TScreen *screen, struct ParseState *sp)
{
    (void) screen;

#if OPT_VT52_MODE
    if (!(screen->vtXX_level)) {
	sp->groundtable = vt52_table;
    } else if (screen->terminal_id >= 100)
#endif
    {
	sp->groundtable = ansi_table;
    }
}

static void
select_charset(struct ParseState *sp, int type, int size)
{
    TRACE(("select_charset %d %d\n", type, size));
    sp->scstype = type;
    sp->scssize = size;
    if (size == 94) {
	sp->parsestate = scstable;
    } else {
	sp->parsestate = scs96table;
    }
}

static void
decode_scs(XtermWidget xw, int which, int prefix, int suffix)
{
    /* *INDENT-OFF* */
    static struct {
	DECNRCM_codes result;
	int prefix;
	int suffix;
	int min_level;
	int max_level;
	int need_nrc;
    } table[] = {
	{ nrc_ASCII,             0,   'B', 1, 9, 0 },
	{ nrc_British,           0,   'A', 1, 9, 0 },
	{ nrc_DEC_Spec_Graphic,  0,   '0', 1, 9, 0 },
	{ nrc_DEC_Alt_Chars,     0,   '1', 1, 1, 0 },
	{ nrc_DEC_Alt_Graphics,  0,   '2', 1, 1, 0 },
	/* VT2xx */
	{ nrc_DEC_Supp,          0,   '<', 2, 9, 0 },
	{ nrc_Dutch,             0,   '4', 2, 9, 1 },
	{ nrc_Finnish,           0,   '5', 2, 9, 1 },
	{ nrc_Finnish2,          0,   'C', 2, 9, 1 },
	{ nrc_French,            0,   'R', 2, 9, 1 },
	{ nrc_French2,           0,   'f', 2, 9, 1 },
	{ nrc_French_Canadian,   0,   'Q', 2, 9, 1 },
	{ nrc_German,            0,   'K', 2, 9, 1 },
	{ nrc_Italian,           0,   'Y', 2, 9, 1 },
	{ nrc_Norwegian_Danish2, 0,   'E', 2, 9, 1 },
	{ nrc_Norwegian_Danish3, 0,   '6', 2, 9, 1 },
	{ nrc_Spanish,           0,   'Z', 2, 9, 1 },
	{ nrc_Swedish,           0,   '7', 2, 9, 1 },
	{ nrc_Swedish2,          0,   'H', 2, 9, 1 },
	{ nrc_Swiss,             0,   '=', 2, 9, 1 },
	/* VT3xx */
	{ nrc_British_Latin_1,   0,   'A', 3, 9, 1 },
	{ nrc_DEC_Supp_Graphic,  '%', '5', 3, 9, 0 },
	{ nrc_DEC_Technical,     0,   '>', 3, 9, 0 },
	{ nrc_French_Canadian2,  0,   '9', 3, 9, 1 },
	{ nrc_Norwegian_Danish,  0,   '`', 3, 9, 1 },
	{ nrc_Portugese,         '%', '6', 3, 9, 1 },
#if 0
	/* VT5xx (not implemented) */
	{ nrc_Cyrillic,          '&', '4', 5, 9, 0 },
	{ nrc_Greek,             '"', '?', 5, 9, 0 },
	{ nrc_Greek_Supp,        0,   'F', 5, 9, 0 },
	{ nrc_Hebrew,            '"', '4', 5, 9, 0 },
	{ nrc_Hebrew2,           '%', '=', 5, 9, 1 },
	{ nrc_Hebrew_Supp,       0,   'H', 5, 9, 0 },
	{ nrc_Latin_5_Supp,      0,   'M', 5, 9, 0 },
	{ nrc_Latin_Cyrillic,    0,   'L', 5, 9, 0 },
	{ nrc_Russian,           '&', '5', 5, 9, 1 },
	{ nrc_SCS_NRCS,          '%', '3', 5, 9, 0 },
	{ nrc_Turkish,           '%', '0', 5, 9, 0 },
	{ nrc_Turkish2,		 '%', '2', 5, 9, 1 },
#endif
    };
    /* *INDENT-ON* */

    TScreen *screen = TScreenOf(xw);
    Cardinal n;
    DECNRCM_codes result = nrc_Unknown;

    suffix &= 0x7f;
    for (n = 0; n < XtNumber(table); ++n) {
	if (prefix == table[n].prefix
	    && suffix == table[n].suffix
	    && screen->vtXX_level >= table[n].min_level
	    && screen->vtXX_level <= table[n].max_level
	    && (table[n].need_nrc == 0 || (xw->flags & NATIONAL) != 0)) {
	    result = table[n].result;
	    break;
	}
    }
    if (result != nrc_Unknown) {
	screen->gsets[which] = result;
	TRACE(("setting G%d to %s\n", which, visibleScsCode((int) result)));
    } else {
	TRACE(("...unknown GSET\n"));
    }
}

/*
 * Given a parameter number, and subparameter (starting in each case from zero)
 * return the corresponding index into the parameter array.  If the combination
 * is not found, return -1.
 */
static int
subparam_index(int p, int s)
{
    int result = -1;
    int j, p2, s2;

    for (j = p2 = 0; j < nparam; ++j, ++p2) {
	if (parms.is_sub[j]) {
	    s2 = 0;

	    do {
		if ((p == p2) && (s == s2)) {
		    result = j;
		    break;
		}
		++s2;
	    } while ((++j < nparam) && (parms.is_sub[j - 1] < parms.is_sub[j]));

	    if (result >= 0)
		break;

	    --j;		/* undo the last "while" */
	} else if (p == p2) {
	    if (s == 0) {
		result = j;
	    }
	    break;
	}
    }
    TRACE2(("...subparam_index %d.%d = %d\n", p + 1, s + 1, result));
    return result;
}

/*
 * Check if the given item in the parameter array has subparameters.
 * If so, return the number of subparameters to use as a loop limit, etc.
 */
static int
param_has_subparams(int item)
{
    int result = 0;
    if (parms.has_subparams) {
	int n = subparam_index(item, 0);
	if (n >= 0 && parms.is_sub[n]) {
	    while (++n < nparam && parms.is_sub[n - 1] < parms.is_sub[n]) {
		result++;
	    }
	}
    }
    TRACE(("...param_has_subparams(%d) ->%d\n", item, result));
    return result;
}

#if OPT_DIRECT_COLOR || OPT_256_COLORS || OPT_88_COLORS || OPT_ISO_COLORS
/*
 * Given an index into the parameter array, return the corresponding parameter
 * number (starting from zero).
 */
static int
param_number(int item)
{
    int result = -1;
    int j, p;

    for (j = p = 0; j < nparam; ++j, ++p) {
	if (p >= item) {
	    result = j;
	    break;
	}
	if (parms.is_sub[j]) {
	    while ((++j < nparam) && (parms.is_sub[j - 1] < parms.is_sub[j])) {
		/* EMPTY */
	    }
	    --j;
	}
    }

    TRACE2(("...param_number(%d) = %d\n", item, result));
    return result;
}

static int
get_subparam(int p, int s)
{
    int item = subparam_index(p, s);
    int result = (item >= 0) ? parms.params[item] : DEFAULT;
    TRACE(("...get_subparam[%d] = %d\n", item, result));
    return result;
}

/*
 * Some background -
 *
 * Todd Larason provided the initial changes to support 256-colors in July 1999.
 * I pointed out that the description of SGR 38/48 in ECMA-48 was vague, and
 * was unsure if there would be some standard using those codes.  His response
 * was that this was documented (it turns out, in equally vague terms) in ITU
 * T.416
 *
 * Discussing this with Todd Larason in mid-1999, my point was that given the
 * high cost of obtaining ITU T.416 (ISO-8613-6), the standard was not going
 * to be effective (more than $100 then, more than $200 in 2012)
 *
 * We overlooked the detail about ":" as a subparameter delimiter (documented
 * in 5.4.t2 in ECMA-48).  Some discussion in KDE in mid-2006 led Lars Doelle
 * to discuss the issue with me.  Lars' initial concern dealt with the fact
 * that a sequence such as
 *	CSI 38 ; 5 ; 1 m
 * violated the principle that SGR parameters could be used in any order.
 * Further discussion (see KDE #107487) resolved that the standard expected
 * that the sequence would look like
 *	CSI 38 ; 5 : 1 m
 * which still violates that principle, since the "5:1" parameter has to
 * follow the "38" to be useful.
 *
 * This function accepts either format (per request by Paul Leonerd Evans).
 * It also accepts
 *	CSI 38 : 5 : 1 m
 * according to Lars' original assumption.  While implementing that, I added
 * support for Konsole's interpretation of "CSI 38 : 2" as a 24-bit RGB value.
 * ISO-8613-6 documents that as "direct color".
 *
 * At the time in 2012, no one noticed (or commented) regarding ISO-8613-6's
 * quirk in the description of direct color:  it mentions a color space
 * identifier parameter which should follow the "2" (as parameter 1).  In the
 * same section, ISO-8613-6 mentions a parameter 6 which can be ignored, as
 * well as parameters 7 and 8.  Like parameter 1, parameters 7 and 8 are not
 * defined clearly in the standard, and a close reading indicates they are
 * optional, saying they "may be used".  This implementation ignores parameters
 * 6 (and above), and provides for the color space identifier by checking the
 * number of parameters:
 *	3 after "2" (no color space identifier)
 *	4 or more after "2" (color space identifier)
 *
 * By the way - all of the parameters are decimal integers, and missing
 * parameters represent a default value.  ISO-8613-6 is clear about that.
 */
#define extended_colors_limit(n) ((n) == 5 ? 1 : ((n) == 2 ? 3 : 0))
static Boolean
parse_extended_colors(XtermWidget xw, int *colorp, int *itemp, Boolean *extended)
{
    Boolean result = False;
    int item = *itemp;
    int next = item;
    int base = param_number(item);
    int code = -1;
    int values[3];		/* maximum number of subparameters */
    int need = 0;		/* number of subparameters needed */
    int have;
    int n;

    /*
     * On entry, 'item' points to the 38/48 code in the parameter array.
     * If that has subparameters, we will expect all of the values to
     * be subparameters of that item.
     */
    if ((have = param_has_subparams(item)) != 0) {
	/* accept CSI 38 : 5 : 1 m */
	/* accept CSI 38 : 2 : 1 : 2 : 3 m */
	code = get_subparam(base, 1);
	need = extended_colors_limit(code);
	next = item + have;
	for (n = 0; n < need && n < 3; ++n) {
	    values[n] = get_subparam(base, 2 + n + (have > 4));
	}
    } else if (++item < nparam) {
	++base;
	if ((have = param_has_subparams(item)) != 0) {
	    /* accept CSI 38 ; 5 : 1 m */
	    /* accept CSI 38 ; 2 : 1 : 2 : 3 m */
	    code = get_subparam(base, 0);
	    need = extended_colors_limit(code);
	    next = base + have;
	    for (n = 0; n < need && n < 3; ++n) {
		values[n] = get_subparam(base, 1 + n + (have > 3));
	    }
	} else {
	    /* accept CSI 38 ; 5 ; 1 m */
	    /* accept CSI 38 ; 2 ; 1 ; 2 ; 3 m */
	    code = GetParam(item);
	    need = extended_colors_limit(code);
	    next = item + need;
	    for (n = 0; n < need && n < 3; ++n) {
		values[n] = GetParam(item + 1 + n);
	    }
	}
    }
    item = next;

    *extended = False;
    switch (code) {
    case 2:
	/* direct color in rgb space */
	if ((values[0] >= 0 && values[0] < 256) &&
	    (values[1] >= 0 && values[1] < 256) &&
	    (values[2] >= 0 && values[2] < 256)) {
#if OPT_DIRECT_COLOR
	    if (TScreenOf(xw)->direct_color && xw->has_rgb) {
		*colorp = getDirectColor(xw, values[0], values[1], values[2]);
		result = True;
		*extended = True;
	    } else
#endif
	    {
		*colorp = xtermClosestColor(xw, values[0], values[1], values[2]);
		result = okIndexedColor(*colorp);
	    }
	} else {
	    *colorp = -1;
	}
	break;
    case 5:
	/* indexed color */
	*colorp = values[0];
	result = okIndexedColor(*colorp);
	break;
    default:
	*colorp = -1;
	break;
    }

    TRACE(("...resulting color %d/%d %s\n",
	   *colorp, NUM_ANSI_COLORS,
	   result ? "OK" : "ERR"));

    *itemp = item;
    return result;
}
#endif /* ...extended_colors */

static int
optional_param(int which)
{
    return (nparam > which) ? GetParam(which) : DEFAULT;
}

static int
zero_if_default(int which)
{
    int result = (nparam > which) ? GetParam(which) : 0;
    if (result <= 0)
	result = 0;
    return result;
}

static int
one_if_default(int which)
{
    int result = (nparam > which) ? GetParam(which) : 0;
    if (result <= 0)
	result = 1;
    return result;
}

/*
 * Color palette changes using the OSC controls require a repaint of the
 * screen - but not immediately.  Do the repaint as soon as we detect a
 * state which will not lead to another color palette change.
 */
static void
repaintWhenPaletteChanged(XtermWidget xw, struct ParseState *sp)
{
    Boolean ignore = False;

    switch (sp->nextstate) {
    case CASE_ESC:
	ignore = ((sp->parsestate == ansi_table) ||
		  (sp->parsestate == sos_table));
	break;
    case CASE_OSC:
	ignore = ((sp->parsestate == ansi_table) ||
		  (sp->parsestate == esc_table));
	break;
    case CASE_IGNORE:
	ignore = (sp->parsestate == sos_table);
	break;
    case CASE_ST:
	ignore = ((sp->parsestate == esc_table) ||
		  (sp->parsestate == sos_table));
	break;
    case CASE_ESC_DIGIT:
	ignore = (sp->parsestate == csi_table);
	break;
    case CASE_ESC_SEMI:
	ignore = (sp->parsestate == csi2_table);
	break;
    }

    if (!ignore) {
	TRACE(("repaintWhenPaletteChanged\n"));
	xw->work.palette_changed = False;
	xtermRepaint(xw);
    }
}

#if OPT_C1_PRINT || OPT_WIDE_CHARS
#define ParseSOS(screen) ((screen)->c1_printable == 0)
#else
#define ParseSOS(screen) 0
#endif

#define ResetState(sp) ParamsDone(), (sp)->parsestate = (sp)->groundtable

static void
illegal_parse(XtermWidget xw, unsigned c, struct ParseState *sp)
{
    ResetState(sp);
    sp->nextstate = sp->parsestate[E2A(c)];
    Bell(xw, XkbBI_MinorError, 0);
}

static void
init_parser(XtermWidget xw, struct ParseState *sp)
{
    TScreen *screen = TScreenOf(xw);

    memset(sp, 0, sizeof(*sp));
    sp->scssize = 94;		/* number of printable/nonspace ASCII */
    sp->lastchar = -1;		/* not a legal IChar */
    sp->nextstate = -1;		/* not a legal state */

    init_groundtable(screen, sp);
    ResetState(sp);
}

static void
init_reply(unsigned type)
{
    memset(&reply, 0, sizeof(reply));
    reply.a_type = (Char) type;
}

static Boolean
doparsing(XtermWidget xw, unsigned c, struct ParseState *sp)
{
    TScreen *screen = TScreenOf(xw);
    int item;
    int count;
    int value;
    int laststate;
    int thischar = -1;
    XTermRect myRect;
    Boolean extended;

    do {
#if OPT_WIDE_CHARS
	int this_is_wide = 0;

	/*
	 * Handle zero-width combining characters.  Make it faster by noting
	 * that according to the Unicode charts, the majority of Western
	 * character sets do not use this feature.  There are some unassigned
	 * codes at 0x242, but no zero-width characters until past 0x300.
	 */
	if (c >= 0x300
	    && screen->wide_chars
	    && my_wcwidth((wchar_t) c) == 0
	    && !isWideControl(c)) {
	    int prev, test;
	    Boolean used = True;
	    int use_row;
	    int use_col;

	    WriteNow();
	    use_row = (screen->char_was_written
		       ? screen->last_written_row
		       : screen->cur_row);
	    use_col = (screen->char_was_written
		       ? screen->last_written_col
		       : screen->cur_col);

	    /*
	     * Check if the latest data can be added to the base character.
	     * If there is already a combining character stored for the cell,
	     * we cannot, since that would change the order.
	     */
	    if (screen->normalized_c
		&& !IsCellCombined(screen, use_row, use_col)) {
		prev = (int) XTERM_CELL(use_row, use_col);
		test = do_precomposition(prev, (int) c);
		TRACE(("do_precomposition (U+%04X [%d], U+%04X [%d]) -> U+%04X [%d]\n",
		       prev, my_wcwidth((wchar_t) prev),
		       (int) c, my_wcwidth((wchar_t) c),
		       test, my_wcwidth((wchar_t) test)));
	    } else {
		prev = -1;
		test = -1;
	    }

	    /* substitute combined character with precomposed character
	     * only if it does not change the width of the base character
	     */
	    if (test != -1
		&& my_wcwidth((wchar_t) test) == my_wcwidth((wchar_t) prev)) {
		putXtermCell(screen, use_row, use_col, test);
	    } else if (screen->char_was_written
		       || getXtermCell(screen, use_row, use_col) > ' ') {
		addXtermCombining(screen, use_row, use_col, c);
	    } else {
		/*
		 * none of the above... we will add the combining character as
		 * a base character.
		 */
		used = False;
	    }

	    if (used) {
		if (!screen->scroll_amt)
		    ScrnUpdate(xw, use_row, use_col, 1, 1, 1);
		continue;
	    }
	}
#endif

	/* Intercept characters for printer controller mode */
	if (PrinterOf(screen).printer_controlmode == 2) {
	    if ((c = (unsigned) xtermPrinterControl(xw, (int) c)) == 0)
		continue;
	}

	/*
	 * VT52 is a little ugly in the one place it has a parameterized
	 * control sequence, since the parameter falls after the character
	 * that denotes the type of sequence.
	 */
#if OPT_VT52_MODE
	if (sp->vt52_cup) {
	    if (nparam < NPARAM - 1) {
		SetParam(nparam++, (int) (c & 0x7f) - 32);
		parms.is_sub[nparam] = 0;
	    }
	    if (nparam < 2)
		continue;
	    sp->vt52_cup = False;
	    CursorSet(screen, zero_if_default(0), zero_if_default(1), xw->flags);
	    sp->parsestate = vt52_table;
	    SetParam(0, 0);
	    SetParam(1, 0);
	    continue;
	}
#endif

	laststate = sp->nextstate;
	if (c == ANSI_DEL
	    && sp->parsestate == sp->groundtable
	    && sp->scssize == 96
	    && sp->scstype != 0) {
	    /*
	     * Handle special case of shifts for 96-character sets by checking
	     * if we have a DEL.  The other special case for SPACE will always
	     * be printable.
	     */
	    sp->nextstate = CASE_PRINT;
	} else
#if OPT_WIDE_CHARS
	if (c > 255) {
	    /*
	     * The parsing tables all have 256 entries.  If we're supporting
	     * wide characters, we handle them by treating them the same as
	     * printing characters.
	     */
	    if (sp->parsestate == sp->groundtable) {
		sp->nextstate = CASE_PRINT;
	    } else if (sp->parsestate == sos_table) {
		c &= 0xffff;
		if (c > 255) {
		    TRACE(("Found code > 255 while in SOS state: %04X\n", c));
		    c = '?';
		}
	    } else {
		sp->nextstate = CASE_GROUND_STATE;
	    }
	} else
#endif
	    sp->nextstate = sp->parsestate[E2A(c)];

#if OPT_BROKEN_OSC
	/*
	 * Linux console palette escape sequences start with an OSC, but do
	 * not terminate correctly.  Some scripts do not check before writing
	 * them, making xterm appear to hang (it's awaiting a valid string
	 * terminator).  Just ignore these if we see them - there's no point
	 * in emulating bad code.
	 */
	if (screen->brokenLinuxOSC
	    && sp->parsestate == sos_table) {
	    if (sp->string_used) {
		switch (sp->string_area[0]) {
		case 'P':
		    if (sp->string_used <= 7)
			break;
		    /* FALLTHRU */
		case 'R':
		    illegal_parse(xw, c, sp);
		    TRACE(("Reset to ground state (brokenLinuxOSC)\n"));
		    break;
		}
	    }
	}
#endif

#if OPT_BROKEN_ST
	/*
	 * Before patch #171, carriage control embedded within an OSC string
	 * would terminate it.  Some (buggy, of course) applications rely on
	 * this behavior.  Accommodate them by allowing one to compile xterm
	 * and emulate the old behavior.
	 */
	if (screen->brokenStringTerm
	    && sp->parsestate == sos_table
	    && c < 32) {
	    switch (c) {
	    case ANSI_EOT:	/* FALLTHRU */
	    case ANSI_BS:	/* FALLTHRU */
	    case ANSI_HT:	/* FALLTHRU */
	    case ANSI_LF:	/* FALLTHRU */
	    case ANSI_VT:	/* FALLTHRU */
	    case ANSI_FF:	/* FALLTHRU */
	    case ANSI_CR:	/* FALLTHRU */
	    case ANSI_SO:	/* FALLTHRU */
	    case ANSI_SI:	/* FALLTHRU */
	    case ANSI_XON:	/* FALLTHRU */
	    case ANSI_CAN:
		illegal_parse(xw, c, sp);
		TRACE(("Reset to ground state (brokenStringTerm)\n"));
		break;
	    }
	}
#endif

#if OPT_C1_PRINT
	/*
	 * This is not completely foolproof, but will allow an application
	 * with values in the C1 range to use them as printable characters,
	 * provided that they are not intermixed with an escape sequence.
	 */
	if (screen->c1_printable
	    && (c >= 128 && c < 256)) {
	    sp->nextstate = (sp->parsestate == esc_table
			     ? CASE_ESC_IGNORE
			     : sp->parsestate[E2A(160)]);
	    TRACE(("allowC1Printable %04X %s ->%s\n",
		   c, which_table(sp->parsestate),
		   visibleVTparse(sp->nextstate)));
	}
#endif

#if OPT_WIDE_CHARS
	/*
	 * If we have a C1 code and the c1_printable flag is not set, simply
	 * ignore it when it was translated from UTF-8.  That is because the
	 * value could not have been present as-is in the UTF-8.
	 *
	 * To see that CASE_IGNORE is a consistent value, note that it is
	 * always used for NUL and other uninteresting C0 controls.
	 */
#if OPT_C1_PRINT
	if (!screen->c1_printable)
#endif
	    if (screen->wide_chars
		&& (c >= 128 && c < 160)) {
		sp->nextstate = CASE_IGNORE;
	    }

	/*
	 * If this character is a different width than the last one, put the
	 * previous text into the buffer and draw it now.
	 */
	this_is_wide = isWide((int) c);
	if (this_is_wide != sp->last_was_wide) {
	    WriteNow();
	}
#endif

	/*
	 * Accumulate string for printable text.  This may be 8/16-bit
	 * characters.
	 */
	if (sp->nextstate == CASE_PRINT) {
	    SafeAlloc(IChar, sp->print_area, sp->print_used, sp->print_size);
	    if (new_string == 0) {
		xtermWarning("Cannot allocate %lu bytes for printable text\n",
			     (unsigned long) new_length);
		continue;
	    }
	    SafeFree(sp->print_area, sp->print_size);
#if OPT_VT52_MODE
	    /*
	     * Strip output text to 7-bits for VT52.  We should do this for
	     * VT100 also (which is a 7-bit device), but xterm has been
	     * doing this for so long we shouldn't change this behavior.
	     */
	    if (screen->vtXX_level < 1)
		c &= 0x7f;
#endif
	    sp->print_area[sp->print_used++] = (IChar) c;
	    sp->lastchar = thischar = (int) c;
#if OPT_WIDE_CHARS
	    sp->last_was_wide = this_is_wide;
#endif
	    if (morePtyData(screen, VTbuffer)) {
		continue;
	    }
	}

	if (sp->nextstate == CASE_PRINT
	    || (laststate == CASE_PRINT && sp->print_used)) {
	    WriteNow();
	}

	/*
	 * Accumulate string for APC, DCS, PM, OSC, SOS controls
	 * This should always be 8-bit characters.
	 */
	if (sp->parsestate == sos_table) {
	    SafeAlloc(Char, sp->string_area, sp->string_used, sp->string_size);
	    if (new_string == 0) {
		xtermWarning("Cannot allocate %lu bytes for string mode %d\n",
			     (unsigned long) new_length, sp->string_mode);
		continue;
	    }
	    SafeFree(sp->string_area, sp->string_size);
#if OPT_WIDE_CHARS
	    /*
	     * We cannot display codes above 255, but let's try to
	     * accommodate the application a little by not aborting the
	     * string.
	     */
	    if ((c & 0xffff) > 255) {
		sp->nextstate = CASE_PRINT;
		c = '?';
	    }
#endif
	    sp->string_area[(sp->string_used)++] = CharOf(c);
	} else if (sp->parsestate != esc_table) {
	    /* if we were accumulating, we're not any more */
	    sp->string_mode = 0;
	    sp->string_used = 0;
	}

	DumpParams();
	TRACE(("parse %04X -> %s %s (used=%lu)\n",
	       c, visibleVTparse(sp->nextstate),
	       which_table(sp->parsestate),
	       (unsigned long) sp->string_used));

	/*
	 * If the parameter list has subparameters (tokens separated by ":")
	 * reject any controls that do not accept subparameters.
	 */
	if (parms.has_subparams) {
	    switch (sp->nextstate) {
	    case CASE_GROUND_STATE:
	    case CASE_CSI_IGNORE:
		/* FALLTHRU */

	    case CASE_ESC_DIGIT:
	    case CASE_ESC_SEMI:
	    case CASE_ESC_COLON:
		/* these states are required to parse parameter lists */
		break;

	    case CASE_SGR:
		TRACE(("...possible subparam usage\n"));
		break;

	    case CASE_CSI_DEC_DOLLAR_STATE:
	    case CASE_CSI_DOLLAR_STATE:
	    case CASE_CSI_EX_STATE:
	    case CASE_CSI_QUOTE_STATE:
	    case CASE_CSI_SPACE_STATE:
	    case CASE_CSI_STAR_STATE:
	    case CASE_CSI_TICK_STATE:
	    case CASE_DEC2_STATE:
	    case CASE_DEC3_STATE:
	    case CASE_DEC_STATE:
		/* use this branch when we do not yet have the final character */
		TRACE(("...unexpected subparam usage\n"));
		ParamsDone();
		sp->nextstate = CASE_CSI_IGNORE;
		break;

	    default:
		/* use this branch for cases where we have the final character
		 * in the table that processed the parameter list.
		 */
		TRACE(("...unexpected subparam usage\n"));
		ResetState(sp);
		continue;
	    }
	}

	if (xw->work.palette_changed) {
	    repaintWhenPaletteChanged(xw, sp);
	}

	switch (sp->nextstate) {
	case CASE_PRINT:
	    TRACE(("CASE_PRINT - printable characters\n"));
	    break;

	case CASE_GROUND_STATE:
	    TRACE(("CASE_GROUND_STATE - exit ignore mode\n"));
	    ResetState(sp);
	    break;

	case CASE_IGNORE:
	    TRACE(("CASE_IGNORE - Ignore character %02X\n", c));
	    break;

	case CASE_ENQ:
	    TRACE(("CASE_ENQ - answerback\n"));
	    for (count = 0; screen->answer_back[count] != 0; count++)
		unparseputc(xw, screen->answer_back[count]);
	    unparse_end(xw);
	    break;

	case CASE_BELL:
	    TRACE(("CASE_BELL - bell\n"));
	    if (sp->string_mode == ANSI_OSC) {
		if (sp->string_used)
		    sp->string_area[--(sp->string_used)] = '\0';
		do_osc(xw, sp->string_area, sp->string_used, (int) c);
		ResetState(sp);
	    } else {
		/* bell */
		Bell(xw, XkbBI_TerminalBell, 0);
	    }
	    break;

	case CASE_BS:
	    TRACE(("CASE_BS - backspace\n"));
	    CursorBack(xw, 1);
	    break;

	case CASE_CR:
	    TRACE(("CASE_CR\n"));
	    CarriageReturn(xw);
	    break;

	case CASE_ESC:
	    if_OPT_VT52_MODE(screen, {
		sp->parsestate = vt52_esc_table;
		break;
	    });
	    sp->parsestate = esc_table;
	    break;

#if OPT_VT52_MODE
	case CASE_VT52_CUP:
	    TRACE(("CASE_VT52_CUP - VT52 cursor addressing\n"));
	    sp->vt52_cup = True;
	    InitParams();
	    break;

	case CASE_VT52_IGNORE:
	    TRACE(("CASE_VT52_IGNORE - VT52 ignore-character\n"));
	    sp->parsestate = vt52_ignore_table;
	    break;
#endif

	case CASE_VMOT:
	    TRACE(("CASE_VMOT\n"));
	    /*
	     * form feed, line feed, vertical tab
	     */
	    xtermAutoPrint(xw, c);
	    xtermIndex(xw, 1);
	    if (xw->flags & LINEFEED)
		CarriageReturn(xw);
	    else
		do_xevents();
	    break;

	case CASE_CBT:
	    TRACE(("CASE_CBT\n"));
	    /* cursor backward tabulation */
	    count = one_if_default(0);
	    while ((count-- > 0)
		   && (TabToPrevStop(xw))) ;
	    ResetState(sp);
	    break;

	case CASE_CHT:
	    TRACE(("CASE_CHT\n"));
	    /* cursor forward tabulation */
	    count = one_if_default(0);
	    while ((count-- > 0)
		   && (TabToNextStop(xw))) ;
	    ResetState(sp);
	    break;

	case CASE_TAB:
	    /* tab */
	    TabToNextStop(xw);
	    break;

	case CASE_SI:
	    screen->curgl = 0;
	    if_OPT_VT52_MODE(screen, {
		ResetState(sp);
	    });
	    break;

	case CASE_SO:
	    screen->curgl = 1;
	    if_OPT_VT52_MODE(screen, {
		ResetState(sp);
	    });
	    break;

	case CASE_DECDHL:
	    xterm_DECDHL(xw, c == '3');
	    ResetState(sp);
	    break;

	case CASE_DECSWL:
	    xterm_DECSWL(xw);
	    ResetState(sp);
	    break;

	case CASE_DECDWL:
	    xterm_DECDWL(xw);
	    ResetState(sp);
	    break;

	case CASE_SCR_STATE:
	    /* enter scr state */
	    sp->parsestate = scrtable;
	    break;

	case CASE_SCS0_STATE:
	    /* enter scs state 0 */
	    select_charset(sp, 0, 94);
	    break;

	case CASE_SCS1_STATE:
	    /* enter scs state 1 */
	    select_charset(sp, 1, 94);
	    break;

	case CASE_SCS2_STATE:
	    /* enter scs state 2 */
	    select_charset(sp, 2, 94);
	    break;

	case CASE_SCS3_STATE:
	    /* enter scs state 3 */
	    select_charset(sp, 3, 94);
	    break;

	case CASE_SCS1A_STATE:
	    /* enter scs state 1 */
	    select_charset(sp, 1, 96);
	    break;

	case CASE_SCS2A_STATE:
	    /* enter scs state 2 */
	    select_charset(sp, 2, 96);
	    break;

	case CASE_SCS3A_STATE:
	    /* enter scs state 3 */
	    select_charset(sp, 3, 96);
	    break;

	case CASE_ESC_IGNORE:
	    /* unknown escape sequence */
	    sp->parsestate = eigtable;
	    break;

	case CASE_ESC_DIGIT:
	    /* digit in csi or dec mode */
	    if (nparam > 0) {
		value = zero_if_default(nparam - 1);
		SetParam(nparam - 1, (10 * value) + ((int) c - '0'));
		if (GetParam(nparam - 1) > 65535)
		    SetParam(nparam - 1, 65535);
		if (sp->parsestate == csi_table)
		    sp->parsestate = csi2_table;
	    }
	    break;

	case CASE_ESC_SEMI:
	    /* semicolon in csi or dec mode */
	    if (nparam < NPARAM) {
		parms.is_sub[nparam] = 0;
		SetParam(nparam++, DEFAULT);
	    }
	    if (sp->parsestate == csi_table)
		sp->parsestate = csi2_table;
	    break;

	    /*
	     * A _few_ commands accept colon-separated subparameters.
	     * Mark the parameter list so that we can exclude (most) bogus
	     * commands with simple/fast checks.
	     */
	case CASE_ESC_COLON:
	    if (nparam < NPARAM) {
		parms.has_subparams = 1;
		if (nparam == 0) {
		    parms.is_sub[nparam] = 1;
		    SetParam(nparam++, DEFAULT);
		} else if (parms.is_sub[nparam - 1] == 0) {
		    parms.is_sub[nparam - 1] = 1;
		    parms.is_sub[nparam] = 2;
		    parms.params[nparam] = 0;
		    ++nparam;
		} else {
		    parms.is_sub[nparam] = 1 + parms.is_sub[nparam - 1];
		    parms.params[nparam] = 0;
		    ++nparam;
		}
	    }
	    break;

	case CASE_DEC_STATE:
	    /* enter dec mode */
	    sp->parsestate = dec_table;
	    break;

	case CASE_DEC2_STATE:
	    /* enter dec2 mode */
	    sp->parsestate = dec2_table;
	    break;

	case CASE_DEC3_STATE:
	    /* enter dec3 mode */
	    sp->parsestate = dec3_table;
	    break;

	case CASE_ICH:
	    TRACE(("CASE_ICH - insert char\n"));
	    InsertChar(xw, (unsigned) one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_CUU:
	    TRACE(("CASE_CUU - cursor up\n"));
	    CursorUp(screen, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_CUD:
	    TRACE(("CASE_CUD - cursor down\n"));
	    CursorDown(screen, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_CUF:
	    TRACE(("CASE_CUF - cursor forward\n"));
	    CursorForward(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_CUB:
	    TRACE(("CASE_CUB - cursor backward\n"));
	    CursorBack(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_CUP:
	    TRACE(("CASE_CUP - cursor position\n"));
	    if_OPT_XMC_GLITCH(screen, {
		Jump_XMC(xw);
	    });
	    CursorSet(screen, one_if_default(0) - 1, one_if_default(1) - 1, xw->flags);
	    ResetState(sp);
	    break;

	case CASE_VPA:
	    TRACE(("CASE_VPA - vertical position absolute\n"));
	    CursorSet(screen, one_if_default(0) - 1, CursorCol(xw), xw->flags);
	    ResetState(sp);
	    break;

	case CASE_HPA:
	    TRACE(("CASE_HPA - horizontal position absolute\n"));
	    CursorSet(screen, CursorRow(xw), one_if_default(0) - 1, xw->flags);
	    ResetState(sp);
	    break;

	case CASE_VPR:
	    TRACE(("CASE_VPR - vertical position relative\n"));
	    CursorSet(screen,
		      CursorRow(xw) + one_if_default(0),
		      CursorCol(xw),
		      xw->flags);
	    ResetState(sp);
	    break;

	case CASE_HPR:
	    TRACE(("CASE_HPR - horizontal position relative\n"));
	    CursorSet(screen,
		      CursorRow(xw),
		      CursorCol(xw) + one_if_default(0),
		      xw->flags);
	    ResetState(sp);
	    break;

	case CASE_HP_BUGGY_LL:
	    TRACE(("CASE_HP_BUGGY_LL\n"));
	    /* Some HP-UX applications have the bug that they
	       assume ESC F goes to the lower left corner of
	       the screen, regardless of what terminfo says. */
	    if (screen->hp_ll_bc)
		CursorSet(screen, screen->max_row, 0, xw->flags);
	    ResetState(sp);
	    break;

	case CASE_ED:
	    TRACE(("CASE_ED - erase display\n"));
	    do_cd_xtra_scroll(xw);
	    do_erase_display(xw, zero_if_default(0), OFF_PROTECT);
	    ResetState(sp);
	    break;

	case CASE_EL:
	    TRACE(("CASE_EL - erase line\n"));
	    do_erase_line(xw, zero_if_default(0), OFF_PROTECT);
	    ResetState(sp);
	    break;

	case CASE_ECH:
	    TRACE(("CASE_ECH - erase char\n"));
	    /* ECH */
	    ClearRight(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_IL:
	    TRACE(("CASE_IL - insert line\n"));
	    set_cur_col(screen, ScrnLeftMargin(xw));
	    InsertLine(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_DL:
	    TRACE(("CASE_DL - delete line\n"));
	    set_cur_col(screen, ScrnLeftMargin(xw));
	    DeleteLine(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_DCH:
	    TRACE(("CASE_DCH - delete char\n"));
	    DeleteChar(xw, (unsigned) one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_TRACK_MOUSE:
	    /*
	     * A single parameter other than zero is always scroll-down.
	     * A zero-parameter is used to reset the mouse mode, and is
	     * not useful for scrolling anyway.
	     */
	    if (nparam > 1 || GetParam(0) == 0) {
		CELL start;

		TRACE(("CASE_TRACK_MOUSE\n"));
		/* Track mouse as long as in window and between
		 * specified rows
		 */
		start.row = one_if_default(2) - 1;
		start.col = GetParam(1) - 1;
		TrackMouse(xw,
			   GetParam(0),
			   &start,
			   GetParam(3) - 1, GetParam(4) - 2);
	    } else {
		TRACE(("CASE_SD - scroll down\n"));
		/* SD */
		RevScroll(xw, one_if_default(0));
		do_xevents();
	    }
	    ResetState(sp);
	    break;

	case CASE_DECID:
	    TRACE(("CASE_DECID\n"));
	    if_OPT_VT52_MODE(screen, {
		unparseputc(xw, ANSI_ESC);
		unparseputc(xw, '/');
		unparseputc(xw, 'Z');
		unparse_end(xw);
		ResetState(sp);
		break;
	    });
	    SetParam(0, DEFAULT);	/* Default ID parameter */
	    /* FALLTHRU */
	case CASE_DA1:
	    TRACE(("CASE_DA1\n"));
	    if (GetParam(0) <= 0) {	/* less than means DEFAULT */
		count = 0;
		init_reply(ANSI_CSI);
		reply.a_pintro = '?';

		/*
		 * The first parameter corresponds to the highest operating
		 * level (i.e., service level) of the emulation.  A DEC
		 * terminal can be setup to respond with a different DA
		 * response, but there's no control sequence that modifies
		 * this.  We set it via a resource.
		 */
		if (screen->terminal_id < 200) {
		    switch (screen->terminal_id) {
		    case 125:
			reply.a_param[count++] = 12;	/* VT125 */
#if OPT_REGIS_GRAPHICS
			reply.a_param[count++] = 0 | 2 | 1;	/* no STP, AVO, GPO (ReGIS) */
#else
			reply.a_param[count++] = 0 | 2 | 0;	/* no STP, AVO, no GPO (ReGIS) */
#endif
			reply.a_param[count++] = 0;	/* no printer */
			reply.a_param[count++] = XTERM_PATCH;	/* ROM version */
			break;
		    case 102:
			reply.a_param[count++] = 6;	/* VT102 */
			break;
		    case 101:
			reply.a_param[count++] = 1;	/* VT101 */
			reply.a_param[count++] = 0;	/* no options */
			break;
		    default:	/* VT100 */
			reply.a_param[count++] = 1;	/* VT100 */
			reply.a_param[count++] = 0 | 2 | 0;	/* no STP, AVO, no GPO (ReGIS) */
			break;
		    }
		} else {
		    reply.a_param[count++] = (ParmType) (60
							 + screen->terminal_id
							 / 100);
		    reply.a_param[count++] = 1;		/* 132-columns */
		    reply.a_param[count++] = 2;		/* printer */
#if OPT_REGIS_GRAPHICS
		    if (screen->terminal_id == 240 ||
			screen->terminal_id == 241 ||
			screen->terminal_id == 330 ||
			screen->terminal_id == 340) {
			reply.a_param[count++] = 3;	/* ReGIS graphics */
		    }
#endif
#if OPT_SIXEL_GRAPHICS
		    if (screen->terminal_id == 240 ||
			screen->terminal_id == 241 ||
			screen->terminal_id == 330 ||
			screen->terminal_id == 340 ||
			screen->terminal_id == 382) {
			reply.a_param[count++] = 4;	/* sixel graphics */
		    }
#endif
		    reply.a_param[count++] = 6;		/* selective-erase */
#if OPT_SUNPC_KBD
		    if (xw->keyboard.type == keyboardIsVT220)
#endif
			reply.a_param[count++] = 8;	/* user-defined-keys */
		    reply.a_param[count++] = 9;		/* national replacement charsets */
		    reply.a_param[count++] = 15;	/* technical characters */
		    if (screen->terminal_id >= 400) {
			reply.a_param[count++] = 18;	/* windowing capability */
			reply.a_param[count++] = 21;	/* horizontal scrolling */
		    }
		    if_OPT_ISO_COLORS(screen, {
			reply.a_param[count++] = 22;	/* ANSI color, VT525 */
		    });
#if OPT_DEC_LOCATOR
		    reply.a_param[count++] = 29;	/* ANSI text locator */
#endif
		}
		reply.a_nparam = (ParmType) count;
		reply.a_inters = 0;
		reply.a_final = 'c';
		unparseseq(xw, &reply);
	    }
	    ResetState(sp);
	    break;

	case CASE_DA2:
	    TRACE(("CASE_DA2\n"));
	    if (GetParam(0) <= 0) {	/* less than means DEFAULT */
		count = 0;
		init_reply(ANSI_CSI);
		reply.a_pintro = '>';

		if (screen->terminal_id >= 200) {
		    switch (screen->terminal_id) {
		    case 220:
		    default:
			reply.a_param[count++] = 1;	/* VT220 */
			break;
		    case 240:
			/* http://www.decuslib.com/DECUS/vax87a/gendyn/vt200_kind.lis */
			reply.a_param[count++] = 2;	/* VT240 */
			break;
		    case 320:
			/* http://www.vt100.net/docs/vt320-uu/appendixe.html */
			reply.a_param[count++] = 24;	/* VT320 */
			break;
		    case 330:
			reply.a_param[count++] = 18;	/* VT330 */
			break;
		    case 340:
			reply.a_param[count++] = 19;	/* VT340 */
			break;
		    case 420:
			reply.a_param[count++] = 41;	/* VT420 */
			break;
		    case 510:
			/* http://www.vt100.net/docs/vt510-rm/DA2 */
			reply.a_param[count++] = 61;	/* VT510 */
			break;
		    case 520:
			reply.a_param[count++] = 64;	/* VT520 */
			break;
		    case 525:
			reply.a_param[count++] = 65;	/* VT525 */
			break;
		    }
		} else {
		    reply.a_param[count++] = 0;		/* VT100 (nonstandard) */
		}
		reply.a_param[count++] = XTERM_PATCH;	/* Version */
		reply.a_param[count++] = 0;	/* options (none) */
		reply.a_nparam = (ParmType) count;
		reply.a_inters = 0;
		reply.a_final = 'c';
		unparseseq(xw, &reply);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECRPTUI:
	    TRACE(("CASE_DECRPTUI\n"));
	    if ((screen->vtXX_level >= 4)
		&& (GetParam(0) <= 0)) {	/* less than means DEFAULT */
		unparseputc1(xw, ANSI_DCS);
		unparseputc(xw, '!');
		unparseputc(xw, '|');
		unparseputc(xw, '0');
		unparseputc1(xw, ANSI_ST);
		unparse_end(xw);
	    }
	    ResetState(sp);
	    break;

	case CASE_TBC:
	    TRACE(("CASE_TBC - tab clear\n"));
	    if ((value = GetParam(0)) <= 0)	/* less than means default */
		TabClear(xw->tabs, screen->cur_col);
	    else if (value == 3)
		TabZonk(xw->tabs);
	    ResetState(sp);
	    break;

	case CASE_SET:
	    TRACE(("CASE_SET - set mode\n"));
	    ansi_modes(xw, bitset);
	    ResetState(sp);
	    break;

	case CASE_RST:
	    TRACE(("CASE_RST - reset mode\n"));
	    ansi_modes(xw, bitclr);
	    ResetState(sp);
	    break;

	case CASE_SGR:
	    for (item = 0; item < nparam; ++item) {
		int op = GetParam(item);

		if_OPT_XMC_GLITCH(screen, {
		    Mark_XMC(xw, op);
		});
		TRACE(("CASE_SGR %d\n", op));

		/*
		 * Only SGR 38/48 accept subparameters, and in those cases
		 * the values will not be seen at this point.
		 */
		if (param_has_subparams(item)) {
		    switch (op) {
		    case 38:
			/* FALLTHRU */
		    case 48:
			if_OPT_ISO_COLORS(screen, {
			    break;
			});
			/* FALLTHRU */
		    default:
			TRACE(("...unexpected subparameter in SGR\n"));
			op = 9999;
			ResetState(sp);
			break;
		    }
		}

		switch (op) {
		case DEFAULT:
		    /* FALLTHRU */
		case 0:
#if OPT_WIDE_ATTRS
		    setItalicFont(xw, False);
#endif
		    UIntClr(xw->flags,
			    (SGR_MASK | SGR_MASK2 | INVISIBLE));
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Colors(xw);
		    });
		    break;
		case 1:	/* Bold                 */
		    UIntSet(xw->flags, BOLD);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
#if OPT_WIDE_ATTRS
		case 2:	/* faint, decreased intensity or second colour */
		    UIntSet(xw->flags, ATR_FAINT);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 3:	/* italicized */
		    setItalicFont(xw, UseItalicFont(screen));
		    UIntSet(xw->flags, ATR_ITALIC);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
#endif
		case 4:	/* Underscore           */
		    UIntSet(xw->flags, UNDERLINE);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 5:	/* Blink                */
		    UIntSet(xw->flags, BLINK);
		    StartBlinking(screen);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 7:
		    UIntSet(xw->flags, INVERSE);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedBG(xw);
		    });
		    break;
		case 8:
		    UIntSet(xw->flags, INVISIBLE);
		    break;
#if OPT_WIDE_ATTRS
		case 9:	/* crossed-out characters */
		    UIntSet(xw->flags, ATR_STRIKEOUT);
		    break;
#endif
#if OPT_WIDE_ATTRS
		case 21:	/* doubly-underlined */
		    UIntSet(xw->flags, ATR_DBL_UNDER);
		    break;
#endif
		case 22:	/* reset 'bold' */
		    UIntClr(xw->flags, BOLD);
#if OPT_WIDE_ATTRS
		    UIntClr(xw->flags, ATR_FAINT);
#endif
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
#if OPT_WIDE_ATTRS
		case 23:	/* not italicized */
		    setItalicFont(xw, False);
		    UIntClr(xw->flags, ATR_ITALIC);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
#endif
		case 24:
		    UIntClr(xw->flags, UNDERLINE);
#if OPT_WIDE_ATTRS
		    UIntClr(xw->flags, ATR_DBL_UNDER);
#endif
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 25:	/* reset 'blink' */
		    UIntClr(xw->flags, BLINK);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedFG(xw);
		    });
		    break;
		case 27:
		    UIntClr(xw->flags, INVERSE);
		    if_OPT_ISO_COLORS(screen, {
			setExtendedBG(xw);
		    });
		    break;
		case 28:
		    UIntClr(xw->flags, INVISIBLE);
		    break;
#if OPT_WIDE_ATTRS
		case 29:	/* not crossed out */
		    UIntClr(xw->flags, ATR_STRIKEOUT);
		    break;
#endif
		case 30:
		    /* FALLTHRU */
		case 31:
		    /* FALLTHRU */
		case 32:
		    /* FALLTHRU */
		case 33:
		    /* FALLTHRU */
		case 34:
		    /* FALLTHRU */
		case 35:
		    /* FALLTHRU */
		case 36:
		    /* FALLTHRU */
		case 37:
		    if_OPT_ISO_COLORS(screen, {
			xw->sgr_foreground = (op - 30);
			clrDirectFG(xw->flags);
			setExtendedFG(xw);
		    });
		    break;
		case 38:
		    /* This is more complicated than I'd like, but it should
		     * properly eat all the parameters for unsupported modes.
		     */
		    if_OPT_ISO_COLORS(screen, {
			if (parse_extended_colors(xw, &value, &item,
						  &extended)) {
			    xw->sgr_foreground = value;
			    setDirectFG(xw->flags, extended);
			    setExtendedFG(xw);
			}
		    });
		    break;
		case 39:
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Foreground(xw);
		    });
		    break;
		case 40:
		    /* FALLTHRU */
		case 41:
		    /* FALLTHRU */
		case 42:
		    /* FALLTHRU */
		case 43:
		    /* FALLTHRU */
		case 44:
		    /* FALLTHRU */
		case 45:
		    /* FALLTHRU */
		case 46:
		    /* FALLTHRU */
		case 47:
		    if_OPT_ISO_COLORS(screen, {
			xw->sgr_background = (op - 40);
			clrDirectBG(xw->flags);
			setExtendedBG(xw);
		    });
		    break;
		case 48:
		    if_OPT_ISO_COLORS(screen, {
			if (parse_extended_colors(xw, &value, &item,
						  &extended)) {
			    xw->sgr_background = value;
			    setDirectBG(xw->flags, extended);
			    setExtendedBG(xw);
			}
		    });
		    break;
		case 49:
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Background(xw);
		    });
		    break;
		case 90:
		    /* FALLTHRU */
		case 91:
		    /* FALLTHRU */
		case 92:
		    /* FALLTHRU */
		case 93:
		    /* FALLTHRU */
		case 94:
		    /* FALLTHRU */
		case 95:
		    /* FALLTHRU */
		case 96:
		    /* FALLTHRU */
		case 97:
		    if_OPT_AIX_COLORS(screen, {
			xw->sgr_foreground = (op - 90 + 8);
			clrDirectFG(xw->flags);
			setExtendedFG(xw);
		    });
		    break;
		case 100:
#if !OPT_AIX_COLORS
		    if_OPT_ISO_COLORS(screen, {
			reset_SGR_Foreground(xw);
			reset_SGR_Background(xw);
		    });
		    break;
#endif
		case 101:
		    /* FALLTHRU */
		case 102:
		    /* FALLTHRU */
		case 103:
		    /* FALLTHRU */
		case 104:
		    /* FALLTHRU */
		case 105:
		    /* FALLTHRU */
		case 106:
		    /* FALLTHRU */
		case 107:
		    if_OPT_AIX_COLORS(screen, {
			xw->sgr_background = (op - 100 + 8);
			clrDirectBG(xw->flags);
			setExtendedBG(xw);
		    });
		    break;
		}
	    }
	    ResetState(sp);
	    break;

	    /* DSR (except for the '?') is a superset of CPR */
	case CASE_DSR:
	    sp->private_function = True;

	    /* FALLTHRU */
	case CASE_CPR:
	    TRACE(("CASE_DSR - device status report\n"));
	    count = 0;
	    init_reply(ANSI_CSI);
	    reply.a_pintro = CharOf(sp->private_function ? '?' : 0);
	    reply.a_inters = 0;
	    reply.a_final = 'n';

	    switch (GetParam(0)) {
	    case 5:
		TRACE(("...request operating status\n"));
		/* operating status */
		reply.a_param[count++] = 0;	/* (no malfunction ;-) */
		break;
	    case 6:
		TRACE(("...request %s\n",
		       (sp->private_function
			? "DECXCPR"
			: "CPR")));
		/* CPR */
		/* DECXCPR (with page=1) */
		value = (screen->cur_row + 1);
		if ((xw->flags & ORIGIN) != 0) {
		    value -= screen->top_marg;
		}
		reply.a_param[count++] = (ParmType) value;

		value = (screen->cur_col + 1);
		if ((xw->flags & ORIGIN) != 0) {
		    value -= screen->lft_marg;
		}
		reply.a_param[count++] = (ParmType) value;

		if (sp->private_function
		    && screen->vtXX_level >= 4) {	/* VT420 */
		    reply.a_param[count++] = 1;
		}
		reply.a_final = 'R';
		break;
	    case 15:
		TRACE(("...request printer status\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 2) {	/* VT220 */
		    reply.a_param[count++] = 13;	/* no printer detected */
		}
		break;
	    case 25:
		TRACE(("...request UDK status\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 2) {	/* VT220 */
		    reply.a_param[count++] = 20;	/* UDK always unlocked */
		}
		break;
	    case 26:
		TRACE(("...request keyboard status\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 2) {	/* VT220 */
		    reply.a_param[count++] = 27;
		    reply.a_param[count++] = 1;		/* North American */
		    if (screen->vtXX_level >= 4) {	/* VT420 */
			reply.a_param[count++] = 0;	/* ready */
			reply.a_param[count++] = 0;	/* LK201 */
		    }
		}
		break;
	    case 53:		/* according to existing xterm handling */
		/* FALLTHRU */
	    case 55:		/* according to the VT330/VT340 Text Programming Manual */
		TRACE(("...request locator status\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 2) {	/* VT220 */
#if OPT_DEC_LOCATOR
		    reply.a_param[count++] = 50;	/* locator ready */
#else
		    reply.a_param[count++] = 53;	/* no locator */
#endif
		}
		break;
	    case 56:
		TRACE(("...request locator type\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 3) {	/* VT330 (FIXME: what about VT220?) */
		    reply.a_param[count++] = 57;
#if OPT_DEC_LOCATOR
		    reply.a_param[count++] = 1;		/* mouse */
#else
		    reply.a_param[count++] = 0;		/* unknown */
#endif
		}
		break;
	    case 62:
		TRACE(("...request DECMSR - macro space\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 4) {	/* VT420 */
		    reply.a_pintro = 0;
		    reply.a_radix[count] = 16;	/* no data */
		    reply.a_param[count++] = 0;		/* no space for macros */
		    reply.a_inters = '*';
		    reply.a_final = L_CURL;
		}
		break;
	    case 63:
		TRACE(("...request DECCKSR - memory checksum\n"));
		/* DECCKSR - Memory checksum */
		if (sp->private_function
		    && screen->vtXX_level >= 4) {	/* VT420 */
		    init_reply(ANSI_DCS);
		    reply.a_param[count++] = (ParmType) GetParam(1);	/* PID */
		    reply.a_delim = "!~";	/* delimiter */
		    reply.a_radix[count] = 16;	/* use hex */
		    reply.a_param[count++] = 0;		/* no data */
		}
		break;
	    case 75:
		TRACE(("...request data integrity\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 4) {	/* VT420 */
		    reply.a_param[count++] = 70;	/* no errors */
		}
		break;
	    case 85:
		TRACE(("...request multi-session configuration\n"));
		if (sp->private_function
		    && screen->vtXX_level >= 4) {	/* VT420 */
		    reply.a_param[count++] = 83;	/* not configured */
		}
		break;
	    default:
		break;
	    }

	    if ((reply.a_nparam = (ParmType) count) != 0)
		unparseseq(xw, &reply);

	    ResetState(sp);
	    sp->private_function = False;
	    break;

	case CASE_MC:
	    TRACE(("CASE_MC - media control\n"));
	    xtermMediaControl(xw, GetParam(0), False);
	    ResetState(sp);
	    break;

	case CASE_DEC_MC:
	    TRACE(("CASE_DEC_MC - DEC media control\n"));
	    xtermMediaControl(xw, GetParam(0), True);
	    ResetState(sp);
	    break;

	case CASE_HP_MEM_LOCK:
	    /* FALLTHRU */
	case CASE_HP_MEM_UNLOCK:
	    TRACE(("%s\n", ((sp->parsestate[c] == CASE_HP_MEM_LOCK)
			    ? "CASE_HP_MEM_LOCK"
			    : "CASE_HP_MEM_UNLOCK")));
	    if (screen->scroll_amt)
		FlushScroll(xw);
	    if (sp->parsestate[c] == CASE_HP_MEM_LOCK)
		set_tb_margins(screen, screen->cur_row, screen->bot_marg);
	    else
		set_tb_margins(screen, 0, screen->bot_marg);
	    ResetState(sp);
	    break;

	case CASE_DECSTBM:
	    TRACE(("CASE_DECSTBM - set scrolling region\n"));
	    {
		int top;
		int bot;
		top = one_if_default(0);
		if (nparam < 2 || (bot = GetParam(1)) == DEFAULT
		    || bot > MaxRows(screen)
		    || bot == 0)
		    bot = MaxRows(screen);
		if (bot > top) {
		    if (screen->scroll_amt)
			FlushScroll(xw);
		    set_tb_margins(screen, top - 1, bot - 1);
		    CursorSet(screen, 0, 0, xw->flags);
		}
		ResetState(sp);
	    }
	    break;

	case CASE_DECREQTPARM:
	    TRACE(("CASE_DECREQTPARM\n"));
	    if (screen->terminal_id < 200) {	/* VT102 */
		value = zero_if_default(0);
		if (value == 0 || value == 1) {
		    init_reply(ANSI_CSI);
		    reply.a_pintro = 0;
		    reply.a_nparam = 7;
		    reply.a_param[0] = (ParmType) (value + 2);
		    reply.a_param[1] = 1;	/* no parity */
		    reply.a_param[2] = 1;	/* eight bits */
		    reply.a_param[3] = 128;	/* transmit 38.4k baud */
		    reply.a_param[4] = 128;	/* receive 38.4k baud */
		    reply.a_param[5] = 1;	/* clock multiplier ? */
		    reply.a_param[6] = 0;	/* STP flags ? */
		    reply.a_inters = 0;
		    reply.a_final = 'x';
		    unparseseq(xw, &reply);
		}
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSET:
	    /* DECSET */
#if OPT_VT52_MODE
	    if (screen->vtXX_level != 0)
#endif
		dpmodes(xw, bitset);
	    ResetState(sp);
#if OPT_TEK4014
	    if (TEK4014_ACTIVE(xw)) {
		TRACE(("Tek4014 is now active...\n"));
		return False;
	    }
#endif
	    break;

	case CASE_DECRST:
	    /* DECRST */
	    dpmodes(xw, bitclr);
	    init_groundtable(screen, sp);
	    ResetState(sp);
	    break;

	case CASE_DECALN:
	    TRACE(("CASE_DECALN - alignment test\n"));
	    if (screen->cursor_state)
		HideCursor();
	    reset_margins(screen);
	    CursorSet(screen, 0, 0, xw->flags);
	    xtermParseRect(xw, 0, 0, &myRect);
	    ScrnFillRectangle(xw, &myRect, 'E', 0, False);
	    ResetState(sp);
	    break;

	case CASE_GSETS:
	    TRACE(("CASE_GSETS(%d) = '%c'\n", sp->scstype, c));
	    decode_scs(xw, sp->scstype, 0, (int) c);
	    ResetState(sp);
	    break;

	case CASE_ANSI_SC:
	    if (IsLeftRightMode(xw)) {
		int left;
		int right;

		TRACE(("CASE_DECSLRM - set left and right margin\n"));
		left = one_if_default(0);
		if (nparam < 2 || (right = GetParam(1)) == DEFAULT
		    || right > MaxCols(screen)
		    || right == 0)
		    right = MaxCols(screen);
		if (right > left) {
		    set_lr_margins(screen, left - 1, right - 1);
		    CursorSet(screen, 0, 0, xw->flags);
		}
	    } else {
		TRACE(("CASE_ANSI_SC - save cursor\n"));
		CursorSave(xw);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSC:
	    TRACE(("CASE_DECSC - save cursor\n"));
	    CursorSave(xw);
	    ResetState(sp);
	    break;

	case CASE_ANSI_RC:
	    /* FALLTHRU */
	case CASE_DECRC:
	    TRACE(("CASE_%sRC - restore cursor\n",
		   (sp->nextstate == CASE_DECRC) ? "DEC" : "ANSI_"));
	    CursorRestore(xw);
	    if_OPT_ISO_COLORS(screen, {
		setExtendedFG(xw);
	    });
	    ResetState(sp);
	    break;

	case CASE_DECKPAM:
	    TRACE(("CASE_DECKPAM\n"));
	    xw->keyboard.flags |= MODE_DECKPAM;
	    update_appkeypad();
	    ResetState(sp);
	    break;

	case CASE_DECKPNM:
	    TRACE(("CASE_DECKPNM\n"));
	    UIntClr(xw->keyboard.flags, MODE_DECKPAM);
	    update_appkeypad();
	    ResetState(sp);
	    break;

	case CASE_CSI_QUOTE_STATE:
	    sp->parsestate = csi_quo_table;
	    break;

#if OPT_BLINK_CURS
	case CASE_CSI_SPACE_STATE:
	    sp->parsestate = csi_sp_table;
	    break;

	case CASE_DECSCUSR:
	    TRACE(("CASE_DECSCUSR\n"));
	    {
		Boolean change = True;
		int blinks = screen->cursor_blink_esc;

		HideCursor();

		switch (GetParam(0)) {
		case DEFAULT:
		    /* FALLTHRU */
		case DEFAULT_STYLE:
		    /* FALLTHRU */
		case BLINK_BLOCK:
		    blinks = True;
		    screen->cursor_shape = CURSOR_BLOCK;
		    break;
		case STEADY_BLOCK:
		    blinks = False;
		    screen->cursor_shape = CURSOR_BLOCK;
		    break;
		case BLINK_UNDERLINE:
		    blinks = True;
		    screen->cursor_shape = CURSOR_UNDERLINE;
		    break;
		case STEADY_UNDERLINE:
		    blinks = False;
		    screen->cursor_shape = CURSOR_UNDERLINE;
		    break;
		case BLINK_BAR:
		    blinks = True;
		    screen->cursor_shape = CURSOR_BAR;
		    break;
		case STEADY_BAR:
		    blinks = False;
		    screen->cursor_shape = CURSOR_BAR;
		    break;
		default:
		    change = False;
		    break;
		}
		TRACE(("cursor_shape:%d blinks:%s\n",
		       screen->cursor_shape, BtoS(blinks)));
		if (change) {
		    xtermSetCursorBox(screen);
		    screen->cursor_blink_esc = blinks;
		    UpdateCursorBlink(screen);
		}
	    }
	    ResetState(sp);
	    break;
#endif

#if OPT_SCROLL_LOCK
	case CASE_DECLL:
	    TRACE(("CASE_DECLL\n"));
	    if (nparam > 0) {
		for (count = 0; count < nparam; ++count) {
		    int op = zero_if_default(count);
		    switch (op) {
		    case 0:
		    case DEFAULT:
			xtermClearLEDs(screen);
			break;
		    case 1:
			/* FALLTHRU */
		    case 2:
			/* FALLTHRU */
		    case 3:
			xtermShowLED(screen,
				     (Cardinal) op,
				     True);
			break;
		    case 21:
			/* FALLTHRU */
		    case 22:
			/* FALLTHRU */
		    case 23:
			xtermShowLED(screen,
				     (Cardinal) (op - 20),
				     True);
			break;
		    }
		}
	    } else {
		xtermClearLEDs(screen);
	    }
	    ResetState(sp);
	    break;
#endif

#if OPT_VT52_MODE
	case CASE_VT52_FINISH:
	    TRACE(("CASE_VT52_FINISH terminal_id %d, vtXX_level %d\n",
		   screen->terminal_id,
		   screen->vtXX_level));
	    if (screen->terminal_id >= 100
		&& screen->vtXX_level == 0) {
		sp->groundtable =
		    sp->parsestate = ansi_table;
		/*
		 * On restore, the terminal does not recognize DECRQSS for
		 * DECSCL (per vttest).
		 */
		screen->vtXX_level = 1;
		screen->curgl = screen->vt52_save_curgl;
		screen->curgr = screen->vt52_save_curgr;
		screen->curss = screen->vt52_save_curss;
		memmove(screen->gsets, screen->vt52_save_gsets, sizeof(screen->gsets));
	    }
	    break;
#endif

	case CASE_ANSI_LEVEL_1:
	    TRACE(("CASE_ANSI_LEVEL_1\n"));
	    set_ansi_conformance(screen, 1);
	    ResetState(sp);
	    break;

	case CASE_ANSI_LEVEL_2:
	    TRACE(("CASE_ANSI_LEVEL_2\n"));
	    set_ansi_conformance(screen, 2);
	    ResetState(sp);
	    break;

	case CASE_ANSI_LEVEL_3:
	    TRACE(("CASE_ANSI_LEVEL_3\n"));
	    set_ansi_conformance(screen, 3);
	    ResetState(sp);
	    break;

	case CASE_DECSCL:
	    TRACE(("CASE_DECSCL(%d,%d)\n", GetParam(0), GetParam(1)));
	    /*
	     * This changes the emulation level, and is not recognized by
	     * VT100s.
	     */
	    if (screen->terminal_id >= 200) {
		/*
		 * Disallow unrecognized parameters, as well as attempts to set
		 * the operating level higher than the given terminal-id.
		 */
		if (GetParam(0) >= 61
		    && GetParam(0) <= 60 + (screen->terminal_id / 100)) {
		    int new_vtXX_level = GetParam(0) - 60;
		    int case_value = zero_if_default(1);
		    /*
		     * VT300, VT420, VT520 manuals claim that DECSCL does a
		     * hard reset (RIS).  VT220 manual states that it is a soft
		     * reset.  Perhaps both are right (unlikely).  Kermit says
		     * it's soft.
		     */
		    ReallyReset(xw, False, False);
		    init_parser(xw, sp);
		    screen->vtXX_level = new_vtXX_level;
		    if (new_vtXX_level > 1) {
			switch (case_value) {
			case 1:
			    show_8bit_control(False);
			    break;
			case 0:
			case 2:
			    show_8bit_control(True);
			    break;
			}
		    }
		}
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSCA:
	    TRACE(("CASE_DECSCA\n"));
	    screen->protected_mode = DEC_PROTECT;
	    if (GetParam(0) <= 0 || GetParam(0) == 2) {
		UIntClr(xw->flags, PROTECTED);
		TRACE(("...clear PROTECTED\n"));
	    } else if (GetParam(0) == 1) {
		xw->flags |= PROTECTED;
		TRACE(("...set PROTECTED\n"));
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSED:
	    TRACE(("CASE_DECSED\n"));
	    do_erase_display(xw, zero_if_default(0), DEC_PROTECT);
	    ResetState(sp);
	    break;

	case CASE_DECSEL:
	    TRACE(("CASE_DECSEL\n"));
	    do_erase_line(xw, zero_if_default(0), DEC_PROTECT);
	    ResetState(sp);
	    break;

	case CASE_GRAPHICS_ATTRIBUTES:
#if OPT_GRAPHICS
	    TRACE(("CASE_GRAPHICS_ATTRIBUTES\n"));
	    {
		/* request: item, action, value */
		/* reply: item, status, value */
		if (nparam != 3) {
		    TRACE(("DATA_ERROR: malformed CASE_GRAPHICS_ATTRIBUTES request with %d parameters\n", nparam));
		} else {
		    int status = 3;
		    int result = 0;
		    int result2 = 0;

		    TRACE(("CASE_GRAPHICS_ATTRIBUTES request: %d, %d, %d\n",
			   GetParam(0), GetParam(1), GetParam(2)));
		    switch (GetParam(0)) {
		    case 1:	/* color register count */
			switch (GetParam(1)) {
			case 1:	/* read */
			    status = 0;
			    result = (int) get_color_register_count(screen);
			    break;
			case 2:	/* reset */
			    screen->numcolorregisters = 0;
			    status = 0;
			    result = (int) get_color_register_count(screen);
			    break;
			case 3:	/* set */
			    if (GetParam(2) > 1 &&
				(unsigned) GetParam(2) <= MAX_COLOR_REGISTERS) {
				screen->numcolorregisters = GetParam(2);
				status = 0;
				result = (int) get_color_register_count(screen);
			    }
			    break;
			case 4:	/* read maximum */
			    status = 0;
			    result = MAX_COLOR_REGISTERS;
			    break;
			default:
			    TRACE(("DATA_ERROR: CASE_GRAPHICS_ATTRIBUTES color register count request with unknown action parameter: %d\n",
				   GetParam(1)));
			    status = 2;
			    break;
			}
			break;
		    case 2:	/* graphics geometry */
			switch (GetParam(1)) {
			case 1:	/* read */
			    status = 0;
			    result = screen->graphics_max_wide;
			    result2 = screen->graphics_max_high;
			    break;
			case 2:	/* reset */
			    /* FALLTHRU */
			case 3:	/* set */
			    /* FALLTHRU */
			case 4:	/* read maximum */
			    /* not implemented */
			    break;
			default:
			    TRACE(("DATA_ERROR: CASE_GRAPHICS_ATTRIBUTES graphics geometry request with unknown action parameter: %d\n",
				   GetParam(1)));
			    status = 2;
			    break;
			}
			break;
# if OPT_REGIS_GRAPHICS
		    case 3:	/* ReGIS geometry */
			switch (GetParam(1)) {
			case 1:	/* read */
			    status = 0;
			    result = screen->graphics_regis_def_wide;
			    result2 = screen->graphics_regis_def_high;
			    break;
			case 2:	/* reset */
			    /* FALLTHRU */
			case 3:	/* set */
			    /* FALLTHRU */
			case 4:	/* read maximum */
			    /* not implemented */
			    break;
			default:
			    TRACE(("DATA_ERROR: CASE_GRAPHICS_ATTRIBUTES ReGIS geometry request with unknown action parameter: %d\n",
				   GetParam(1)));
			    status = 2;
			    break;
			}
			break;
#endif
		    default:
			TRACE(("DATA_ERROR: CASE_GRAPHICS_ATTRIBUTES request with unknown item parameter: %d\n",
			       GetParam(0)));
			status = 1;
			break;
		    }

		    init_reply(ANSI_CSI);
		    reply.a_pintro = '?';
		    count = 0;
		    reply.a_param[count++] = (ParmType) GetParam(0);
		    reply.a_param[count++] = (ParmType) status;
		    reply.a_param[count++] = (ParmType) result;
		    if (GetParam(0) >= 2)
			reply.a_param[count++] = (ParmType) result2;
		    reply.a_nparam = (ParmType) count;
		    reply.a_inters = 0;
		    reply.a_final = 'S';
		    unparseseq(xw, &reply);
		}
	    }
#endif
	    ResetState(sp);
	    break;

	case CASE_ST:
	    TRACE(("CASE_ST: End of String (%lu bytes) (mode=%d)\n",
		   (unsigned long) sp->string_used,
		   sp->string_mode));
	    ResetState(sp);
	    if (!sp->string_used)
		break;
	    sp->string_area[--(sp->string_used)] = '\0';
	    switch (sp->string_mode) {
	    case ANSI_APC:
		/* ignored */
		break;
	    case ANSI_DCS:
		do_dcs(xw, sp->string_area, sp->string_used);
		break;
	    case ANSI_OSC:
		do_osc(xw, sp->string_area, sp->string_used, ANSI_ST);
		break;
	    case ANSI_PM:
		/* ignored */
		break;
	    case ANSI_SOS:
		/* ignored */
		break;
	    default:
		TRACE(("unknown mode\n"));
		break;
	    }
	    break;

	case CASE_SOS:
	    TRACE(("CASE_SOS: Start of String\n"));
	    if (ParseSOS(screen)) {
		sp->string_mode = ANSI_SOS;
		sp->parsestate = sos_table;
	    } else {
		illegal_parse(xw, c, sp);
	    }
	    break;

	case CASE_PM:
	    TRACE(("CASE_PM: Privacy Message\n"));
	    if (ParseSOS(screen)) {
		sp->string_mode = ANSI_PM;
		sp->parsestate = sos_table;
	    } else {
		illegal_parse(xw, c, sp);
	    }
	    break;

	case CASE_DCS:
	    TRACE(("CASE_DCS: Device Control String\n"));
	    sp->string_mode = ANSI_DCS;
	    sp->parsestate = sos_table;
	    break;

	case CASE_APC:
	    TRACE(("CASE_APC: Application Program Command\n"));
	    if (ParseSOS(screen)) {
		sp->string_mode = ANSI_APC;
		sp->parsestate = sos_table;
	    } else {
		illegal_parse(xw, c, sp);
	    }
	    break;

	case CASE_SPA:
	    TRACE(("CASE_SPA - start protected area\n"));
	    screen->protected_mode = ISO_PROTECT;
	    xw->flags |= PROTECTED;
	    ResetState(sp);
	    break;

	case CASE_EPA:
	    TRACE(("CASE_EPA - end protected area\n"));
	    UIntClr(xw->flags, PROTECTED);
	    ResetState(sp);
	    break;

	case CASE_SU:
	    TRACE(("CASE_SU - scroll up\n"));
	    xtermScroll(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_SL:		/* ISO 6429, non-DEC */
	    TRACE(("CASE_SL - scroll left\n"));
	    xtermScrollLR(xw, one_if_default(0), True);
	    ResetState(sp);
	    break;

	case CASE_SR:		/* ISO 6429, non-DEC */
	    TRACE(("CASE_SR - scroll right\n"));
	    xtermScrollLR(xw, one_if_default(0), False);
	    ResetState(sp);
	    break;

	case CASE_DECDC:
	    TRACE(("CASE_DC - delete column\n"));
	    if (screen->vtXX_level >= 4) {
		xtermColScroll(xw, one_if_default(0), True, screen->cur_col);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECIC:
	    TRACE(("CASE_IC - insert column\n"));
	    if (screen->vtXX_level >= 4) {
		xtermColScroll(xw, one_if_default(0), False, screen->cur_col);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECBI:
	    TRACE(("CASE_BI - back index\n"));
	    if (screen->vtXX_level >= 4) {
		xtermColIndex(xw, True);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECFI:
	    TRACE(("CASE_FI - forward index\n"));
	    if (screen->vtXX_level >= 4) {
		xtermColIndex(xw, False);
	    }
	    ResetState(sp);
	    break;

	case CASE_IND:
	    TRACE(("CASE_IND - index\n"));
	    xtermIndex(xw, 1);
	    do_xevents();
	    ResetState(sp);
	    break;

	case CASE_CPL:
	    TRACE(("CASE_CPL - cursor prev line\n"));
	    CursorPrevLine(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_CNL:
	    TRACE(("CASE_CNL - cursor next line\n"));
	    CursorNextLine(xw, one_if_default(0));
	    ResetState(sp);
	    break;

	case CASE_NEL:
	    TRACE(("CASE_NEL\n"));
	    xtermIndex(xw, 1);
	    CarriageReturn(xw);
	    ResetState(sp);
	    break;

	case CASE_HTS:
	    TRACE(("CASE_HTS - horizontal tab set\n"));
	    TabSet(xw->tabs, screen->cur_col);
	    ResetState(sp);
	    break;

	case CASE_RI:
	    TRACE(("CASE_RI - reverse index\n"));
	    RevIndex(xw, 1);
	    ResetState(sp);
	    break;

	case CASE_SS2:
	    TRACE(("CASE_SS2\n"));
	    screen->curss = 2;
	    ResetState(sp);
	    break;

	case CASE_SS3:
	    TRACE(("CASE_SS3\n"));
	    screen->curss = 3;
	    ResetState(sp);
	    break;

	case CASE_CSI_STATE:
	    /* enter csi state */
	    InitParams();
	    SetParam(nparam++, DEFAULT);
	    sp->parsestate = csi_table;
	    break;

	case CASE_ESC_SP_STATE:
	    /* esc space */
	    sp->parsestate = esc_sp_table;
	    break;

	case CASE_CSI_EX_STATE:
	    /* csi exclamation */
	    sp->parsestate = csi_ex_table;
	    break;

	case CASE_CSI_TICK_STATE:
	    /* csi tick (') */
	    sp->parsestate = csi_tick_table;
	    break;

#if OPT_DEC_LOCATOR
	case CASE_DECEFR:
	    TRACE(("CASE_DECEFR - Enable Filter Rectangle\n"));
	    if (okSendMousePos(xw) == DEC_LOCATOR) {
		MotionOff(screen, xw);
		if ((screen->loc_filter_top = GetParam(0)) < 1)
		    screen->loc_filter_top = LOC_FILTER_POS;
		if (nparam < 2
		    || (screen->loc_filter_left = GetParam(1)) < 1)
		    screen->loc_filter_left = LOC_FILTER_POS;
		if (nparam < 3
		    || (screen->loc_filter_bottom = GetParam(2)) < 1)
		    screen->loc_filter_bottom = LOC_FILTER_POS;
		if (nparam < 4
		    || (screen->loc_filter_right = GetParam(3)) < 1)
		    screen->loc_filter_right = LOC_FILTER_POS;
		InitLocatorFilter(xw);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECELR:
	    MotionOff(screen, xw);
	    if (GetParam(0) <= 0 || GetParam(0) > 2) {
		screen->send_mouse_pos = MOUSE_OFF;
		TRACE(("DECELR - Disable Locator Reports\n"));
	    } else {
		TRACE(("DECELR - Enable Locator Reports\n"));
		screen->send_mouse_pos = DEC_LOCATOR;
		xtermShowPointer(xw, True);
		if (GetParam(0) == 2) {
		    screen->locator_reset = True;
		} else {
		    screen->locator_reset = False;
		}
		if (nparam < 2 || GetParam(1) != 1) {
		    screen->locator_pixels = False;
		} else {
		    screen->locator_pixels = True;
		}
		screen->loc_filter = False;
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSLE:
	    TRACE(("DECSLE - Select Locator Events\n"));
	    for (count = 0; count < nparam; ++count) {
		switch (zero_if_default(count)) {
		case 0:
		    MotionOff(screen, xw);
		    screen->loc_filter = False;
		    screen->locator_events = 0;
		    break;
		case 1:
		    screen->locator_events |= LOC_BTNS_DN;
		    break;
		case 2:
		    UIntClr(screen->locator_events, LOC_BTNS_DN);
		    break;
		case 3:
		    screen->locator_events |= LOC_BTNS_UP;
		    break;
		case 4:
		    UIntClr(screen->locator_events, LOC_BTNS_UP);
		    break;
		}
	    }
	    ResetState(sp);
	    break;

	case CASE_DECRQLP:
	    TRACE(("DECRQLP - Request Locator Position\n"));
	    if (GetParam(0) < 2) {
		/* Issue DECLRP Locator Position Report */
		GetLocatorPosition(xw);
	    }
	    ResetState(sp);
	    break;
#endif /* OPT_DEC_LOCATOR */

#if OPT_DEC_RECTOPS
	case CASE_CSI_DOLLAR_STATE:
	    TRACE(("CASE_CSI_DOLLAR_STATE\n"));
	    /* csi dollar ($) */
	    if (screen->vtXX_level >= 3)
		sp->parsestate = csi_dollar_table;
	    else
		sp->parsestate = eigtable;
	    break;

	case CASE_CSI_STAR_STATE:
	    TRACE(("CASE_CSI_STAR_STATE\n"));
	    /* csi star (*) */
	    if (screen->vtXX_level >= 4)
		sp->parsestate = csi_star_table;
	    else
		sp->parsestate = eigtable;
	    break;

	case CASE_DECRQCRA:
	    if (screen->vtXX_level >= 4) {
		int checksum;

		TRACE(("CASE_DECRQCRA - Request checksum of rectangular area\n"));
		xtermCheckRect(xw, ParamPair(0), &checksum);
		init_reply(ANSI_DCS);
		count = 0;
		reply.a_param[count++] = (ParmType) GetParam(1);	/* PID */
		reply.a_delim = "!~";	/* delimiter */
		reply.a_radix[count] = 16;
		reply.a_param[count++] = (ParmType) checksum;
		reply.a_nparam = (ParmType) count;
		unparseseq(xw, &reply);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECCRA:
	    if (screen->vtXX_level >= 4) {
		TRACE(("CASE_DECCRA - Copy rectangular area\n"));
		xtermParseRect(xw, ParamPair(0), &myRect);
		ScrnCopyRectangle(xw, &myRect, ParamPair(5));
	    }
	    ResetState(sp);
	    break;

	case CASE_DECERA:
	    if (screen->vtXX_level >= 4) {
		TRACE(("CASE_DECERA - Erase rectangular area\n"));
		xtermParseRect(xw, ParamPair(0), &myRect);
		ScrnFillRectangle(xw, &myRect, ' ', 0, True);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECFRA:
	    if (screen->vtXX_level >= 4) {
		value = zero_if_default(0);

		TRACE(("CASE_DECFRA - Fill rectangular area\n"));
		if (nparam > 0
		    && ((value >= 32 && value <= 126)
			|| (value >= 160 && value <= 255))) {
		    xtermParseRect(xw, ParamPair(1), &myRect);
		    ScrnFillRectangle(xw, &myRect, value, xw->flags, True);
		}
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSERA:
	    if (screen->vtXX_level >= 4) {
		TRACE(("CASE_DECSERA - Selective erase rectangular area\n"));
		xtermParseRect(xw, ParamPair(0), &myRect);
		ScrnWipeRectangle(xw, &myRect);
	    }
	    ResetState(sp);
	    break;

	case CASE_DECSACE:
	    TRACE(("CASE_DECSACE - Select attribute change extent\n"));
	    screen->cur_decsace = zero_if_default(0);
	    ResetState(sp);
	    break;

	case CASE_DECCARA:
	    if (screen->vtXX_level >= 4) {
		TRACE(("CASE_DECCARA - Change attributes in rectangular area\n"));
		xtermParseRect(xw, ParamPair(0), &myRect);
		ScrnMarkRectangle(xw, &myRect, False, ParamPair(4));
	    }
	    ResetState(sp);
	    break;

	case CASE_DECRARA:
	    if (screen->vtXX_level >= 4) {
		TRACE(("CASE_DECRARA - Reverse attributes in rectangular area\n"));
		xtermParseRect(xw, ParamPair(0), &myRect);
		ScrnMarkRectangle(xw, &myRect, True, ParamPair(4));
	    }
	    ResetState(sp);
	    break;

	case CASE_RQM:
	    TRACE(("CASE_RQM\n"));
	    do_ansi_rqm(xw, ParamPair(0));
	    ResetState(sp);
	    break;

	case CASE_DECRQM:
	    TRACE(("CASE_DECRQM\n"));
	    do_dec_rqm(xw, ParamPair(0));
	    ResetState(sp);
	    break;

	case CASE_CSI_DEC_DOLLAR_STATE:
	    TRACE(("CASE_CSI_DEC_DOLLAR_STATE\n"));
	    /* csi ? dollar ($) */
	    sp->parsestate = csi_dec_dollar_table;
	    break;
#else
	case CASE_CSI_DOLLAR_STATE:
	    /* csi dollar ($) */
	    sp->parsestate = eigtable;
	    break;

	case CASE_CSI_STAR_STATE:
	    /* csi dollar (*) */
	    sp->parsestate = eigtable;
	    break;

	case CASE_CSI_DEC_DOLLAR_STATE:
	    /* csi ? dollar ($) */
	    sp->parsestate = eigtable;
	    break;
#endif /* OPT_DEC_RECTOPS */

	case CASE_S7C1T:
	    TRACE(("CASE_S7C1T\n"));
	    if (screen->vtXX_level >= 2) {
		show_8bit_control(False);
		ResetState(sp);
	    }
	    break;

	case CASE_S8C1T:
	    TRACE(("CASE_S8C1T\n"));
	    if (screen->vtXX_level >= 2) {
		show_8bit_control(True);
		ResetState(sp);
	    }
	    break;

	case CASE_OSC:
	    TRACE(("CASE_OSC: Operating System Command\n"));
	    sp->parsestate = sos_table;
	    sp->string_mode = ANSI_OSC;
	    break;

	case CASE_RIS:
	    TRACE(("CASE_RIS\n"));
	    VTReset(xw, True, True);
	    /* NOTREACHED */

	case CASE_DECSTR:
	    TRACE(("CASE_DECSTR\n"));
	    VTReset(xw, False, False);
	    /* NOTREACHED */

	case CASE_REP:
	    TRACE(("CASE_REP\n"));
	    if (sp->lastchar >= 0 &&
		sp->lastchar < 256 &&
		sp->groundtable[E2A(sp->lastchar)] == CASE_PRINT) {
		IChar repeated[2];
		count = one_if_default(0);
		repeated[0] = (IChar) sp->lastchar;
		while (count-- > 0) {
		    dotext(xw,
			   screen->gsets[(int) (screen->curgl)],
			   repeated, 1);
		}
	    }
	    ResetState(sp);
	    break;

	case CASE_LS2:
	    TRACE(("CASE_LS2\n"));
	    screen->curgl = 2;
	    ResetState(sp);
	    break;

	case CASE_LS3:
	    TRACE(("CASE_LS3\n"));
	    screen->curgl = 3;
	    ResetState(sp);
	    break;

	case CASE_LS3R:
	    TRACE(("CASE_LS3R\n"));
	    screen->curgr = 3;
	    ResetState(sp);
	    break;

	case CASE_LS2R:
	    TRACE(("CASE_LS2R\n"));
	    screen->curgr = 2;
	    ResetState(sp);
	    break;

	case CASE_LS1R:
	    TRACE(("CASE_LS1R\n"));
	    screen->curgr = 1;
	    ResetState(sp);
	    break;

	case CASE_XTERM_SAVE:
	    savemodes(xw);
	    ResetState(sp);
	    break;

	case CASE_XTERM_RESTORE:
	    restoremodes(xw);
	    ResetState(sp);
	    break;

	case CASE_XTERM_WINOPS:
	    TRACE(("CASE_XTERM_WINOPS\n"));
	    window_ops(xw);
	    ResetState(sp);
	    break;
#if OPT_WIDE_CHARS
	case CASE_ESC_PERCENT:
	    TRACE(("CASE_ESC_PERCENT\n"));
	    sp->parsestate = esc_pct_table;
	    break;

	case CASE_UTF8:
	    /* If we did not set UTF-8 mode from resource or the
	     * command-line, allow it to be enabled/disabled by
	     * control sequence.
	     */
	    TRACE(("CASE_UTF8 wide:%d, utf8:%d, req:%s\n",
		   screen->wide_chars,
		   screen->utf8_mode,
		   BtoS(c == 'G')));
	    if ((!screen->wide_chars) && (c == 'G')) {
		WriteNow();
		ChangeToWide(xw);
	    }
	    if (screen->wide_chars
		&& !screen->utf8_always) {
		switchPtyData(screen, c == 'G');
		TRACE(("UTF8 mode %s\n",
		       BtoS(screen->utf8_mode)));
	    } else {
		TRACE(("UTF8 mode NOT turned %s (%s)\n",
		       BtoS(c == 'G'),
		       (screen->utf8_mode == uAlways)
		       ? "UTF-8 mode set from command-line"
		       : "wideChars resource was not set"));
	    }
	    ResetState(sp);
	    break;

	case CASE_SCS_PERCENT:
	    TRACE(("CASE_SCS_PERCENT\n"));
	    sp->parsestate = scs_pct_table;
	    break;

	case CASE_GSETS_PERCENT:
	    TRACE(("CASE_GSETS_PERCENT(%d) = '%c'\n", sp->scstype, c));
	    decode_scs(xw, sp->scstype, '%', (int) c);
	    ResetState(sp);
	    break;
#endif
#if OPT_MOD_FKEYS
	case CASE_SET_MOD_FKEYS:
	    TRACE(("CASE_SET_MOD_FKEYS\n"));
	    if (nparam >= 1) {
		set_mod_fkeys(xw,
			      GetParam(0),
			      ((nparam > 1)
			       ? GetParam(1)
			       : DEFAULT),
			      True);
	    } else {
		for (value = 1; value <= 5; ++value)
		    set_mod_fkeys(xw, value, DEFAULT, True);
	    }
	    break;
	case CASE_SET_MOD_FKEYS0:
	    TRACE(("CASE_SET_MOD_FKEYS0\n"));
	    if (nparam >= 1 && GetParam(0) != DEFAULT) {
		set_mod_fkeys(xw, GetParam(0), -1, False);
	    } else {
		xw->keyboard.modify_now.function_keys = -1;
	    }
	    break;
#endif
	case CASE_HIDE_POINTER:
	    TRACE(("CASE_HIDE_POINTER\n"));
	    if (nparam >= 1 && GetParam(0) != DEFAULT) {
		screen->pointer_mode = GetParam(0);
	    } else {
		screen->pointer_mode = DEF_POINTER_MODE;
	    }
	    break;

	case CASE_SM_TITLE:
	    TRACE(("CASE_SM_TITLE\n"));
	    if (nparam >= 1) {
		int n;
		for (n = 0; n < nparam; ++n) {
		    if (GetParam(n) != DEFAULT)
			screen->title_modes |= (1 << GetParam(n));
		}
	    } else {
		screen->title_modes = DEF_TITLE_MODES;
	    }
	    TRACE(("...title_modes %#x\n", screen->title_modes));
	    break;

	case CASE_RM_TITLE:
	    TRACE(("CASE_RM_TITLE\n"));
	    if (nparam >= 1) {
		int n;
		for (n = 0; n < nparam; ++n) {
		    if (GetParam(n) != DEFAULT)
			screen->title_modes &= ~(1 << GetParam(n));
		}
	    } else {
		screen->title_modes = DEF_TITLE_MODES;
	    }
	    TRACE(("...title_modes %#x\n", screen->title_modes));
	    break;

	case CASE_CSI_IGNORE:
	    sp->parsestate = cigtable;
	    break;

	case CASE_DECSWBV:
	    TRACE(("CASE_DECSWBV\n"));
	    switch (zero_if_default(0)) {
	    case 2:
		/* FALLTHRU */
	    case 3:
		/* FALLTHRU */
	    case 4:
		screen->warningVolume = bvLow;
		break;
	    case 5:
		/* FALLTHRU */
	    case 6:
		/* FALLTHRU */
	    case 7:
		/* FALLTHRU */
	    case 8:
		screen->warningVolume = bvHigh;
		break;
	    default:
		screen->warningVolume = bvOff;
		break;
	    }
	    TRACE(("...warningVolume %d\n", screen->warningVolume));
	    ResetState(sp);
	    break;

	case CASE_DECSMBV:
	    TRACE(("CASE_DECSMBV\n"));
	    switch (zero_if_default(0)) {
	    case 2:
		/* FALLTHRU */
	    case 3:
		/* FALLTHRU */
	    case 4:
		screen->marginVolume = bvLow;
		break;
	    case 0:
		/* FALLTHRU */
	    case 5:
		/* FALLTHRU */
	    case 6:
		/* FALLTHRU */
	    case 7:
		/* FALLTHRU */
	    case 8:
		screen->marginVolume = bvHigh;
		break;
	    default:
		screen->marginVolume = bvOff;
		break;
	    }
	    TRACE(("...marginVolume %d\n", screen->marginVolume));
	    ResetState(sp);
	    break;
	}
	if (sp->parsestate == sp->groundtable)
	    sp->lastchar = thischar;
    } while (0);

#if OPT_WIDE_CHARS
    screen->utf8_inparse = (Boolean) ((screen->utf8_mode != uFalse)
				      && (sp->parsestate != sos_table));
#endif

    return True;
}

static void
VTparse(XtermWidget xw)
{
    /* We longjmp back to this point in VTReset() */
    (void) setjmp(vtjmpbuf);
    init_parser(xw, &myState);

    do {
    } while (doparsing(xw, doinput(), &myState));
}

static Char *v_buffer;		/* pointer to physical buffer */
static Char *v_bufstr = NULL;	/* beginning of area to write */
static Char *v_bufptr;		/* end of area to write */
static Char *v_bufend;		/* end of physical buffer */

/* Write data to the pty as typed by the user, pasted with the mouse,
   or generated by us in response to a query ESC sequence. */

void
v_write(int f, const Char *data, unsigned len)
{
    TRACE2(("v_write(%d:%s)\n", len, visibleChars(data, len)));
    if (v_bufstr == NULL) {
	if (len > 0) {
	    v_buffer = (Char *) XtMalloc((Cardinal) len);
	    v_bufstr = v_buffer;
	    v_bufptr = v_buffer;
	    v_bufend = v_buffer + len;
	}
	if (v_bufstr == NULL) {
	    return;
	}
    }
    if_DEBUG({
	fprintf(stderr, "v_write called with %u bytes (%ld left over)",
		len, (long) (v_bufptr - v_bufstr));
	if (len > 1 && len < 10)
	    fprintf(stderr, " \"%.*s\"", len, (const char *) data);
	fprintf(stderr, "\n");
    });

#ifdef VMS
    if ((1 << f) != pty_mask) {
	tt_write((const char *) data, len);
	return;
    }
#else /* VMS */
    if (!FD_ISSET(f, &pty_mask)) {
	IGNORE_RC(write(f, (const char *) data, (size_t) len));
	return;
    }
#endif /* VMS */

    /*
     * Append to the block we already have.
     * Always doing this simplifies the code, and
     * isn't too bad, either.  If this is a short
     * block, it isn't too expensive, and if this is
     * a long block, we won't be able to write it all
     * anyway.
     */

    if (len > 0) {
#if OPT_DABBREV
	TScreenOf(term)->dabbrev_working = False;	/* break dabbrev sequence */
#endif
	if (v_bufend < v_bufptr + len) {	/* we've run out of room */
	    if (v_bufstr != v_buffer) {
		/* there is unused space, move everything down */
		/* possibly overlapping memmove here */
		if_DEBUG({
		    fprintf(stderr, "moving data down %ld\n",
			    (long) (v_bufstr - v_buffer));
		});
		memmove(v_buffer, v_bufstr, (size_t) (v_bufptr - v_bufstr));
		v_bufptr -= v_bufstr - v_buffer;
		v_bufstr = v_buffer;
	    }
	    if (v_bufend < v_bufptr + len) {
		/* still won't fit: get more space */
		/* Don't use XtRealloc because an error is not fatal. */
		unsigned size = (unsigned) (v_bufptr - v_buffer);
		v_buffer = TypeRealloc(Char, size + len, v_buffer);
		if (v_buffer) {
		    if_DEBUG({
			fprintf(stderr, "expanded buffer to %u\n",
				size + len);
		    });
		    v_bufstr = v_buffer;
		    v_bufptr = v_buffer + size;
		    v_bufend = v_bufptr + len;
		} else {
		    /* no memory: ignore entire write request */
		    xtermWarning("cannot allocate buffer space\n");
		    v_buffer = v_bufstr;	/* restore clobbered pointer */
		}
	    }
	}
	if (v_bufend >= v_bufptr + len) {
	    /* new stuff will fit */
	    memmove(v_bufptr, data, (size_t) len);
	    v_bufptr += len;
	}
    }

    /*
     * Write out as much of the buffer as we can.
     * Be careful not to overflow the pty's input silo.
     * We are conservative here and only write
     * a small amount at a time.
     *
     * If we can't push all the data into the pty yet, we expect write
     * to return a non-negative number less than the length requested
     * (if some data written) or -1 and set errno to EAGAIN,
     * EWOULDBLOCK, or EINTR (if no data written).
     *
     * (Not all systems do this, sigh, so the code is actually
     * a little more forgiving.)
     */

#define MAX_PTY_WRITE 128	/* 1/2 POSIX minimum MAX_INPUT */

    if (v_bufptr > v_bufstr) {
	int riten;

#ifdef VMS
	riten = tt_write(v_bufstr,
			 ((v_bufptr - v_bufstr <= VMS_TERM_BUFFER_SIZE)
			  ? v_bufptr - v_bufstr
			  : VMS_TERM_BUFFER_SIZE));
	if (riten == 0)
	    return (riten);
#else /* VMS */
	riten = (int) write(f, v_bufstr,
			    (size_t) ((v_bufptr - v_bufstr <= MAX_PTY_WRITE)
				      ? v_bufptr - v_bufstr
				      : MAX_PTY_WRITE));
	if (riten < 0)
#endif /* VMS */
	{
	    if_DEBUG({
		perror("write");
	    });
	    riten = 0;
	}
	if_DEBUG({
	    fprintf(stderr, "write called with %ld, wrote %d\n",
		    ((long) ((v_bufptr - v_bufstr) <= MAX_PTY_WRITE)
		     ? (long) (v_bufptr - v_bufstr)
		     : MAX_PTY_WRITE),
		    riten);
	});
	v_bufstr += riten;
	if (v_bufstr >= v_bufptr)	/* we wrote it all */
	    v_bufstr = v_bufptr = v_buffer;
    }

    /*
     * If we have lots of unused memory allocated, return it
     */
    if (v_bufend - v_bufptr > 1024) {	/* arbitrary hysteresis */
	/* save pointers across realloc */
	int start = (int) (v_bufstr - v_buffer);
	int size = (int) (v_bufptr - v_buffer);
	unsigned allocsize = (unsigned) (size ? size : 1);

	v_buffer = TypeRealloc(Char, allocsize, v_buffer);
	if (v_buffer) {
	    v_bufstr = v_buffer + start;
	    v_bufptr = v_buffer + size;
	    v_bufend = v_buffer + allocsize;
	    if_DEBUG({
		fprintf(stderr, "shrunk buffer to %u\n", allocsize);
	    });
	} else {
	    /* should we print a warning if couldn't return memory? */
	    v_buffer = v_bufstr - start;	/* restore clobbered pointer */
	}
    }
}

static void
updateCursor(TScreen *screen)
{
    if (screen->cursor_set != screen->cursor_state) {
	if (screen->cursor_set)
	    ShowCursor();
	else
	    HideCursor();
    }
}

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
static void
reallyStopBlinking(TScreen *screen)
{
    if (screen->cursor_state == BLINKED_OFF) {
	/* force cursor to display if it is enabled */
	screen->cursor_state = !screen->cursor_set;
	updateCursor(screen);
	xevents();
    }
}
#endif

#ifdef VMS
#define	ptymask()	(v_bufptr > v_bufstr ? pty_mask : 0)

static void
in_put(XtermWidget xw)
{
    static PtySelect select_mask;
    static PtySelect write_mask;
    int update = VTbuffer->update;
    int size;

    int status;
    Dimension replyWidth, replyHeight;
    XtGeometryResult stat;

    TScreen *screen = TScreenOf(xw);
    char *cp;
    int i;

    select_mask = pty_mask;	/* force initial read */
    for (;;) {

	/* if the terminal changed size, resize the widget */
	if (tt_changed) {
	    tt_changed = False;

	    stat = REQ_RESIZE((Widget) xw,
			      ((Dimension) FontWidth(screen)
			       * (tt_width)
			       + 2 * screen->border
			       + screen->fullVwin.sb_info.width),
			      ((Dimension) FontHeight(screen)
			       * (tt_length)
			       + 2 * screen->border),
			      &replyWidth, &replyHeight);

	    if (stat == XtGeometryYes || stat == XtGeometryDone) {
		xw->core.width = replyWidth;
		xw->core.height = replyHeight;

		ScreenResize(xw, replyWidth, replyHeight, &xw->flags);
	    }
	    repairSizeHints();
	}

	if (screen->eventMode == NORMAL
	    && readPtyData(xw, &select_mask, VTbuffer)) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		/* Scroll to bottom */
		WindowScroll(xw, 0, False);
	    break;
	}
	if (screen->scroll_amt)
	    FlushScroll(xw);
	if (screen->cursor_set && CursorMoved(screen)) {
	    if (screen->cursor_state)
		HideCursor();
	    ShowCursor();
#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
	    PreeditPosition(xw);
#endif
	} else {
	    updateCursor(screen);
	}

	if (QLength(screen->display)) {
	    select_mask = X_mask;
	} else {
	    write_mask = ptymask();
	    XFlush(screen->display);
	    select_mask = Select_mask;
	    if (screen->eventMode != NORMAL)
		select_mask = X_mask;
	}
	if (write_mask & ptymask()) {
	    v_write(screen->respond, 0, 0);	/* flush buffer */
	}

	if (select_mask & X_mask) {
	    xevents();
	    if (VTbuffer->update != update)
		break;
	}
    }
}
#else /* VMS */

static void
in_put(XtermWidget xw)
{
    static PtySelect select_mask;
    static PtySelect write_mask;

    TScreen *screen = TScreenOf(xw);
    int i;
    int update = VTbuffer->update;
#if OPT_DOUBLE_BUFFER
    int should_wait = 1;
#endif

    static struct timeval select_timeout;

#if OPT_BLINK_CURS
    /*
     * Compute the timeout for the blinking cursor to be much smaller than
     * the "on" or "off" interval.
     */
    int tick = ((screen->blink_on < screen->blink_off)
		? screen->blink_on
		: screen->blink_off);
    tick *= (1000 / 8);		/* 1000 for msec/usec, 8 for "much" smaller */
    if (tick < 1)
	tick = 1;
#endif

    for (;;) {
	int size;
	int time_select;

	if (screen->eventMode == NORMAL
	    && (size = readPtyData(xw, &select_mask, VTbuffer)) != 0) {
	    if (screen->scrollWidget
		&& screen->scrollttyoutput
		&& screen->topline < 0)
		WindowScroll(xw, 0, False);	/* Scroll to bottom */
	    /* stop speed reading at some point to look for X stuff */
	    TRACE(("VTbuffer uses %ld/%d\n",
		   (long) (VTbuffer->last - VTbuffer->buffer),
		   BUF_SIZE));
	    if ((VTbuffer->last - VTbuffer->buffer) > BUF_SIZE) {
		FD_CLR(screen->respond, &select_mask);
		break;
	    }
#if OPT_DOUBLE_BUFFER
	    if (should_wait) {
		/* wait 25 msec for potential extra data (avoids some bogus flickering) */
		/* that's only 40 FPS but hey, it's still lower than the input lag on some consoles! :) */
		usleep(25000);
		should_wait = 0;
	    }
	    select_timeout.tv_sec = 0;
	    i = Select(max_plus1, &select_mask, &write_mask, 0,
		       &select_timeout);
	    if (i > 0 && FD_ISSET(screen->respond, &select_mask))
		continue;
	    else
		break;
#elif defined(HAVE_SCHED_YIELD)
	    /*
	     * If we've read a full (small/fragment) buffer, let the operating
	     * system have a turn, and we'll resume reading until we've either
	     * read only a fragment of the buffer, or we've filled the large
	     * buffer (see above).  Doing this helps keep up with large bursts
	     * of output.
	     */
	    if (size == FRG_SIZE) {
		select_timeout.tv_sec = 0;
		i = Select(max_plus1, &select_mask, &write_mask, 0,
			   &select_timeout);
		if (i > 0 && FD_ISSET(screen->respond, &select_mask)) {
		    sched_yield();
		} else
		    break;
	    } else {
		break;
	    }
#else
	    (void) size;	/* unused in this branch */
	    break;
#endif
	}
	/* update the screen */
	if (screen->scroll_amt)
	    FlushScroll(xw);
	if (screen->cursor_set && CursorMoved(screen)) {
	    if (screen->cursor_state)
		HideCursor();
	    ShowCursor();
#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
	    PreeditPosition(xw);
#endif
	} else {
	    updateCursor(screen);
	}

	XFlush(screen->display);	/* always flush writes before waiting */

	/* Update the masks and, unless X events are already in the queue,
	   wait for I/O to be possible. */
	XFD_COPYSET(&Select_mask, &select_mask);
	/* in selection mode xterm does not read pty */
	if (screen->eventMode != NORMAL)
	    FD_CLR(screen->respond, &select_mask);

	if (v_bufptr > v_bufstr) {
	    XFD_COPYSET(&pty_mask, &write_mask);
	} else
	    FD_ZERO(&write_mask);
	select_timeout.tv_sec = 0;
	time_select = 0;

	/*
	 * if there's either an XEvent or an XtTimeout pending, just take
	 * a quick peek, i.e. timeout from the select() immediately.  If
	 * there's nothing pending, let select() block a little while, but
	 * for a shorter interval than the arrow-style scrollbar timeout.
	 * The blocking is optional, because it tends to increase the load
	 * on the host.
	 */
	if (xtermAppPending()) {
	    select_timeout.tv_usec = 0;
	    time_select = 1;
	} else if (screen->awaitInput) {
	    select_timeout.tv_usec = 50000;
	    time_select = 1;
#if OPT_BLINK_CURS
	} else if ((screen->blink_timer != 0 &&
		    ((screen->select & FOCUS) || screen->always_highlight)) ||
		   (screen->cursor_state == BLINKED_OFF)) {
	    select_timeout.tv_usec = tick;
	    while (select_timeout.tv_usec > 1000000) {
		select_timeout.tv_usec -= 1000000;
		select_timeout.tv_sec++;
	    }
	    time_select = 1;
#endif
#if OPT_SESSION_MGT
	} else if (resource.sessionMgt) {
	    if (ice_fd >= 0)
		FD_SET(ice_fd, &select_mask);
#endif
	}
	if (need_cleanup)
	    NormalExit();
#if OPT_DOUBLE_BUFFER
	if (screen->needSwap) {
	    XdbeSwapInfo swap;
	    swap.swap_window = VWindow(screen);
	    swap.swap_action = XdbeCopied;
	    XdbeSwapBuffers(XtDisplay(term), &swap, 1);
	    XFlush(XtDisplay(xw));
	    screen->needSwap = 0;
	}
#endif
	i = Select(max_plus1, &select_mask, &write_mask, 0,
		   (time_select ? &select_timeout : 0));
	if (i < 0) {
	    if (errno != EINTR)
		SysError(ERROR_SELECT);
	    continue;
	}

	/* if there is room to write more data to the pty, go write more */
	if (FD_ISSET(screen->respond, &write_mask)) {
	    v_write(screen->respond, (Char *) 0, 0);	/* flush buffer */
	}

	/* if there are X events already in our queue, it
	   counts as being readable */
	if (xtermAppPending() ||
	    FD_ISSET(ConnectionNumber(screen->display), &select_mask)) {
	    xevents();
	    if (VTbuffer->update != update)	/* HandleInterpret */
		break;
	}

    }
}
#endif /* VMS */

static IChar
doinput(void)
{
    TScreen *screen = TScreenOf(term);

    while (!morePtyData(screen, VTbuffer))
	in_put(term);
    return nextPtyData(screen, VTbuffer);
}

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
/*
 *  For OverTheSpot, client has to inform the position for XIM preedit.
 */
static void
PreeditPosition(XtermWidget xw)
{
    TInput *input = lookupTInput(xw, (Widget) xw);
    TScreen *screen = TScreenOf(xw);
    CLineData *ld;
    XPoint spot;
    XVaNestedList list;

    if (input && input->xic
	&& (ld = getLineData(screen, screen->cur_row)) != 0) {
	spot.x = (short) LineCursorX(screen, ld, screen->cur_col);
	spot.y = (short) (CursorY(screen, screen->cur_row) + xw->work.xim_fs_ascent);
	list = XVaCreateNestedList(0,
				   XNSpotLocation, &spot,
				   XNForeground, T_COLOR(screen, TEXT_FG),
				   XNBackground, T_COLOR(screen, TEXT_BG),
				   (void *) 0);
	XSetICValues(input->xic, XNPreeditAttributes, list, (void *) 0);
	XFree(list);
    }
}
#endif

static void
WrapLine(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    LineData *ld = getLineData(screen, screen->cur_row);

    if (ld != 0) {
	/* mark that we had to wrap this line */
	LineSetFlag(ld, LINEWRAPPED);
	if (screen->show_wrap_marks) {
	    ShowWrapMarks(xw, screen->cur_row, ld);
	}
	xtermAutoPrint(xw, '\n');
	xtermIndex(xw, 1);
	set_cur_col(screen, ScrnLeftMargin(xw));
    }
}

/*
 * process a string of characters according to the character set indicated
 * by charset.  worry about end of line conditions (wraparound if selected).
 */
void
dotext(XtermWidget xw,
       int charset,
       IChar *buf,		/* start of characters to process */
       Cardinal len)		/* end */
{
    TScreen *screen = TScreenOf(xw);
#if OPT_WIDE_CHARS
    Cardinal chars_chomped = 1;
    int next_col = screen->cur_col;
#else
    int next_col, this_col;	/* must be signed */
#endif
    Cardinal offset;
    int right = ScrnRightMargin(xw);

    /*
     * It is possible to use CUP, etc., to move outside margins.  In that
     * case, the right-margin is ineffective.
     */
    if (screen->cur_col > right) {
	right = screen->max_col;
    }
#if OPT_WIDE_CHARS
    if (screen->vt100_graphics)
#endif
	if (!xtermCharSetOut(xw, buf, buf + len, charset))
	    return;

    if_OPT_XMC_GLITCH(screen, {
	Cardinal n;
	if (charset != '?') {
	    for (n = 0; n < len; n++) {
		if (buf[n] == XMC_GLITCH)
		    buf[n] = XMC_GLITCH + 1;
	    }
	}
    });

#if OPT_WIDE_CHARS
    for (offset = 0;
	 offset < len && (chars_chomped > 0 || screen->do_wrap);
	 offset += chars_chomped) {
	int width_available = right + 1 - screen->cur_col;
	int width_here = 0;
	Boolean force_wrap;
	Boolean need_wrap;
	Boolean did_wrap;
	int last_chomp = 0;
	chars_chomped = 0;

	do {
	    force_wrap = False;
	    need_wrap = False;
	    did_wrap = False;

	    if (screen->do_wrap) {
		screen->do_wrap = False;
		if ((xw->flags & WRAPAROUND)) {
		    WrapLine(xw);
		    width_available = right + 1 - screen->cur_col;
		    next_col = screen->cur_col;
		    did_wrap = True;
		}
	    }

	    /*
	     * This can happen with left/right margins...
	     */
	    if (width_available <= 0) {
		break;
	    }

	    /*
	     * Regarding the soft-hyphen aberration, see
	     * http://archives.miloush.net/michkap/archive/2006/09/02/736881.html
	     */
	    while (width_here <= width_available
		   && chars_chomped < (len - offset)) {
		Cardinal n = chars_chomped + offset;
		if (!screen->utf8_mode
		    || (screen->vt100_graphics && charset == '0')) {
		    last_chomp = 1;
		} else if (screen->c1_printable &&
			   buf[n] >= 0x80 &&
			   buf[n] <= 0xa0) {
		    last_chomp = 1;
		} else {
		    last_chomp = my_wcwidth((wchar_t) buf[n]);
		    if (last_chomp <= 0) {
			IChar ch = buf[n];
			Bool eat_it = (ch > 127);
			if (ch == 0xad) {
			    /*
			     * Only display soft-hyphen if it happens to be at
			     * the right-margin.  While that means that only
			     * the displayed character could be selected for
			     * pasting, a well-behaved application would never
			     * send this, anyway...
			     */
			    if (width_here < width_available - 1) {
				eat_it = True;
			    } else {
				last_chomp = 1;
				eat_it = False;
			    }
			    TRACE(("...will%s display soft-hyphen\n",
				   eat_it ? " not" : ""));
			}
			/*
			 * Supposedly we dealt with combining characters and
			 * control characters in doparse().  Anything left over
			 * is junk that we will not attempt to display.
			 */
			if (eat_it) {
			    TRACE(("...will not display U+%04X\n", ch));
			    --len;
			    while (n < len) {
				buf[n] = buf[n + 1];
				++n;
			    }
			    last_chomp = 0;
			    chars_chomped--;
			}
		    }
		}
		width_here += last_chomp;
		chars_chomped++;
	    }

	    if (width_here > width_available) {
		if (last_chomp > right + 1) {
		    break;	/* give up - it is too big */
		} else if (chars_chomped-- == 0) {
		    /* This can happen with left/right margins... */
		    break;
		}
		width_here -= last_chomp;
		if (chars_chomped > 0) {
		    if (!(xw->flags & WRAPAROUND)) {
			buf[chars_chomped + offset - 1] = buf[len - 1];
		    } else {
			need_wrap = True;
		    }
		}
	    } else if (width_here == width_available) {
		need_wrap = True;
	    } else if (chars_chomped != (len - offset)) {
		need_wrap = True;
	    }

	    if (chars_chomped != 0 && next_col <= screen->max_col) {
		WriteText(xw, buf + offset, chars_chomped);
	    } else if (!did_wrap
		       && len > 0
		       && (xw->flags & WRAPAROUND)
		       && screen->cur_col > ScrnLeftMargin(xw)) {
		force_wrap = True;
		need_wrap = True;
	    }
	    next_col += width_here;
	    screen->do_wrap = need_wrap;
	} while (force_wrap);
    }

    /*
     * Remember that we wrote something to the screen, for use as a base of
     * combining characters.  The logic above may have called cursor-forward
     * or carriage-return operations which resets this flag, so we set it at
     * the very end.
     */
    screen->char_was_written = True;
#else /* ! OPT_WIDE_CHARS */

    for (offset = 0; offset < len; offset += (Cardinal) this_col) {
#if OPT_DEC_CHRSET
	CLineData *ld = getLineData(screen, screen->cur_row);
#endif

	int last_col = LineMaxCol(screen, ld);
	if (last_col > (right + 1))
	    last_col = right + 1;
	this_col = last_col - screen->cur_col + 1;
	if (this_col <= 1) {
	    if (screen->do_wrap) {
		screen->do_wrap = False;
		if ((xw->flags & WRAPAROUND)) {
		    WrapLine(xw);
		}
	    }
	    this_col = 1;
	}
	if (offset + (Cardinal) this_col > len) {
	    this_col = (int) (len - offset);
	}
	next_col = screen->cur_col + this_col;

	WriteText(xw, buf + offset, (unsigned) this_col);

	/*
	 * The call to WriteText updates screen->cur_col.
	 * If screen->cur_col is less than next_col, we must have
	 * hit the right margin - so set the do_wrap flag.
	 */
	screen->do_wrap = (Boolean) (screen->cur_col < next_col);
    }

#endif /* OPT_WIDE_CHARS */
}

#if OPT_WIDE_CHARS
unsigned
visual_width(const IChar *str, Cardinal len)
{
    /* returns the visual width of a string (doublewide characters count
       as 2, normalwide characters count as 1) */
    unsigned my_len = 0;
    while (len) {
	int ch = (int) *str++;
	if (isWide(ch))
	    my_len += 2;
	else
	    my_len++;
	len--;
    }
    return my_len;
}
#endif

#if HANDLE_STRUCT_NOTIFY
/* Flag icon name with "***" on window output when iconified.
 */
static void
HandleStructNotify(Widget w GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XEvent *event,
		   Boolean *cont GCC_UNUSED)
{
    XtermWidget xw = term;

    switch (event->type) {
    case MapNotify:
	TRACE(("HandleStructNotify(MapNotify) %#lx\n", event->xmap.window));
	resetZIconBeep(xw);
	mapstate = !IsUnmapped;
	break;
    case UnmapNotify:
	TRACE(("HandleStructNotify(UnmapNotify) %#lx\n", event->xunmap.window));
	mapstate = IsUnmapped;
	break;
    case ConfigureNotify:
	if (event->xconfigure.window == XtWindow(toplevel)) {
#if !OPT_TOOLBAR
	    int height, width;

	    height = event->xconfigure.height;
	    width = event->xconfigure.width;
#endif
	    TRACE(("HandleStructNotify(ConfigureNotify) %#lx %d,%d %dx%d\n",
		   event->xconfigure.window,
		   event->xconfigure.y, event->xconfigure.x,
		   event->xconfigure.height, event->xconfigure.width));

#if OPT_TOOLBAR
	    /*
	     * The notification is for the top-level widget, but we care about
	     * vt100 (ignore the tek4014 window).
	     */
	    if (TScreenOf(xw)->Vshow) {
		VTwin *Vwin = WhichVWin(TScreenOf(xw));
		TbInfo *info = &(Vwin->tb_info);
		TbInfo save = *info;

		if (info->menu_bar) {
		    XtVaGetValues(info->menu_bar,
				  XtNheight, &info->menu_height,
				  XtNborderWidth, &info->menu_border,
				  (XtPointer) 0);

		    if (save.menu_height != info->menu_height
			|| save.menu_border != info->menu_border) {

			TRACE(("...menu_height %d\n", info->menu_height));
			TRACE(("...menu_border %d\n", info->menu_border));
			TRACE(("...had height  %d, border %d\n",
			       save.menu_height,
			       save.menu_border));

			/*
			 * Window manager still may be using the old values.
			 * Try to fool it.
			 */
			REQ_RESIZE((Widget) xw,
				   TScreenOf(xw)->fullVwin.fullwidth,
				   (Dimension) (info->menu_height
						- save.menu_height
						+ TScreenOf(xw)->fullVwin.fullheight),
				   NULL, NULL);
			repairSizeHints();
		    }
		}
	    }
#else
	    if (height != xw->hints.height || width != xw->hints.width)
		RequestResize(xw, height, width, False);
#endif /* OPT_TOOLBAR */
	}
	break;
    case ReparentNotify:
	TRACE(("HandleStructNotify(ReparentNotify) %#lx\n", event->xreparent.window));
	break;
    default:
	TRACE(("HandleStructNotify(event %s) %#lx\n",
	       visibleEventType(event->type),
	       event->xany.window));
	break;
    }
}
#endif /* HANDLE_STRUCT_NOTIFY */

#if OPT_BLINK_CURS
static int
DoStartBlinking(TScreen *screen)
{
    int actual = ((screen->cursor_blink == cbTrue ||
		   screen->cursor_blink == cbAlways)
		  ? 1
		  : 0);
    int wanted = screen->cursor_blink_esc ? 1 : 0;
    int result;
    if (screen->cursor_blink_xor) {
	result = actual ^ wanted;
    } else {
	result = actual | wanted;
    }
    return result;
}

static void
SetCursorBlink(TScreen *screen, BlinkOps enable)
{
    if (SettableCursorBlink(screen)) {
	screen->cursor_blink = enable;
    }
    if (DoStartBlinking(screen)) {
	StartBlinking(screen);
    } else {
	/* EMPTY */
#if OPT_BLINK_TEXT
	reallyStopBlinking(screen);
#else
	StopBlinking(screen);
#endif
    }
    update_cursorblink();
}

void
ToggleCursorBlink(TScreen *screen)
{
    if (screen->cursor_blink == cbTrue) {
	SetCursorBlink(screen, cbFalse);
    } else if (screen->cursor_blink == cbFalse) {
	SetCursorBlink(screen, cbTrue);
    }
}
#endif

/*
 * process ANSI modes set, reset
 */
static void
ansi_modes(XtermWidget xw, BitFunc func)
{
    int i;

    for (i = 0; i < nparam; ++i) {
	switch (GetParam(i)) {
	case 2:		/* KAM (if set, keyboard locked */
	    (*func) (&xw->keyboard.flags, MODE_KAM);
	    break;

	case 4:		/* IRM                          */
	    (*func) (&xw->flags, INSERT);
	    break;

	case 12:		/* SRM (if set, local echo      */
	    (*func) (&xw->keyboard.flags, MODE_SRM);
	    break;

	case 20:		/* LNM                          */
	    (*func) (&xw->flags, LINEFEED);
	    update_autolinefeed();
	    break;
	}
    }
}

#define IsSM() (func == bitset)

#define set_bool_mode(flag) \
	flag = (Boolean) IsSM()

static void
really_set_mousemode(XtermWidget xw,
		     Bool enabled,
		     XtermMouseModes mode)
{
    TScreenOf(xw)->send_mouse_pos = enabled ? mode : MOUSE_OFF;
    if (okSendMousePos(xw) != MOUSE_OFF)
	xtermShowPointer(xw, True);
}

#define set_mousemode(mode) really_set_mousemode(xw, IsSM(), mode)

#if OPT_READLINE
#define set_mouseflag(f)		\
	(IsSM()				\
	 ? SCREEN_FLAG_set(screen, f)	\
	 : SCREEN_FLAG_unset(screen, f))
#endif

/*
 * process DEC private modes set, reset
 */
static void
dpmodes(XtermWidget xw, BitFunc func)
{
    TScreen *screen = TScreenOf(xw);
    int i, j;
    unsigned myflags;

    TRACE(("changing %d DEC private modes\n", nparam));
    for (i = 0; i < nparam; ++i) {
	int code = GetParam(i);

	TRACE(("%s %d\n", IsSM()? "DECSET" : "DECRST", code));
	switch ((DECSET_codes) code) {
	case srm_DECCKM:
	    (*func) (&xw->keyboard.flags, MODE_DECCKM);
	    update_appcursor();
	    break;
	case srm_DECANM:	/* ANSI/VT52 mode      */
	    if (IsSM()) {	/* ANSI (VT100) */
		/*
		 * Setting DECANM should have no effect, since this function
		 * cannot be reached from vt52 mode.
		 */
		/* EMPTY */ ;
	    }
#if OPT_VT52_MODE
	    else if (screen->terminal_id >= 100) {	/* VT52 */
		TRACE(("DECANM terminal_id %d, vtXX_level %d\n",
		       screen->terminal_id,
		       screen->vtXX_level));
		screen->vtXX_level = 0;
		screen->vt52_save_curgl = screen->curgl;
		screen->vt52_save_curgr = screen->curgr;
		screen->vt52_save_curss = screen->curss;
		memmove(screen->vt52_save_gsets, screen->gsets, sizeof(screen->gsets));
		resetCharsets(screen);
		InitParams();	/* ignore the remaining params, if any */
	    }
#endif
	    break;
	case srm_DECCOLM:
	    if (screen->c132) {
		if (!(xw->flags & NOCLEAR_COLM))
		    ClearScreen(xw);
		CursorSet(screen, 0, 0, xw->flags);
		if ((j = IsSM()? 132 : 80) !=
		    ((xw->flags & IN132COLUMNS) ? 132 : 80) ||
		    j != MaxCols(screen))
		    RequestResize(xw, -1, j, True);
		(*func) (&xw->flags, IN132COLUMNS);
		if (xw->flags & IN132COLUMNS) {
		    UIntClr(xw->flags, LEFT_RIGHT);
		    reset_lr_margins(screen);
		}
	    }
	    break;
	case srm_DECSCLM:	/* (slow scroll)        */
	    if (IsSM()) {
		screen->jumpscroll = 0;
		if (screen->scroll_amt)
		    FlushScroll(xw);
	    } else
		screen->jumpscroll = 1;
	    (*func) (&xw->flags, SMOOTHSCROLL);
	    update_jumpscroll();
	    break;
	case srm_DECSCNM:
	    myflags = xw->flags;
	    (*func) (&xw->flags, REVERSE_VIDEO);
	    if ((xw->flags ^ myflags) & REVERSE_VIDEO)
		ReverseVideo(xw);
	    /* update_reversevideo done in RevVid */
	    break;

	case srm_DECOM:
	    (*func) (&xw->flags, ORIGIN);
	    CursorSet(screen, 0, 0, xw->flags);
	    break;

	case srm_DECAWM:
	    (*func) (&xw->flags, WRAPAROUND);
	    update_autowrap();
	    break;
	case srm_DECARM:
	    /* ignore autorepeat
	     * XAutoRepeatOn() and XAutoRepeatOff() can do this, but only
	     * for the whole display - not limited to a given window.
	     */
	    break;
	case srm_X10_MOUSE:	/* MIT bogus sequence           */
	    MotionOff(screen, xw);
	    set_mousemode(X10_MOUSE);
	    break;
#if OPT_TOOLBAR
	case srm_RXVT_TOOLBAR:
	    ShowToolbar(IsSM());
	    break;
#endif
#if OPT_BLINK_CURS
	case srm_ATT610_BLINK:	/* AT&T 610: Start/stop blinking cursor */
	    if (SettableCursorBlink(screen)) {
		set_bool_mode(screen->cursor_blink_esc);
		UpdateCursorBlink(screen);
	    }
	    break;
	case srm_CURSOR_BLINK_OPS:
	    /* intentionally ignored (this is user-preference) */
	    break;
	case srm_XOR_CURSOR_BLINKS:
	    /* intentionally ignored (this is user-preference) */
	    break;
#endif
	case srm_DECPFF:	/* print form feed */
	    set_bool_mode(PrinterOf(screen).printer_formfeed);
	    break;
	case srm_DECPEX:	/* print extent */
	    set_bool_mode(PrinterOf(screen).printer_extent);
	    break;
	case srm_DECTCEM:	/* Show/hide cursor (VT200) */
	    set_bool_mode(screen->cursor_set);
	    break;
	case srm_RXVT_SCROLLBAR:
	    if (screen->fullVwin.sb_info.width != (IsSM()? ON : OFF))
		ToggleScrollBar(xw);
	    break;
#if OPT_SHIFT_FONTS
	case srm_RXVT_FONTSIZE:
	    set_bool_mode(xw->misc.shift_fonts);
	    break;
#endif
#if OPT_TEK4014
	case srm_DECTEK:
	    if (IsSM() && !(screen->inhibit & I_TEK)) {
		FlushLog(xw);
		TEK4014_ACTIVE(xw) = True;
		TRACE(("Tek4014 is now active...\n"));
		update_vttekmode();
	    }
	    break;
#endif
	case srm_132COLS:	/* 132 column mode              */
	    set_bool_mode(screen->c132);
	    update_allow132();
	    break;
	case srm_CURSES_HACK:
	    set_bool_mode(screen->curses);
	    update_cursesemul();
	    break;
	case srm_DECNRCM:	/* national charset (VT220) */
	    if (screen->vtXX_level >= 2) {
		if ((*func) (&xw->flags, NATIONAL)) {
		    modified_DECNRCM(xw);
		}
	    }
	    break;
	case srm_MARGIN_BELL:	/* margin bell                  */
	    set_bool_mode(screen->marginbell);
	    if (!screen->marginbell)
		screen->bellArmed = -1;
	    update_marginbell();
	    break;
	case srm_REVERSEWRAP:	/* reverse wraparound   */
	    (*func) (&xw->flags, REVERSEWRAP);
	    update_reversewrap();
	    break;
#ifdef ALLOWLOGGING
	case srm_ALLOWLOGGING:	/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    /*
	     * if this feature is enabled, logging may be
	     * enabled and disabled via escape sequences.
	     */
	    if (IsSM())
		StartLog(xw);
	    else
		CloseLog(xw);
#else
	    Bell(xw, XkbBI_Info, 0);
	    Bell(xw, XkbBI_Info, 0);
#endif /* ALLOWLOGFILEONOFF */
	    break;
#endif
	case srm_OPT_ALTBUF_CURSOR:	/* alternate buffer & cursor */
	    if (!xw->misc.titeInhibit) {
		if (IsSM()) {
		    CursorSave(xw);
		    ToAlternate(xw, True);
		    ClearScreen(xw);
		} else {
		    FromAlternate(xw);
		    CursorRestore(xw);
		}
	    } else if (IsSM()) {
		do_ti_xtra_scroll(xw);
	    }
	    break;
	case srm_OPT_ALTBUF:
	    /* FALLTHRU */
	case srm_ALTBUF:	/* alternate buffer */
	    if (!xw->misc.titeInhibit) {
		if (IsSM()) {
		    ToAlternate(xw, False);
		} else {
		    if (screen->whichBuf
			&& (code == 1047))
			ClearScreen(xw);
		    FromAlternate(xw);
		}
	    } else if (IsSM()) {
		do_ti_xtra_scroll(xw);
	    }
	    break;
	case srm_DECNKM:
	    (*func) (&xw->keyboard.flags, MODE_DECKPAM);
	    update_appkeypad();
	    break;
	case srm_DECBKM:
	    /* back-arrow mapped to backspace or delete(D) */
	    (*func) (&xw->keyboard.flags, MODE_DECBKM);
	    TRACE(("DECSET DECBKM %s\n",
		   BtoS(xw->keyboard.flags & MODE_DECBKM)));
	    update_decbkm();
	    break;
	case srm_DECLRMM:
	    if (screen->vtXX_level >= 4) {	/* VT420 */
		(*func) (&xw->flags, LEFT_RIGHT);
		if (IsLeftRightMode(xw)) {
		    xterm_ResetDouble(xw);
		} else {
		    reset_lr_margins(screen);
		}
	    }
	    break;
#if OPT_SIXEL_GRAPHICS
	case srm_DECSDM:	/* sixel scrolling */
	    if (screen->terminal_id == 240 ||	/* FIXME: VT24x did not scroll sixel graphics */
		screen->terminal_id == 241 ||
		screen->terminal_id == 330 ||
		screen->terminal_id == 340 ||
		screen->terminal_id == 382) {
		(*func) (&xw->keyboard.flags, MODE_DECSDM);
		TRACE(("DECSET/DECRST DECSDM %s (resource default is %d)\n",
		       BtoS(xw->keyboard.flags & MODE_DECSDM),
		       TScreenOf(xw)->sixel_scrolling));
		update_decsdm();
	    }
	    break;
#endif
	case srm_DECNCSM:
	    if (screen->vtXX_level >= 5) {	/* VT510 */
		(*func) (&xw->flags, NOCLEAR_COLM);
	    }
	    break;
	case srm_VT200_MOUSE:	/* xterm bogus sequence         */
	    MotionOff(screen, xw);
	    set_mousemode(VT200_MOUSE);
	    break;
	case srm_VT200_HIGHLIGHT_MOUSE:	/* xterm sequence w/hilite tracking */
	    MotionOff(screen, xw);
	    set_mousemode(VT200_HIGHLIGHT_MOUSE);
	    break;
	case srm_BTN_EVENT_MOUSE:
	    MotionOff(screen, xw);
	    set_mousemode(BTN_EVENT_MOUSE);
	    break;
	case srm_ANY_EVENT_MOUSE:
	    set_mousemode(ANY_EVENT_MOUSE);
	    if (screen->send_mouse_pos == MOUSE_OFF) {
		MotionOff(screen, xw);
	    } else {
		MotionOn(screen, xw);
	    }
	    break;
#if OPT_FOCUS_EVENT
	case srm_FOCUS_EVENT_MOUSE:
	    set_bool_mode(screen->send_focus_pos);
	    break;
#endif
	case srm_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_SGR_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_URXVT_EXT_MODE_MOUSE:
	    /*
	     * Rather than choose an arbitrary precedence among the coordinate
	     * modes, they are mutually exclusive.  For consistency, a reset is
	     * only effective against the matching mode.
	     */
	    if (IsSM()) {
		screen->extend_coords = code;
	    } else if (screen->extend_coords == code) {
		screen->extend_coords = 0;
	    }
	    break;
	case srm_ALTERNATE_SCROLL:
	    set_bool_mode(screen->alternateScroll);
	    break;
	case srm_RXVT_SCROLL_TTY_OUTPUT:
	    set_bool_mode(screen->scrollttyoutput);
	    update_scrollttyoutput();
	    break;
	case srm_RXVT_SCROLL_TTY_KEYPRESS:
	    set_bool_mode(screen->scrollkey);
	    update_scrollkey();
	    break;
	case srm_EIGHT_BIT_META:
	    if (screen->eight_bit_meta != ebNever) {
		set_bool_mode(screen->eight_bit_meta);
	    }
	    break;
#if OPT_NUM_LOCK
	case srm_REAL_NUMLOCK:
	    set_bool_mode(xw->misc.real_NumLock);
	    update_num_lock();
	    break;
	case srm_META_SENDS_ESC:
	    set_bool_mode(screen->meta_sends_esc);
	    update_meta_esc();
	    break;
#endif
	case srm_DELETE_IS_DEL:
	    set_bool_mode(screen->delete_is_del);
	    update_delete_del();
	    break;
#if OPT_NUM_LOCK
	case srm_ALT_SENDS_ESC:
	    set_bool_mode(screen->alt_sends_esc);
	    update_alt_esc();
	    break;
#endif
	case srm_KEEP_SELECTION:
	    set_bool_mode(screen->keepSelection);
	    update_keepSelection();
	    break;
	case srm_SELECT_TO_CLIPBOARD:
	    set_bool_mode(screen->selectToClipboard);
	    update_selectToClipboard();
	    break;
	case srm_BELL_IS_URGENT:
	    set_bool_mode(screen->bellIsUrgent);
	    update_bellIsUrgent();
	    break;
	case srm_POP_ON_BELL:
	    set_bool_mode(screen->poponbell);
	    update_poponbell();
	    break;
	case srm_KEEP_CLIPBOARD:
	    set_bool_mode(screen->keepClipboard);
	    update_keepClipboard();
	    break;
	case srm_ALLOW_ALTBUF:
	    if (IsSM()) {
		xw->misc.titeInhibit = False;
	    } else if (!xw->misc.titeInhibit) {
		xw->misc.titeInhibit = True;
		FromAlternate(xw);
	    }
	    update_titeInhibit();
	    break;
	case srm_SAVE_CURSOR:
	    if (!xw->misc.titeInhibit) {
		if (IsSM())
		    CursorSave(xw);
		else
		    CursorRestore(xw);
	    }
	    break;
#if OPT_TCAP_FKEYS
	case srm_TCAP_FKEYS:
	    set_keyboard_type(xw, keyboardIsTermcap, IsSM());
	    break;
#endif
#if OPT_SUN_FUNC_KEYS
	case srm_SUN_FKEYS:
	    set_keyboard_type(xw, keyboardIsSun, IsSM());
	    break;
#endif
#if OPT_HP_FUNC_KEYS
	case srm_HP_FKEYS:
	    set_keyboard_type(xw, keyboardIsHP, IsSM());
	    break;
#endif
#if OPT_SCO_FUNC_KEYS
	case srm_SCO_FKEYS:
	    set_keyboard_type(xw, keyboardIsSCO, IsSM());
	    break;
#endif
	case srm_LEGACY_FKEYS:
	    set_keyboard_type(xw, keyboardIsLegacy, IsSM());
	    break;
#if OPT_SUNPC_KBD
	case srm_VT220_FKEYS:
	    set_keyboard_type(xw, keyboardIsVT220, IsSM());
	    break;
#endif
#if OPT_READLINE
	case srm_BUTTON1_MOVE_POINT:
	    set_mouseflag(click1_moves);
	    break;
	case srm_BUTTON2_MOVE_POINT:
	    set_mouseflag(paste_moves);
	    break;
	case srm_DBUTTON3_DELETE:
	    set_mouseflag(dclick3_deletes);
	    break;
	case srm_PASTE_IN_BRACKET:
	    set_mouseflag(paste_brackets);
	    break;
	case srm_PASTE_QUOTE:
	    set_mouseflag(paste_quotes);
	    break;
	case srm_PASTE_LITERAL_NL:
	    set_mouseflag(paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
#if OPT_GRAPHICS
	case srm_PRIVATE_COLOR_REGISTERS:	/* private color registers for each graphic */
	    TRACE(("DECSET/DECRST PRIVATE_COLOR_REGISTERS to %s (resource default is %s)\n",
		   BtoS(screen->privatecolorregisters),
		   BtoS(TScreenOf(xw)->privatecolorregisters)));
	    set_bool_mode(screen->privatecolorregisters);
	    update_privatecolorregisters();
	    break;
#endif
#if OPT_SIXEL_GRAPHICS
	case srm_SIXEL_SCROLLS_RIGHT:	/* sixel scrolling moves cursor to right */
	    if (screen->terminal_id == 240 ||	/* FIXME: VT24x did not scroll sixel graphics */
		screen->terminal_id == 241 ||
		screen->terminal_id == 330 ||
		screen->terminal_id == 340 ||
		screen->terminal_id == 382) {
		set_bool_mode(screen->sixel_scrolls_right);
		TRACE(("DECSET/DECRST SIXEL_SCROLLS_RIGHT to %s (resource default is %s)\n",
		       BtoS(screen->sixel_scrolls_right),
		       BtoS(TScreenOf(xw)->sixel_scrolls_right)));
	    }
	    break;
#endif
	default:
	    TRACE(("DATA_ERROR: unknown private code %d\n", code));
	    break;
	}
    }
}

/*
 * process xterm private modes save
 */
static void
savemodes(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int i;

    for (i = 0; i < nparam; i++) {
	int code = GetParam(i);

	TRACE(("savemodes %d\n", code));
	switch ((DECSET_codes) code) {
	case srm_DECCKM:
	    DoSM(DP_DECCKM, xw->keyboard.flags & MODE_DECCKM);
	    break;
	case srm_DECANM:	/* ANSI/VT52 mode      */
	    /* no effect */
	    break;
	case srm_DECCOLM:
	    if (screen->c132)
		DoSM(DP_DECCOLM, xw->flags & IN132COLUMNS);
	    break;
	case srm_DECSCLM:	/* (slow scroll)        */
	    DoSM(DP_DECSCLM, xw->flags & SMOOTHSCROLL);
	    break;
	case srm_DECSCNM:
	    DoSM(DP_DECSCNM, xw->flags & REVERSE_VIDEO);
	    break;
	case srm_DECOM:
	    DoSM(DP_DECOM, xw->flags & ORIGIN);
	    break;
	case srm_DECAWM:
	    DoSM(DP_DECAWM, xw->flags & WRAPAROUND);
	    break;
	case srm_DECARM:
	    /* ignore autorepeat */
	    break;
	case srm_X10_MOUSE:	/* mouse bogus sequence */
	    DoSM(DP_X_X10MSE, screen->send_mouse_pos);
	    break;
#if OPT_TOOLBAR
	case srm_RXVT_TOOLBAR:
	    DoSM(DP_TOOLBAR, resource.toolBar);
	    break;
#endif
#if OPT_BLINK_CURS
	case srm_ATT610_BLINK:	/* AT&T 610: Start/stop blinking cursor */
	    if (SettableCursorBlink(screen)) {
		DoSM(DP_CRS_BLINK, screen->cursor_blink_esc);
	    }
	    break;
	case srm_CURSOR_BLINK_OPS:
	    /* intentionally ignored (this is user-preference) */
	    break;
	case srm_XOR_CURSOR_BLINKS:
	    /* intentionally ignored (this is user-preference) */
	    break;
#endif
	case srm_DECPFF:	/* print form feed */
	    DoSM(DP_PRN_FORMFEED, PrinterOf(screen).printer_formfeed);
	    break;
	case srm_DECPEX:	/* print extent */
	    DoSM(DP_PRN_EXTENT, PrinterOf(screen).printer_extent);
	    break;
	case srm_DECTCEM:	/* Show/hide cursor (VT200) */
	    DoSM(DP_CRS_VISIBLE, screen->cursor_set);
	    break;
	case srm_RXVT_SCROLLBAR:
	    DoSM(DP_RXVT_SCROLLBAR, (screen->fullVwin.sb_info.width != 0));
	    break;
#if OPT_SHIFT_FONTS
	case srm_RXVT_FONTSIZE:
	    DoSM(DP_RXVT_FONTSIZE, xw->misc.shift_fonts);
	    break;
#endif
#if OPT_TEK4014
	case srm_DECTEK:
	    DoSM(DP_DECTEK, TEK4014_ACTIVE(xw));
	    break;
#endif
	case srm_132COLS:	/* 132 column mode              */
	    DoSM(DP_X_DECCOLM, screen->c132);
	    break;
	case srm_CURSES_HACK:	/* curses hack                  */
	    DoSM(DP_X_MORE, screen->curses);
	    break;
	case srm_DECNRCM:	/* national charset (VT220) */
	    if (screen->vtXX_level >= 2) {
		DoSM(DP_DECNRCM, xw->flags & NATIONAL);
	    }
	    break;
	case srm_MARGIN_BELL:	/* margin bell                  */
	    DoSM(DP_X_MARGIN, screen->marginbell);
	    break;
	case srm_REVERSEWRAP:	/* reverse wraparound   */
	    DoSM(DP_X_REVWRAP, xw->flags & REVERSEWRAP);
	    break;
#ifdef ALLOWLOGGING
	case srm_ALLOWLOGGING:	/* logging              */
	    DoSM(DP_X_LOGGING, screen->logging);
	    break;
#endif
	case srm_OPT_ALTBUF_CURSOR:
	    /* FALLTHRU */
	case srm_OPT_ALTBUF:
	    /* FALLTHRU */
	case srm_ALTBUF:	/* alternate buffer             */
	    DoSM(DP_X_ALTBUF, screen->whichBuf);
	    break;
	case srm_DECNKM:
	    DoSM(DP_DECKPAM, xw->keyboard.flags & MODE_DECKPAM);
	    break;
	case srm_DECBKM:	/* backarrow mapping */
	    DoSM(DP_DECBKM, xw->keyboard.flags & MODE_DECBKM);
	    break;
	case srm_DECLRMM:	/* left-right */
	    DoSM(DP_X_LRMM, LEFT_RIGHT);
	    break;
#if OPT_SIXEL_GRAPHICS
	case srm_DECSDM:	/* sixel scrolling */
	    DoSM(DP_DECSDM, xw->keyboard.flags & MODE_DECSDM);
	    update_decsdm();
	    break;
#endif
	case srm_DECNCSM:	/* noclear */
	    DoSM(DP_X_NCSM, NOCLEAR_COLM);
	    break;
	case srm_VT200_MOUSE:	/* mouse bogus sequence         */
	    /* FALLTHRU */
	case srm_VT200_HIGHLIGHT_MOUSE:
	    /* FALLTHRU */
	case srm_BTN_EVENT_MOUSE:
	    /* FALLTHRU */
	case srm_ANY_EVENT_MOUSE:
	    DoSM(DP_X_MOUSE, screen->send_mouse_pos);
	    break;
#if OPT_FOCUS_EVENT
	case srm_FOCUS_EVENT_MOUSE:
	    DoSM(DP_X_FOCUS, screen->send_focus_pos);
	    break;
#endif
	case srm_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_SGR_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_URXVT_EXT_MODE_MOUSE:
	    DoSM(DP_X_EXT_MOUSE, screen->extend_coords);
	    break;
	case srm_ALTERNATE_SCROLL:
	    DoSM(DP_ALTERNATE_SCROLL, screen->alternateScroll);
	    break;
	case srm_RXVT_SCROLL_TTY_OUTPUT:
	    DoSM(DP_RXVT_SCROLL_TTY_OUTPUT, screen->scrollttyoutput);
	    break;
	case srm_RXVT_SCROLL_TTY_KEYPRESS:
	    DoSM(DP_RXVT_SCROLL_TTY_KEYPRESS, screen->scrollkey);
	    break;
	case srm_EIGHT_BIT_META:
	    DoSM(DP_EIGHT_BIT_META, screen->eight_bit_meta);
	    break;
#if OPT_NUM_LOCK
	case srm_REAL_NUMLOCK:
	    DoSM(DP_REAL_NUMLOCK, xw->misc.real_NumLock);
	    break;
	case srm_META_SENDS_ESC:
	    DoSM(DP_META_SENDS_ESC, screen->meta_sends_esc);
	    break;
#endif
	case srm_DELETE_IS_DEL:
	    DoSM(DP_DELETE_IS_DEL, screen->delete_is_del);
	    break;
#if OPT_NUM_LOCK
	case srm_ALT_SENDS_ESC:
	    DoSM(DP_ALT_SENDS_ESC, screen->alt_sends_esc);
	    break;
#endif
	case srm_KEEP_SELECTION:
	    DoSM(DP_KEEP_SELECTION, screen->keepSelection);
	    break;
	case srm_SELECT_TO_CLIPBOARD:
	    DoSM(DP_SELECT_TO_CLIPBOARD, screen->selectToClipboard);
	    break;
	case srm_BELL_IS_URGENT:
	    DoSM(DP_BELL_IS_URGENT, screen->bellIsUrgent);
	    break;
	case srm_POP_ON_BELL:
	    DoSM(DP_POP_ON_BELL, screen->poponbell);
	    break;
	case srm_KEEP_CLIPBOARD:
	    DoSM(DP_KEEP_CLIPBOARD, screen->keepClipboard);
	    break;
#if OPT_TCAP_FKEYS
	case srm_TCAP_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_SUN_FUNC_KEYS
	case srm_SUN_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_HP_FUNC_KEYS
	case srm_HP_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_SCO_FUNC_KEYS
	case srm_SCO_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_SUNPC_KBD
	case srm_VT220_FKEYS:
	    /* FALLTHRU */
#endif
	case srm_LEGACY_FKEYS:
	    DoSM(DP_KEYBOARD_TYPE, xw->keyboard.type);
	    break;
	case srm_ALLOW_ALTBUF:
	    DoSM(DP_ALLOW_ALTBUF, xw->misc.titeInhibit);
	    break;
	case srm_SAVE_CURSOR:
	    if (!xw->misc.titeInhibit) {
		CursorSave(xw);
	    }
	    break;
#if OPT_READLINE
	case srm_BUTTON1_MOVE_POINT:
	    SCREEN_FLAG_save(screen, click1_moves);
	    break;
	case srm_BUTTON2_MOVE_POINT:
	    SCREEN_FLAG_save(screen, paste_moves);
	    break;
	case srm_DBUTTON3_DELETE:
	    SCREEN_FLAG_save(screen, dclick3_deletes);
	    break;
	case srm_PASTE_IN_BRACKET:
	    SCREEN_FLAG_save(screen, paste_brackets);
	    break;
	case srm_PASTE_QUOTE:
	    SCREEN_FLAG_save(screen, paste_quotes);
	    break;
	case srm_PASTE_LITERAL_NL:
	    SCREEN_FLAG_save(screen, paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
#if OPT_GRAPHICS
	case srm_PRIVATE_COLOR_REGISTERS:	/* private color registers for each graphic */
	    TRACE(("save PRIVATE_COLOR_REGISTERS %s\n",
		   BtoS(screen->privatecolorregisters)));
	    DoSM(DP_X_PRIVATE_COLOR_REGISTERS, screen->privatecolorregisters);
	    update_privatecolorregisters();
	    break;
#endif
#if OPT_SIXEL_GRAPHICS
	case srm_SIXEL_SCROLLS_RIGHT:
	    TRACE(("save SIXEL_SCROLLS_RIGHT %s\n",
		   BtoS(screen->sixel_scrolls_right)));
	    DoSM(DP_SIXEL_SCROLLS_RIGHT, screen->sixel_scrolls_right);
	    break;
#endif
	}
    }
}

/*
 * process xterm private modes restore
 */
static void
restoremodes(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int i, j;

    for (i = 0; i < nparam; i++) {
	int code = GetParam(i);

	TRACE(("restoremodes %d\n", code));
	switch ((DECSET_codes) code) {
	case srm_DECCKM:
	    bitcpy(&xw->keyboard.flags,
		   screen->save_modes[DP_DECCKM], MODE_DECCKM);
	    update_appcursor();
	    break;
	case srm_DECANM:	/* ANSI/VT52 mode      */
	    /* no effect */
	    break;
	case srm_DECCOLM:
	    if (screen->c132) {
		if (!(xw->flags & NOCLEAR_COLM))
		    ClearScreen(xw);
		CursorSet(screen, 0, 0, xw->flags);
		if ((j = (screen->save_modes[DP_DECCOLM] & IN132COLUMNS)
		     ? 132 : 80) != ((xw->flags & IN132COLUMNS)
				     ? 132 : 80) || j != MaxCols(screen))
		    RequestResize(xw, -1, j, True);
		bitcpy(&xw->flags,
		       screen->save_modes[DP_DECCOLM],
		       IN132COLUMNS);
	    }
	    break;
	case srm_DECSCLM:	/* (slow scroll)        */
	    if (screen->save_modes[DP_DECSCLM] & SMOOTHSCROLL) {
		screen->jumpscroll = 0;
		if (screen->scroll_amt)
		    FlushScroll(xw);
	    } else
		screen->jumpscroll = 1;
	    bitcpy(&xw->flags, screen->save_modes[DP_DECSCLM], SMOOTHSCROLL);
	    update_jumpscroll();
	    break;
	case srm_DECSCNM:
	    if ((screen->save_modes[DP_DECSCNM] ^ xw->flags) & REVERSE_VIDEO) {
		bitcpy(&xw->flags, screen->save_modes[DP_DECSCNM], REVERSE_VIDEO);
		ReverseVideo(xw);
		/* update_reversevideo done in RevVid */
	    }
	    break;
	case srm_DECOM:
	    bitcpy(&xw->flags, screen->save_modes[DP_DECOM], ORIGIN);
	    CursorSet(screen, 0, 0, xw->flags);
	    break;

	case srm_DECAWM:
	    bitcpy(&xw->flags, screen->save_modes[DP_DECAWM], WRAPAROUND);
	    update_autowrap();
	    break;
	case srm_DECARM:
	    /* ignore autorepeat */
	    break;
	case srm_X10_MOUSE:	/* MIT bogus sequence           */
	    DoRM0(DP_X_X10MSE, screen->send_mouse_pos);
	    really_set_mousemode(xw,
				 screen->send_mouse_pos != MOUSE_OFF,
				 (XtermMouseModes) screen->send_mouse_pos);
	    break;
#if OPT_TOOLBAR
	case srm_RXVT_TOOLBAR:
	    DoRM(DP_TOOLBAR, resource.toolBar);
	    ShowToolbar(resource.toolBar);
	    break;
#endif
#if OPT_BLINK_CURS
	case srm_ATT610_BLINK:	/* Start/stop blinking cursor */
	    if (SettableCursorBlink(screen)) {
		DoRM(DP_CRS_BLINK, screen->cursor_blink_esc);
		UpdateCursorBlink(screen);
	    }
	    break;
	case srm_CURSOR_BLINK_OPS:
	    /* intentionally ignored (this is user-preference) */
	    break;
	case srm_XOR_CURSOR_BLINKS:
	    /* intentionally ignored (this is user-preference) */
	    break;
#endif
	case srm_DECPFF:	/* print form feed */
	    DoRM(DP_PRN_FORMFEED, PrinterOf(screen).printer_formfeed);
	    break;
	case srm_DECPEX:	/* print extent */
	    DoRM(DP_PRN_EXTENT, PrinterOf(screen).printer_extent);
	    break;
	case srm_DECTCEM:	/* Show/hide cursor (VT200) */
	    DoRM(DP_CRS_VISIBLE, screen->cursor_set);
	    break;
	case srm_RXVT_SCROLLBAR:
	    if ((screen->fullVwin.sb_info.width != 0) !=
		screen->save_modes[DP_RXVT_SCROLLBAR]) {
		ToggleScrollBar(xw);
	    }
	    break;
#if OPT_SHIFT_FONTS
	case srm_RXVT_FONTSIZE:
	    DoRM(DP_RXVT_FONTSIZE, xw->misc.shift_fonts);
	    break;
#endif
#if OPT_TEK4014
	case srm_DECTEK:
	    if (!(screen->inhibit & I_TEK) &&
		(TEK4014_ACTIVE(xw) != (Boolean) screen->save_modes[DP_DECTEK])) {
		FlushLog(xw);
		TEK4014_ACTIVE(xw) = (Boolean) screen->save_modes[DP_DECTEK];
		update_vttekmode();
	    }
	    break;
#endif
	case srm_132COLS:	/* 132 column mode              */
	    DoRM(DP_X_DECCOLM, screen->c132);
	    update_allow132();
	    break;
	case srm_CURSES_HACK:	/* curses hack                  */
	    DoRM(DP_X_MORE, screen->curses);
	    update_cursesemul();
	    break;
	case srm_DECNRCM:	/* national charset (VT220) */
	    if (screen->vtXX_level >= 2) {
		if (bitcpy(&xw->flags, screen->save_modes[DP_DECNRCM], NATIONAL))
		    modified_DECNRCM(xw);
	    }
	    break;
	case srm_MARGIN_BELL:	/* margin bell                  */
	    if ((DoRM(DP_X_MARGIN, screen->marginbell)) == 0)
		screen->bellArmed = -1;
	    update_marginbell();
	    break;
	case srm_REVERSEWRAP:	/* reverse wraparound   */
	    bitcpy(&xw->flags, screen->save_modes[DP_X_REVWRAP], REVERSEWRAP);
	    update_reversewrap();
	    break;
#ifdef ALLOWLOGGING
	case srm_ALLOWLOGGING:	/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    if (screen->save_modes[DP_X_LOGGING])
		StartLog(xw);
	    else
		CloseLog(xw);
#endif /* ALLOWLOGFILEONOFF */
	    /* update_logging done by StartLog and CloseLog */
	    break;
#endif
	case srm_OPT_ALTBUF_CURSOR:	/* alternate buffer & cursor */
	    /* FALLTHRU */
	case srm_OPT_ALTBUF:
	    /* FALLTHRU */
	case srm_ALTBUF:	/* alternate buffer */
	    if (!xw->misc.titeInhibit) {
		if (screen->save_modes[DP_X_ALTBUF])
		    ToAlternate(xw, False);
		else
		    FromAlternate(xw);
		/* update_altscreen done by ToAlt and FromAlt */
	    } else if (screen->save_modes[DP_X_ALTBUF]) {
		do_ti_xtra_scroll(xw);
	    }
	    break;
	case srm_DECNKM:
	    bitcpy(&xw->flags, screen->save_modes[DP_DECKPAM], MODE_DECKPAM);
	    update_appkeypad();
	    break;
	case srm_DECBKM:	/* backarrow mapping */
	    bitcpy(&xw->flags, screen->save_modes[DP_DECBKM], MODE_DECBKM);
	    update_decbkm();
	    break;
	case srm_DECLRMM:	/* left-right */
	    bitcpy(&xw->flags, screen->save_modes[DP_X_LRMM], LEFT_RIGHT);
	    if (IsLeftRightMode(xw)) {
		xterm_ResetDouble(xw);
	    } else {
		reset_lr_margins(screen);
	    }
	    break;
#if OPT_SIXEL_GRAPHICS
	case srm_DECSDM:	/* sixel scrolling */
	    bitcpy(&xw->keyboard.flags, screen->save_modes[DP_DECSDM], MODE_DECSDM);
	    update_decsdm();
	    break;
#endif
	case srm_DECNCSM:	/* noclear */
	    bitcpy(&xw->flags, screen->save_modes[DP_X_NCSM], NOCLEAR_COLM);
	    break;
	case srm_VT200_MOUSE:	/* mouse bogus sequence         */
	    /* FALLTHRU */
	case srm_VT200_HIGHLIGHT_MOUSE:
	    /* FALLTHRU */
	case srm_BTN_EVENT_MOUSE:
	    /* FALLTHRU */
	case srm_ANY_EVENT_MOUSE:
	    DoRM0(DP_X_MOUSE, screen->send_mouse_pos);
	    really_set_mousemode(xw,
				 screen->send_mouse_pos != MOUSE_OFF,
				 (XtermMouseModes) screen->send_mouse_pos);
	    break;
#if OPT_FOCUS_EVENT
	case srm_FOCUS_EVENT_MOUSE:
	    DoRM(DP_X_FOCUS, screen->send_focus_pos);
	    break;
#endif
	case srm_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_SGR_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_URXVT_EXT_MODE_MOUSE:
	    DoRM(DP_X_EXT_MOUSE, screen->extend_coords);
	    break;
	case srm_ALLOW_ALTBUF:
	    DoRM(DP_ALLOW_ALTBUF, xw->misc.titeInhibit);
	    if (xw->misc.titeInhibit)
		FromAlternate(xw);
	    update_titeInhibit();
	    break;
	case srm_SAVE_CURSOR:
	    if (!xw->misc.titeInhibit) {
		CursorRestore(xw);
	    }
	    break;
	case srm_ALTERNATE_SCROLL:
	    DoRM(DP_ALTERNATE_SCROLL, screen->alternateScroll);
	    break;
	case srm_RXVT_SCROLL_TTY_OUTPUT:
	    DoRM(DP_RXVT_SCROLL_TTY_OUTPUT, screen->scrollttyoutput);
	    update_scrollttyoutput();
	    break;
	case srm_RXVT_SCROLL_TTY_KEYPRESS:
	    DoRM(DP_RXVT_SCROLL_TTY_KEYPRESS, screen->scrollkey);
	    update_scrollkey();
	    break;
	case srm_EIGHT_BIT_META:
	    DoRM(DP_EIGHT_BIT_META, screen->eight_bit_meta);
	    break;
#if OPT_NUM_LOCK
	case srm_REAL_NUMLOCK:
	    DoRM(DP_REAL_NUMLOCK, xw->misc.real_NumLock);
	    update_num_lock();
	    break;
	case srm_META_SENDS_ESC:
	    DoRM(DP_META_SENDS_ESC, screen->meta_sends_esc);
	    update_meta_esc();
	    break;
#endif
	case srm_DELETE_IS_DEL:
	    DoRM(DP_DELETE_IS_DEL, screen->delete_is_del);
	    update_delete_del();
	    break;
#if OPT_NUM_LOCK
	case srm_ALT_SENDS_ESC:
	    DoRM(DP_ALT_SENDS_ESC, screen->alt_sends_esc);
	    update_alt_esc();
	    break;
#endif
	case srm_KEEP_SELECTION:
	    DoRM(DP_KEEP_SELECTION, screen->keepSelection);
	    update_keepSelection();
	    break;
	case srm_SELECT_TO_CLIPBOARD:
	    DoRM(DP_SELECT_TO_CLIPBOARD, screen->selectToClipboard);
	    update_selectToClipboard();
	    break;
	case srm_BELL_IS_URGENT:
	    DoRM(DP_BELL_IS_URGENT, screen->bellIsUrgent);
	    update_bellIsUrgent();
	    break;
	case srm_POP_ON_BELL:
	    DoRM(DP_POP_ON_BELL, screen->poponbell);
	    update_poponbell();
	    break;
	case srm_KEEP_CLIPBOARD:
	    DoRM(DP_KEEP_CLIPBOARD, screen->keepClipboard);
	    update_keepClipboard();
	    break;
#if OPT_TCAP_FKEYS
	case srm_TCAP_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_SUN_FUNC_KEYS
	case srm_SUN_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_HP_FUNC_KEYS
	case srm_HP_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_SCO_FUNC_KEYS
	case srm_SCO_FKEYS:
	    /* FALLTHRU */
#endif
#if OPT_SUNPC_KBD
	case srm_VT220_FKEYS:
	    /* FALLTHRU */
#endif
	case srm_LEGACY_FKEYS:
	    xw->keyboard.type = (xtermKeyboardType) screen->save_modes[DP_KEYBOARD_TYPE];
	    break;
#if OPT_READLINE
	case srm_BUTTON1_MOVE_POINT:
	    SCREEN_FLAG_restore(screen, click1_moves);
	    break;
	case srm_BUTTON2_MOVE_POINT:
	    SCREEN_FLAG_restore(screen, paste_moves);
	    break;
	case srm_DBUTTON3_DELETE:
	    SCREEN_FLAG_restore(screen, dclick3_deletes);
	    break;
	case srm_PASTE_IN_BRACKET:
	    SCREEN_FLAG_restore(screen, paste_brackets);
	    break;
	case srm_PASTE_QUOTE:
	    SCREEN_FLAG_restore(screen, paste_quotes);
	    break;
	case srm_PASTE_LITERAL_NL:
	    SCREEN_FLAG_restore(screen, paste_literal_nl);
	    break;
#endif /* OPT_READLINE */
#if OPT_GRAPHICS
	case srm_PRIVATE_COLOR_REGISTERS:	/* private color registers for each graphic */
	    TRACE(("restore PRIVATE_COLOR_REGISTERS before: %s\n",
		   BtoS(screen->privatecolorregisters)));
	    DoRM(DP_X_PRIVATE_COLOR_REGISTERS, screen->privatecolorregisters);
	    TRACE(("restore PRIVATE_COLOR_REGISTERS after: %s\n",
		   BtoS(screen->privatecolorregisters)));
	    update_privatecolorregisters();
	    break;
#endif
#if OPT_SIXEL_GRAPHICS
	case srm_SIXEL_SCROLLS_RIGHT:
	    TRACE(("restore SIXEL_SCROLLS_RIGHT before: %s\n",
		   BtoS(screen->sixel_scrolls_right)));
	    DoRM(DP_SIXEL_SCROLLS_RIGHT, screen->sixel_scrolls_right);
	    TRACE(("restore SIXEL_SCROLLS_RIGHT after: %s\n",
		   BtoS(screen->sixel_scrolls_right)));
	    break;
#endif
	}
    }
}

/*
 * Convert an XTextProperty to a string.
 *
 * This frees the data owned by the XTextProperty, and returns in its place the
 * string, which must be freed by the caller.
 */
static char *
property_to_string(XtermWidget xw, XTextProperty * text)
{
    TScreen *screen = TScreenOf(xw);
    Display *dpy = screen->display;
    char *result = 0;
    char **list;
    int length = 0;
    int rc;

    TRACE(("property_to_string value %p, encoding %s, format %d, nitems %ld\n",
	   text->value,
	   TraceAtomName(dpy, text->encoding),
	   text->format,
	   text->nitems));

#if OPT_WIDE_CHARS
    /*
     * We will use the XmbTextPropertyToTextList call to extract UTF-8 data.
     * The xtermUtf8ToTextList() call is used to convert UTF-8 explicitly to
     * ISO-8859-1.
     */
    if ((text->format != 8)
	|| IsTitleMode(xw, tmGetUtf8)
	|| (rc = xtermUtf8ToTextList(xw, text, &list, &length)) < 0)
#endif
	if ((rc = XmbTextPropertyToTextList(dpy, text, &list, &length)) < 0)
	    rc = XTextPropertyToStringList(text, &list, &length);

    if (rc >= 0) {
	int n, c, pass;
	size_t need = 0;

	for (pass = 0; pass < 2; ++pass) {
	    for (n = 0, need = 0; n < length; n++) {
		char *s = list[n];
		while ((c = *s++) != '\0') {
		    if (pass)
			result[need] = (char) c;
		    ++need;
		}
	    }
	    if (pass)
		result[need] = '\0';
	    else
		result = malloc(need + 1);
	    if (result == 0)
		break;
	}
	XFreeStringList(list);
    }
    if (text->value != 0)
	XFree(text->value);

    return result;
}

static char *
get_icon_label(XtermWidget xw)
{
    XTextProperty text;
    char *result = 0;

    if (XGetWMIconName(TScreenOf(xw)->display, VShellWindow(xw), &text)) {
	result = property_to_string(xw, &text);
    }
    return result;
}

static char *
get_window_label(XtermWidget xw)
{
    XTextProperty text;
    char *result = 0;

    if (XGetWMName(TScreenOf(xw)->display, VShellWindow(xw), &text)) {
	result = property_to_string(xw, &text);
    }
    return result;
}

/*
 * Report window label (icon or title) in dtterm protocol
 * ESC ] code label ESC backslash
 */
static void
report_win_label(XtermWidget xw,
		 int code,
		 char *text)
{
    unparseputc(xw, ANSI_ESC);
    unparseputc(xw, ']');
    unparseputc(xw, code);

    if (text != 0) {
	int copy = IsTitleMode(xw, tmGetBase16);
	if (copy) {
	    TRACE(("Encoding hex:%s\n", text));
	    text = x_encode_hex(text);
	}
	unparseputs(xw, text);
	if (copy)
	    free(text);
    }

    unparseputc(xw, ANSI_ESC);
    unparseputc(xw, '\\');	/* should be ST */
    unparse_end(xw);
}

/*
 * Window operations (from CDE dtterm description, as well as extensions).
 * See also "allowWindowOps" resource.
 */
static void
window_ops(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    XWindowChanges values;
    XWindowAttributes win_attrs;
#if OPT_MAXIMIZE
    unsigned root_width;
    unsigned root_height;
#endif
    int code = zero_if_default(0);
    char *label;

    TRACE(("window_ops %d\n", code));
    switch (code) {
    case ewRestoreWin:		/* Restore (de-iconify) window */
	if (AllowWindowOps(xw, ewRestoreWin)) {
	    TRACE(("...de-iconify window\n"));
	    XMapWindow(screen->display,
		       VShellWindow(xw));
	}
	break;

    case ewMinimizeWin:	/* Minimize (iconify) window */
	if (AllowWindowOps(xw, ewMinimizeWin)) {
	    TRACE(("...iconify window\n"));
	    XIconifyWindow(screen->display,
			   VShellWindow(xw),
			   DefaultScreen(screen->display));
	}
	break;

    case ewSetWinPosition:	/* Move the window to the given position */
	if (AllowWindowOps(xw, ewSetWinPosition)) {
	    unsigned value_mask;

	    values.x = zero_if_default(1);
	    values.y = zero_if_default(2);
	    TRACE(("...move window to %d,%d\n", values.x, values.y));
	    value_mask = (CWX | CWY);
	    XReconfigureWMWindow(screen->display,
				 VShellWindow(xw),
				 DefaultScreen(screen->display),
				 value_mask,
				 &values);
	}
	break;

    case ewSetWinSizePixels:	/* Resize the window to given size in pixels */
	if (AllowWindowOps(xw, ewSetWinSizePixels)) {
	    RequestResize(xw, optional_param(1), optional_param(2), False);
	}
	break;

    case ewRaiseWin:		/* Raise the window to the front of the stack */
	if (AllowWindowOps(xw, ewRaiseWin)) {
	    TRACE(("...raise window\n"));
	    XRaiseWindow(screen->display, VShellWindow(xw));
	}
	break;

    case ewLowerWin:		/* Lower the window to the bottom of the stack */
	if (AllowWindowOps(xw, ewLowerWin)) {
	    TRACE(("...lower window\n"));
	    XLowerWindow(screen->display, VShellWindow(xw));
	}
	break;

    case ewRefreshWin:		/* Refresh the window */
	if (AllowWindowOps(xw, ewRefreshWin)) {
	    TRACE(("...redraw window\n"));
	    Redraw();
	}
	break;

    case ewSetWinSizeChars:	/* Resize the text-area, in characters */
	if (AllowWindowOps(xw, ewSetWinSizeChars)) {
	    RequestResize(xw, optional_param(1), optional_param(2), True);
	}
	break;

#if OPT_MAXIMIZE
    case ewMaximizeWin:	/* Maximize or restore */
	if (AllowWindowOps(xw, ewMaximizeWin)) {
	    RequestMaximize(xw, zero_if_default(1));
	}
	break;
    case ewFullscreenWin:	/* Fullscreen or restore */
	if (AllowWindowOps(xw, ewFullscreenWin)) {
	    FullScreen(xw, zero_if_default(1));
	}
	break;
#endif

    case ewGetWinState:	/* Report the window's state */
	if (AllowWindowOps(xw, ewGetWinState)) {
	    TRACE(("...get window attributes\n"));
	    xtermGetWinAttrs(screen->display,
			     VWindow(screen),
			     &win_attrs);
	    init_reply(ANSI_CSI);
	    reply.a_pintro = 0;
	    reply.a_nparam = 1;
	    reply.a_param[0] = (ParmType) ((win_attrs.map_state == IsViewable)
					   ? 1
					   : 2);
	    reply.a_inters = 0;
	    reply.a_final = 't';
	    unparseseq(xw, &reply);
	}
	break;

    case ewGetWinPosition:	/* Report the window's position */
	if (AllowWindowOps(xw, ewGetWinPosition)) {
	    TRACE(("...get window position\n"));
	    xtermGetWinAttrs(screen->display,
			     WMFrameWindow(xw),
			     &win_attrs);
	    init_reply(ANSI_CSI);
	    reply.a_pintro = 0;
	    reply.a_nparam = 3;
	    reply.a_param[0] = 3;
	    reply.a_param[1] = (ParmType) win_attrs.x;
	    reply.a_param[2] = (ParmType) win_attrs.y;
	    reply.a_inters = 0;
	    reply.a_final = 't';
	    unparseseq(xw, &reply);
	}
	break;

    case ewGetWinSizePixels:	/* Report the window's size in pixels */
	if (AllowWindowOps(xw, ewGetWinSizePixels)) {
	    TRACE(("...get window size in pixels\n"));
	    init_reply(ANSI_CSI);
	    reply.a_pintro = 0;
	    reply.a_nparam = 3;
	    reply.a_param[0] = 4;
	    reply.a_param[1] = (ParmType) Height(screen);
	    reply.a_param[2] = (ParmType) Width(screen);
	    reply.a_inters = 0;
	    reply.a_final = 't';
	    unparseseq(xw, &reply);
	}
	break;

    case ewGetWinSizeChars:	/* Report the text's size in characters */
	if (AllowWindowOps(xw, ewGetWinSizeChars)) {
	    TRACE(("...get window size in characters\n"));
	    init_reply(ANSI_CSI);
	    reply.a_pintro = 0;
	    reply.a_nparam = 3;
	    reply.a_param[0] = 8;
	    reply.a_param[1] = (ParmType) MaxRows(screen);
	    reply.a_param[2] = (ParmType) MaxCols(screen);
	    reply.a_inters = 0;
	    reply.a_final = 't';
	    unparseseq(xw, &reply);
	}
	break;

#if OPT_MAXIMIZE
    case ewGetScreenSizeChars:	/* Report the screen's size, in characters */
	if (AllowWindowOps(xw, ewGetScreenSizeChars)) {
	    TRACE(("...get screen size in characters\n"));
	    TRACE(("...using font size %dx%d\n",
		   FontHeight(screen),
		   FontWidth(screen)));
	    (void) QueryMaximize(xw, &root_width, &root_height);
	    init_reply(ANSI_CSI);
	    reply.a_pintro = 0;
	    reply.a_nparam = 3;
	    reply.a_param[0] = 9;
	    reply.a_param[1] = (ParmType) (root_height
					   / (unsigned) FontHeight(screen));
	    reply.a_param[2] = (ParmType) (root_width
					   / (unsigned) FontWidth(screen));
	    reply.a_inters = 0;
	    reply.a_final = 't';
	    unparseseq(xw, &reply);
	}
	break;
#endif

    case ewGetIconTitle:	/* Report the icon's label */
	if (AllowWindowOps(xw, ewGetIconTitle)) {
	    TRACE(("...get icon's label\n"));
	    report_win_label(xw, 'L', label = get_icon_label(xw));
	    free(label);
	}
	break;

    case ewGetWinTitle:	/* Report the window's title */
	if (AllowWindowOps(xw, ewGetWinTitle)) {
	    TRACE(("...get window's label\n"));
	    report_win_label(xw, 'l', label = get_window_label(xw));
	    free(label);
	}
	break;

    case ewPushTitle:		/* save the window's title(s) on stack */
	if (AllowWindowOps(xw, ewPushTitle)) {
	    SaveTitle *last = screen->save_title;
	    SaveTitle *item = TypeCalloc(SaveTitle);

	    TRACE(("...push title onto stack\n"));
	    if (item != 0) {
		switch (zero_if_default(1)) {
		case 0:
		    item->iconName = get_icon_label(xw);
		    item->windowName = get_window_label(xw);
		    break;
		case 1:
		    item->iconName = get_icon_label(xw);
		    break;
		case 2:
		    item->windowName = get_window_label(xw);
		    break;
		}
		item->next = last;
		if (item->iconName == 0) {
		    item->iconName = ((last == 0)
				      ? get_icon_label(xw)
				      : x_strdup(last->iconName));
		}
		if (item->windowName == 0) {
		    item->windowName = ((last == 0)
					? get_window_label(xw)
					: x_strdup(last->windowName));
		}
		screen->save_title = item;
	    }
	}
	break;

    case ewPopTitle:		/* restore the window's title(s) from stack */
	if (AllowWindowOps(xw, ewPopTitle)) {
	    SaveTitle *item = screen->save_title;

	    TRACE(("...pop title off stack\n"));
	    if (item != 0) {
		switch (zero_if_default(1)) {
		case 0:
		    ChangeIconName(xw, item->iconName);
		    ChangeTitle(xw, item->windowName);
		    break;
		case 1:
		    ChangeIconName(xw, item->iconName);
		    break;
		case 2:
		    ChangeTitle(xw, item->windowName);
		    break;
		}
		screen->save_title = item->next;
		free(item->iconName);
		free(item->windowName);
		free(item);
	    }
	}
	break;

    default:			/* DECSLPP (24, 25, 36, 48, 72, 144) */
	if (AllowWindowOps(xw, ewSetWinLines)) {
	    if (code >= 24)
		RequestResize(xw, code, -1, True);
	}
	break;
    }
}

/*
 * set a bit in a word given a pointer to the word and a mask.
 */
static int
bitset(unsigned *p, unsigned mask)
{
    unsigned before = *p;
    *p |= mask;
    return (before != *p);
}

/*
 * clear a bit in a word given a pointer to the word and a mask.
 */
static int
bitclr(unsigned *p, unsigned mask)
{
    unsigned before = *p;
    *p &= ~mask;
    return (before != *p);
}

/*
 * Copy bits from one word to another, given a mask
 */
static int
bitcpy(unsigned *p, unsigned q, unsigned mask)
{
    unsigned before = *p;
    bitclr(p, mask);
    bitset(p, q & mask);
    return (before != *p);
}

void
unparseputc1(XtermWidget xw, int c)
{
    if (c >= 0x80 && c <= 0x9F) {
	if (!TScreenOf(xw)->control_eight_bits) {
	    unparseputc(xw, A2E(ANSI_ESC));
	    c = A2E(c - 0x40);
	}
    }
    unparseputc(xw, c);
}

void
unparseseq(XtermWidget xw, ANSI *ap)
{
    int c;

    unparseputc1(xw, c = ap->a_type);
    if (c == ANSI_ESC
	|| c == ANSI_DCS
	|| c == ANSI_CSI
	|| c == ANSI_OSC
	|| c == ANSI_PM
	|| c == ANSI_APC
	|| c == ANSI_SS3) {
	int i;
	int inters;

	if (ap->a_pintro != 0)
	    unparseputc(xw, ap->a_pintro);
	for (i = 0; i < ap->a_nparam; ++i) {
	    if (i != 0) {
		if (ap->a_delim) {
		    unparseputs(xw, ap->a_delim);
		} else {
		    unparseputc(xw, ';');
		}
	    }
	    if (ap->a_radix[i]) {
		char temp[8];
		sprintf(temp, "%04X", ap->a_param[i] & 0xffff);
		unparseputs(xw, temp);
	    } else {
		unparseputn(xw, (unsigned int) ap->a_param[i]);
	    }
	}
	if ((inters = ap->a_inters) != 0) {
	    for (i = 3; i >= 0; --i) {
		c = CharOf(inters >> (8 * i));
		if (c != 0)
		    unparseputc(xw, c);
	    }
	}
	switch (ap->a_type) {
	case ANSI_DCS:
	    /* FALLTHRU */
	case ANSI_OSC:
	    /* FALLTHRU */
	case ANSI_PM:
	    /* FALLTHRU */
	case ANSI_APC:
	    unparseputc1(xw, ANSI_ST);
	    break;
	default:
	    unparseputc(xw, (char) ap->a_final);
	    break;
	}
    }
    unparse_end(xw);
}

void
unparseputn(XtermWidget xw, unsigned int n)
{
    unsigned int q;

    q = n / 10;
    if (q != 0)
	unparseputn(xw, q);
    unparseputc(xw, (char) ('0' + (n % 10)));
}

void
unparseputs(XtermWidget xw, const char *s)
{
    if (s != 0) {
	while (*s)
	    unparseputc(xw, *s++);
    }
}

void
unparseputc(XtermWidget xw, int c)
{
    TScreen *screen = TScreenOf(xw);
    IChar *buf = screen->unparse_bfr;
    unsigned len;

    if ((screen->unparse_len + 2) >= sizeof(screen->unparse_bfr) / sizeof(IChar))
	  unparse_end(xw);

    len = screen->unparse_len;

#if OPT_TCAP_QUERY
    /*
     * If we're returning a termcap string, it has to be translated since
     * a DCS must not contain any characters except for the normal 7-bit
     * printable ASCII (counting tab, carriage return, etc).  For now,
     * just use hexadecimal for the whole thing.
     */
    if (screen->tc_query_code >= 0) {
	char tmp[3];
	sprintf(tmp, "%02X", c & 0xFF);
	buf[len++] = CharOf(tmp[0]);
	buf[len++] = CharOf(tmp[1]);
    } else
#endif
    if ((buf[len++] = (IChar) c) == '\r' && (xw->flags & LINEFEED)) {
	buf[len++] = '\n';
    }

    screen->unparse_len = len;

    /* If send/receive mode is reset, we echo characters locally */
    if ((xw->keyboard.flags & MODE_SRM) == 0) {
	(void) doparsing(xw, (unsigned) c, &myState);
    }
}

void
unparse_end(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->unparse_len) {
#ifdef VMS
	tt_write(screen->unparse_bfr, screen->unparse_len);
#else /* VMS */
	writePtyData(screen->respond, screen->unparse_bfr, screen->unparse_len);
#endif /* VMS */
	screen->unparse_len = 0;
    }
}

void
ToggleAlternate(XtermWidget xw)
{
    if (TScreenOf(xw)->whichBuf)
	FromAlternate(xw);
    else
	ToAlternate(xw, False);
}

static void
ToAlternate(XtermWidget xw, Bool clearFirst)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->whichBuf == 0) {
	TRACE(("ToAlternate\n"));
	if (!screen->editBuf_index[1])
	    screen->editBuf_index[1] = allocScrnBuf(xw,
						    (unsigned) MaxRows(screen),
						    (unsigned) MaxCols(screen),
						    &screen->editBuf_data[1]);
	SwitchBufs(xw, 1, clearFirst);
#if OPT_SAVE_LINES
	screen->visbuf = screen->editBuf_index[screen->whichBuf];
#endif
	update_altscreen();
    }
}

static void
FromAlternate(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->whichBuf != 0) {
	TRACE(("FromAlternate\n"));
	if (screen->scroll_amt)
	    FlushScroll(xw);
	SwitchBufs(xw, 0, False);
#if OPT_SAVE_LINES
	screen->visbuf = screen->editBuf_index[screen->whichBuf];
#endif
	update_altscreen();
    }
}

static void
SwitchBufs(XtermWidget xw, int toBuf, Bool clearFirst)
{
    TScreen *screen = TScreenOf(xw);
    int rows, top;

    screen->whichBuf = toBuf;
    if (screen->cursor_state)
	HideCursor();

    rows = MaxRows(screen);
    SwitchBufPtrs(screen, toBuf);

    if ((top = INX2ROW(screen, 0)) < rows) {
	if (screen->scroll_amt) {
	    FlushScroll(xw);
	}
#if OPT_DOUBLE_BUFFER
	XFillRectangle(screen->display,
		       VDrawable(screen),
		       ReverseGC(xw, screen),
		       (int) OriginX(screen),
		       (int) top * FontHeight(screen) + screen->border,
		       (unsigned) Width(screen),
		       (unsigned) ((rows - top) * FontHeight(screen)));
#else
	XClearArea(screen->display,
		   VWindow(screen),
		   (int) OriginX(screen),
		   (int) top * FontHeight(screen) + screen->border,
		   (unsigned) Width(screen),
		   (unsigned) ((rows - top) * FontHeight(screen)),
		   False);
#endif
	if (clearFirst) {
	    ClearBufRows(xw, top, rows);
	}
    }
    ScrnUpdate(xw, 0, 0, rows, MaxCols(screen), False);
}

Bool
CheckBufPtrs(TScreen *screen)
{
    return (screen->visbuf != 0
#if OPT_SAVE_LINES
	    && screen->editBuf_index[0] != 0
#endif
	    && screen->editBuf_index[1] != 0);
}

/*
 * Swap buffer line pointers between alternate and regular screens.
 */
void
SwitchBufPtrs(TScreen *screen, int toBuf GCC_UNUSED)
{
    if (CheckBufPtrs(screen)) {
#if OPT_SAVE_LINES
	screen->visbuf = screen->editBuf_index[toBuf];
#else
	size_t len = ScrnPointers(screen, (size_t) MaxRows(screen));

	memcpy(screen->save_ptr, screen->visbuf, len);
	memcpy(screen->visbuf, screen->editBuf_index[1], len);
	memcpy(screen->editBuf_index[1], screen->save_ptr, len);
#endif
    }
}

void
VTRun(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("VTRun ...\n"));

    if (!screen->Vshow) {
	set_vt_visibility(True);
    }
    update_vttekmode();
    update_vtshow();
    update_tekshow();
    set_vthide_sensitivity();

    ScrnAllocBuf(xw);

    screen->cursor_state = OFF;
    screen->cursor_set = ON;
#if OPT_BLINK_CURS
    if (DoStartBlinking(screen))
	StartBlinking(screen);
#endif

#if OPT_TEK4014
    if (Tpushb > Tpushback) {
	fillPtyData(xw, VTbuffer, (char *) Tpushback, (int) (Tpushb - Tpushback));
	Tpushb = Tpushback;
    }
#endif
    screen->is_running = True;
    if (screen->embed_high && screen->embed_wide) {
	ScreenResize(xw, screen->embed_wide, screen->embed_high, &(xw->flags));
    }
#if OPT_MAXIMIZE
    else if (resource.fullscreen == esTrue || resource.fullscreen == esAlways)
	FullScreen(term, True);
#endif
    if (!setjmp(VTend))
	VTparse(xw);
    StopBlinking(screen);
    HideCursor();
    screen->cursor_set = OFF;
    TRACE(("... VTRun\n"));
}

/*ARGSUSED*/
static void
VTExpose(Widget w GCC_UNUSED,
	 XEvent *event,
	 Region region GCC_UNUSED)
{
    DEBUG_MSG("Expose\n");
    if (event->type == Expose)
	HandleExposure(term, event);
}

static void
VTGraphicsOrNoExpose(XEvent *event)
{
    TScreen *screen = TScreenOf(term);
    if (screen->incopy <= 0) {
	screen->incopy = 1;
	if (screen->scrolls > 0)
	    screen->scrolls--;
    }
    if (event->type == GraphicsExpose)
	if (HandleExposure(term, event))
	    screen->cursor_state = OFF;
    if ((event->type == NoExpose)
	|| ((XGraphicsExposeEvent *) event)->count == 0) {
	if (screen->incopy <= 0 && screen->scrolls > 0)
	    screen->scrolls--;
	if (screen->scrolls)
	    screen->incopy = -1;
	else
	    screen->incopy = 0;
    }
}

/*ARGSUSED*/
static void
VTNonMaskableEvent(Widget w GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XEvent *event,
		   Boolean *cont GCC_UNUSED)
{
    switch (event->type) {
    case GraphicsExpose:
	/* FALLTHRU */
    case NoExpose:
	VTGraphicsOrNoExpose(event);
	break;
    }
}

static void
VTResize(Widget w)
{
    if (XtIsRealized(w)) {
	XtermWidget xw = (XtermWidget) w;
	ScreenResize(xw, xw->core.width, xw->core.height, &xw->flags);
    }
}

#define okDimension(src,dst) ((src <= 32767) \
			  && ((dst = (Dimension) src) == src))

static void
RequestResize(XtermWidget xw, int rows, int cols, Bool text)
{
    TScreen *screen = TScreenOf(xw);
    Dimension replyWidth, replyHeight;
    Dimension askedWidth, askedHeight;
    XtGeometryResult status;
    XWindowAttributes attrs;

    TRACE(("RequestResize(rows=%d, cols=%d, text=%d)\n", rows, cols, text));

    /* check first if the row/column values fit into a Dimension */
    if (cols > 0) {
	if ((int) (askedWidth = (Dimension) cols) < cols) {
	    TRACE(("... cols too large for Dimension\n"));
	    return;
	}
    } else {
	askedWidth = 0;
    }
    if (rows > 0) {
	if ((int) (askedHeight = (Dimension) rows) < rows) {
	    TRACE(("... rows too large for Dimension\n"));
	    return;
	}
    } else {
	askedHeight = 0;
    }

    if (askedHeight == 0
	|| askedWidth == 0
	|| xw->misc.limit_resize > 0) {
	xtermGetWinAttrs(XtDisplay(xw),
			 RootWindowOfScreen(XtScreen(xw)), &attrs);
    }

    if (text) {
	unsigned long value;

	if ((value = (unsigned long) rows) != 0) {
	    if (rows < 0)
		value = (unsigned long) MaxRows(screen);
	    value *= (unsigned long) FontHeight(screen);
	    value += (unsigned long) (2 * screen->border);
	    if (!okDimension(value, askedHeight))
		return;
	}

	if ((value = (unsigned long) cols) != 0) {
	    if (cols < 0)
		value = (unsigned long) MaxCols(screen);
	    value *= (unsigned long) FontWidth(screen);
	    value += (unsigned long) ((2 * screen->border)
				      + ScrollbarWidth(screen));
	    if (!okDimension(value, askedWidth))
		return;
	}

    } else {
	if (rows < 0)
	    askedHeight = FullHeight(screen);
	if (cols < 0)
	    askedWidth = FullWidth(screen);
    }

    if (rows == 0)
	askedHeight = (Dimension) attrs.height;
    if (cols == 0)
	askedWidth = (Dimension) attrs.width;

    if (xw->misc.limit_resize > 0) {
	Dimension high = (Dimension) (xw->misc.limit_resize * attrs.height);
	Dimension wide = (Dimension) (xw->misc.limit_resize * attrs.width);
	if ((int) high < attrs.height)
	    high = (Dimension) attrs.height;
	if (askedHeight > high)
	    askedHeight = high;
	if ((int) wide < attrs.width)
	    wide = (Dimension) attrs.width;
	if (askedWidth > wide)
	    askedWidth = wide;
    }
#ifndef nothack
    getXtermSizeHints(xw);
#endif

    TRACE(("...requesting resize %dx%d\n", askedHeight, askedWidth));
    status = REQ_RESIZE((Widget) xw,
			askedWidth, askedHeight,
			&replyWidth, &replyHeight);

    if (status == XtGeometryYes ||
	status == XtGeometryDone) {
	ScreenResize(xw, replyWidth, replyHeight, &xw->flags);
    }
#ifndef nothack
    /*
     * XtMakeResizeRequest() has the undesirable side-effect of clearing
     * the window manager's hints, even on a failed request.  This would
     * presumably be fixed if the shell did its own work.
     */
    if (xw->hints.flags
	&& replyHeight
	&& replyWidth) {
	xw->hints.height = replyHeight;
	xw->hints.width = replyWidth;

	TRACE(("%s@%d -- ", __FILE__, __LINE__));
	TRACE_HINTS(&xw->hints);
	XSetWMNormalHints(screen->display, VShellWindow(xw), &xw->hints);
	TRACE(("%s@%d -- ", __FILE__, __LINE__));
	TRACE_WM_HINTS(xw);
    }
#endif

    XSync(screen->display, False);	/* synchronize */
    if (xtermAppPending())
	xevents();

    TRACE(("...RequestResize done\n"));
}

static String xterm_trans =
"<ClientMessage>WM_PROTOCOLS: DeleteWindow()\n\
     <MappingNotify>: KeyboardMapping()\n";

int
VTInit(XtermWidget xw)
{
    Widget vtparent = SHELL_OF(xw);

    TRACE(("VTInit {{\n"));

    XtRealizeWidget(vtparent);
    XtOverrideTranslations(vtparent, XtParseTranslationTable(xterm_trans));
    (void) XSetWMProtocols(XtDisplay(vtparent), XtWindow(vtparent),
			   &wm_delete_window, 1);
    TRACE_TRANS("shell", vtparent);
    TRACE_TRANS("vt100", (Widget) (xw));

    ScrnAllocBuf(xw);

    TRACE(("...}} VTInit\n"));
    return (1);
}

static void
VTClassInit(void)
{
    XtAddConverter(XtRString, XtRGravity, XmuCvtStringToGravity,
		   (XtConvertArgList) NULL, (Cardinal) 0);
}

#if OPT_COLOR_RES
/*
 * Override the use of XtDefaultForeground/XtDefaultBackground to make some
 * colors, such as cursor color, use the actual foreground/background value
 * if there is no explicit resource value used.
 */
static Pixel
fill_Tres(XtermWidget target, XtermWidget source, int offset)
{
    char *name;
    ScrnColors temp;
    TScreen *src = TScreenOf(source);
    TScreen *dst = TScreenOf(target);

    dst->Tcolors[offset] = src->Tcolors[offset];
    dst->Tcolors[offset].mode = False;

    if ((name = x_strtrim(dst->Tcolors[offset].resource)) != 0)
	dst->Tcolors[offset].resource = name;

    if (name == 0) {
	dst->Tcolors[offset].value = target->dft_foreground;
    } else if (isDefaultForeground(name)) {
	dst->Tcolors[offset].value = ((offset == TEXT_FG || offset == TEXT_BG)
				      ? target->dft_foreground
				      : dst->Tcolors[TEXT_FG].value);
    } else if (isDefaultBackground(name)) {
	dst->Tcolors[offset].value = ((offset == TEXT_FG || offset == TEXT_BG)
				      ? target->dft_background
				      : dst->Tcolors[TEXT_BG].value);
    } else {
	memset(&temp, 0, sizeof(temp));
	if (AllocateTermColor(target, &temp, offset, name, True)) {
	    if (COLOR_DEFINED(&(temp), offset))
		free(temp.names[offset]);
	    dst->Tcolors[offset].value = temp.colors[offset];
	} else if (offset == TEXT_FG || offset == TEXT_BG) {
	    free(name);
	    dst->Tcolors[offset].resource = 0;
	}
    }
    return dst->Tcolors[offset].value;
}

/*
 * If one or both of the foreground/background colors cannot be allocated,
 * e.g., due to gross misconfiguration, recover by setting both to the
 * display's default values.
 */
static void
repairColors(XtermWidget target)
{
    TScreen *screen = TScreenOf(target);

    if (screen->Tcolors[TEXT_FG].resource == 0 ||
	screen->Tcolors[TEXT_BG].resource == 0) {
	xtermWarning("unable to allocate fg/bg colors\n");
	screen->Tcolors[TEXT_FG].resource = x_strdup(XtDefaultForeground);
	screen->Tcolors[TEXT_BG].resource = x_strdup(XtDefaultBackground);
	if (screen->Tcolors[TEXT_FG].resource == 0 ||
	    screen->Tcolors[TEXT_BG].resource == 0) {
	    Exit(1);
	}
	screen->Tcolors[TEXT_FG].value = target->dft_foreground;
	screen->Tcolors[TEXT_BG].value = target->dft_background;
    }
}
#else
#define fill_Tres(target, source, offset) \
	TScreenOf(target)->Tcolors[offset] = TScreenOf(source)->Tcolors[offset]
#define repairColors(target)	/* nothing */
#endif

#if OPT_WIDE_CHARS
static void
VTInitialize_locale(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    Bool is_utf8 = xtermEnvUTF8();

    TRACE(("VTInitialize_locale\n"));
    TRACE(("... request screen.utf8_mode = %d\n", screen->utf8_mode));
    TRACE(("... request screen.utf8_fonts = %d\n", screen->utf8_fonts));

    screen->utf8_always = (screen->utf8_mode == uAlways);
    if (screen->utf8_mode < 0)
	screen->utf8_mode = uFalse;

    if (screen->utf8_mode > 3)
	screen->utf8_mode = uDefault;

    screen->latin9_mode = 0;
    screen->unicode_font = 0;
#if OPT_LUIT_PROG
    xw->misc.callfilter = 0;
    xw->misc.use_encoding = 0;

    TRACE(("... setup for luit:\n"));
    TRACE(("... request misc.locale_str = \"%s\"\n", xw->misc.locale_str));

    if (screen->utf8_mode == uFalse) {
	TRACE(("... command-line +u8 overrides\n"));
    } else
#if OPT_MINI_LUIT
    if (x_strcasecmp(xw->misc.locale_str, "CHECKFONT") == 0) {
	int fl = (int) strlen(DefaultFontN(xw));
	if (fl > 11
	    && x_strcasecmp(DefaultFontN(xw) + fl - 11, "-ISO10646-1") == 0) {
	    screen->unicode_font = 1;
	    /* unicode font, use True */
#ifdef HAVE_LANGINFO_CODESET
	    if (!strcmp(xtermEnvEncoding(), "ANSI_X3.4-1968")
		|| !strcmp(xtermEnvEncoding(), "ISO-8859-1")) {
		if (screen->utf8_mode == uDefault)
		    screen->utf8_mode = uFalse;
	    } else if (!strcmp(xtermEnvEncoding(), "ISO-8859-15")) {
		if (screen->utf8_mode == uDefault)
		    screen->utf8_mode = uFalse;
		screen->latin9_mode = 1;
	    } else {
		xw->misc.callfilter = (Boolean) (is_utf8 ? 0 : 1);
		screen->utf8_mode = uAlways;
	    }
#else
	    xw->misc.callfilter = is_utf8 ? 0 : 1;
	    screen->utf8_mode = uAlways;
#endif
	} else {
	    /* other encoding, use False */
	    if (screen->utf8_mode == uDefault) {
		screen->utf8_mode = is_utf8 ? uAlways : uFalse;
	    }
	}
    } else
#endif /* OPT_MINI_LUIT */
	if (x_strcasecmp(xw->misc.locale_str, "TRUE") == 0 ||
	    x_strcasecmp(xw->misc.locale_str, "ON") == 0 ||
	    x_strcasecmp(xw->misc.locale_str, "YES") == 0 ||
	    x_strcasecmp(xw->misc.locale_str, "AUTO") == 0 ||
	    strcmp(xw->misc.locale_str, "1") == 0) {
	/* when true ... fully obeying LC_CTYPE locale */
	xw->misc.callfilter = (Boolean) (is_utf8 ? 0 : 1);
	screen->utf8_mode = uAlways;
    } else if (x_strcasecmp(xw->misc.locale_str, "FALSE") == 0 ||
	       x_strcasecmp(xw->misc.locale_str, "OFF") == 0 ||
	       x_strcasecmp(xw->misc.locale_str, "NO") == 0 ||
	       strcmp(xw->misc.locale_str, "0") == 0) {
	/* when false ... original value of utf8_mode is effective */
	if (screen->utf8_mode == uDefault) {
	    screen->utf8_mode = is_utf8 ? uAlways : uFalse;
	}
    } else if (x_strcasecmp(xw->misc.locale_str, "MEDIUM") == 0 ||
	       x_strcasecmp(xw->misc.locale_str, "SEMIAUTO") == 0) {
	/* when medium ... obeying locale only for UTF-8 and Asian */
	if (is_utf8) {
	    screen->utf8_mode = uAlways;
	} else if (
#ifdef MB_CUR_MAX
		      MB_CUR_MAX > 1 ||
#else
		      !strncmp(xtermEnvLocale(), "ja", (size_t) 2) ||
		      !strncmp(xtermEnvLocale(), "ko", (size_t) 2) ||
		      !strncmp(xtermEnvLocale(), "zh", (size_t) 2) ||
#endif
		      !strncmp(xtermEnvLocale(), "th", (size_t) 2) ||
		      !strncmp(xtermEnvLocale(), "vi", (size_t) 2)) {
	    xw->misc.callfilter = 1;
	    screen->utf8_mode = uAlways;
	} else {
	    screen->utf8_mode = uFalse;
	}
    } else if (x_strcasecmp(xw->misc.locale_str, "UTF-8") == 0 ||
	       x_strcasecmp(xw->misc.locale_str, "UTF8") == 0) {
	/* when UTF-8 ... UTF-8 mode */
	screen->utf8_mode = uAlways;
    } else {
	/* other words are regarded as encoding name passed to luit */
	xw->misc.callfilter = 1;
	screen->utf8_mode = uAlways;
	xw->misc.use_encoding = 1;
    }
    TRACE(("... updated misc.callfilter = %s\n", BtoS(xw->misc.callfilter)));
    TRACE(("... updated misc.use_encoding = %s\n", BtoS(xw->misc.use_encoding)));
#else
    if (screen->utf8_mode == uDefault) {
	screen->utf8_mode = is_utf8 ? uAlways : uFalse;
    }
#endif /* OPT_LUIT_PROG */

    if (screen->utf8_fonts == uDefault) {
	switch (screen->utf8_mode) {
	case uFalse:
	    /* FALLTHRU */
	case uTrue:
	    screen->utf8_fonts = screen->utf8_mode;
	    break;
	case uDefault:
	    /* should not happen */
	    screen->utf8_fonts = uTrue;
	    break;
	case uAlways:
	    /* use this to disable menu entry */
	    break;
	}
    }

    screen->utf8_inparse = (Boolean) (screen->utf8_mode != uFalse);

    TRACE(("... updated screen.utf8_mode = %d\n", screen->utf8_mode));
    TRACE(("... updated screen.utf8_fonts = %d\n", screen->utf8_fonts));
    TRACE(("...VTInitialize_locale done\n"));
}
#endif

void
lookupSelectUnit(XtermWidget xw, Cardinal item, String value)
{
    /* *INDENT-OFF* */
    static const struct {
	const char *	name;
	SelectUnit	code;
    } table[] = {
    	{ "char",	Select_CHAR },
    	{ "word",	Select_WORD },
    	{ "line",	Select_LINE },
    	{ "group",	Select_GROUP },
    	{ "page",	Select_PAGE },
    	{ "all",	Select_ALL },
#if OPT_SELECT_REGEX
    	{ "regex",	Select_REGEX },
#endif
    };
    /* *INDENT-ON* */

    TScreen *screen = TScreenOf(xw);
    String next = x_skip_nonblanks(value);
    Cardinal n;

    screen->selectMap[item] = NSELECTUNITS;
    for (n = 0; n < XtNumber(table); ++n) {
	if (!x_strncasecmp(table[n].name, value, (unsigned) (next - value))) {
	    screen->selectMap[item] = table[n].code;
#if OPT_SELECT_REGEX
	    if (table[n].code == Select_REGEX) {
		screen->selectExpr[item] = x_strtrim(next);
		TRACE(("Parsed regex \"%s\"\n", screen->selectExpr[item]));
	    }
#endif
	    break;
	}
    }
}

static void
ParseOnClicks(XtermWidget wnew, XtermWidget wreq, Cardinal item)
{
    lookupSelectUnit(wnew, item, TScreenOf(wreq)->onClick[item]);
}

/*
 * Parse a comma-separated list, returning a string which the caller must
 * free, and updating the source pointer.
 */
static char *
ParseList(const char **source)
{
    const char *base = *source;
    const char *next;
    char *value = 0;
    char *result;

    /* ignore empty values */
    while (*base == ',')
	++base;

    if (*base != '\0') {
	size_t size;

	next = base;
	while (*next != '\0' && *next != ',')
	    ++next;
	size = (size_t) (1 + next - base);
	value = malloc(size);
	if (value != 0) {
	    memcpy(value, base, size);
	    value[size - 1] = '\0';
	}
	*source = next;
    } else {
	*source = base;
    }
    result = x_strtrim(value);
    free(value);
    return result;
}

static void
set_flags_from_list(char *target,
		    const char *source,
		    const FlagList * list)
{
    Cardinal n;
    int value = -1;

    while (!IsEmpty(source)) {
	char *next = ParseList(&source);
	Boolean found = False;

	if (next == 0)
	    break;
	if (isdigit(CharOf(*next))) {
	    char *temp;

	    value = (int) strtol(next, &temp, 0);
	    if (!FullS2L(next, temp)) {
		xtermWarning("Expected a number: %s\n", next);
	    } else {
		for (n = 0; list[n].name != 0; ++n) {
		    if (list[n].code == value) {
			target[value] = 1;
			found = True;
			TRACE(("...found %s (%d)\n", list[n].name, value));
			break;
		    }
		}
	    }
	} else {
	    for (n = 0; list[n].name != 0; ++n) {
		if (!x_wildstrcmp(next, list[n].name)) {
		    value = list[n].code;
		    target[value] = 1;
		    found = True;
		    TRACE(("...found %s (%d)\n", list[n].name, value));
		}
	    }
	}
	if (!found) {
	    xtermWarning("Unrecognized keyword: %s\n", next);
	}
	free(next);
    }
}

#define InitCursorShape(target, source) \
    target->cursor_shape = source->cursor_underline \
	? CURSOR_UNDERLINE \
	: CURSOR_BLOCK

/* ARGSUSED */
static void
VTInitialize(Widget wrequest,
	     Widget new_arg,
	     ArgList args GCC_UNUSED,
	     Cardinal *num_args GCC_UNUSED)
{
#define Kolor(name) TScreenOf(wnew)->name.resource
#define TxtFg(name) !x_strcasecmp(Kolor(Tcolors[TEXT_FG]), Kolor(name))
#define TxtBg(name) !x_strcasecmp(Kolor(Tcolors[TEXT_BG]), Kolor(name))
#define DftFg(name) isDefaultForeground(Kolor(name))
#define DftBg(name) isDefaultBackground(Kolor(name))

#define DATA_END   { NULL,  -1       }

#if OPT_BLINK_CURS
#define DATA(name) { #name, cb##name }
    static const FlagList tblBlinkOps[] =
    {
	DATA(Always)
	,DATA(Never)
	,DATA_END
    };
#undef DATA
#endif

#define DATA(name) { #name, ec##name }
    static const FlagList tblColorOps[] =
    {
	DATA(SetColor)
	,DATA(GetColor)
	,DATA(GetAnsiColor)
	,DATA_END
    };
#undef DATA

#define DATA(name) { #name, ef##name }
    static const FlagList tblFontOps[] =
    {
	DATA(SetFont)
	,DATA(GetFont)
	,DATA_END
    };
#undef DATA

#define DATA(name) { #name, em##name }
    static const FlagList tblMouseOps[] =
    {
	DATA(X10)
	,DATA(Locator)
	,DATA(VT200Click)
	,DATA(VT200Hilite)
	,DATA(AnyButton)
	,DATA(AnyEvent)
	,DATA(FocusEvent)
	,DATA(Extended)
	,DATA(SGR)
	,DATA(URXVT)
	,DATA(AlternateScroll)
	,DATA_END
    };
#undef DATA

#define DATA(name) { #name, et##name }
    static const FlagList tblTcapOps[] =
    {
	DATA(SetTcap)
	,DATA(GetTcap)
	,DATA_END
    };
#undef DATA

#define DATA(name) { #name, ew##name }
    static const FlagList tblWindowOps[] =
    {
	DATA(RestoreWin)
	,DATA(MinimizeWin)
	,DATA(SetWinPosition)
	,DATA(SetWinSizePixels)
	,DATA(RaiseWin)
	,DATA(LowerWin)
	,DATA(RefreshWin)
	,DATA(SetWinSizeChars)
#if OPT_MAXIMIZE
	,DATA(MaximizeWin)
	,DATA(FullscreenWin)
#endif
	,DATA(GetWinState)
	,DATA(GetWinPosition)
	,DATA(GetWinSizePixels)
	,DATA(GetWinSizeChars)
#if OPT_MAXIMIZE
	,DATA(GetScreenSizeChars)
#endif
	,DATA(GetIconTitle)
	,DATA(GetWinTitle)
	,DATA(PushTitle)
	,DATA(PopTitle)
	,DATA(SetWinLines)
	,DATA(SetXprop)
	,DATA(GetSelection)
	,DATA(SetSelection)
	,DATA_END
    };
#undef DATA

#if OPT_RENDERFONT
#define DATA(name) { #name, er##name }
    static const FlagList tblRenderFont[] =
    {
	DATA(Default)
	,DATA_END
    };
#undef DATA
#endif

#if OPT_WIDE_CHARS
#define DATA(name) { #name, u##name }
    static const FlagList tblUtf8Mode[] =
    {
	DATA(Always)
	,DATA(Default)
	,DATA_END
    };
#undef DATA
#endif

#ifndef NO_ACTIVE_ICON
#define DATA(name) { #name, ei##name }
    static const FlagList tblAIconOps[] =
    {
	DATA(Default)
	,DATA_END
    };
#undef DATA
#endif

#define DATA(name) { #name, eb##name }
    static const FlagList tbl8BitMeta[] =
    {
	DATA(Never)
	,DATA(Locale)
	,DATA_END
    };
#undef DATA

    XtermWidget request = (XtermWidget) wrequest;
    XtermWidget wnew = (XtermWidget) new_arg;
    Widget my_parent = SHELL_OF(wnew);
    int i;
    const char *s;

#if OPT_ISO_COLORS
    Bool color_ok;
#endif

#if OPT_COLOR_RES2
    static XtResource fake_resources[] =
    {
#if OPT_256_COLORS
# include <256colres.h>
#elif OPT_88_COLORS
# include <88colres.h>
#endif
    };
#endif /* OPT_COLOR_RES2 */
    TScreen *screen = TScreenOf(wnew);

#if OPT_TRACE
    check_tables();
#endif

    TRACE(("VTInitialize wnew %p, %d / %d resources\n",
	   (void *) wnew, XtNumber(xterm_resources), MAXRESOURCES));
    assert(XtNumber(xterm_resources) < MAXRESOURCES);

    /* Zero out the entire "screen" component of "wnew" widget, then do
     * field-by-field assignment of "screen" fields that are named in the
     * resource list.
     */
    memset(screen, 0, sizeof(wnew->screen));

    /* DESCO Sys#67660
     * Zero out the entire "keyboard" component of "wnew" widget.
     */
    memset(&wnew->keyboard, 0, sizeof(wnew->keyboard));

    /*
     * The workspace has no resources - clear it.
     */
    memset(&wnew->work, 0, sizeof(wnew->work));

    /* dummy values so that we don't try to Realize the parent shell with height
     * or width of 0, which is illegal in X.  The real size is computed in the
     * xtermWidget's Realize proc, but the shell's Realize proc is called first,
     * and must see a valid size.
     */
    wnew->core.height = wnew->core.width = 1;

    /*
     * The definition of -rv now is that it changes the definition of
     * XtDefaultForeground and XtDefaultBackground.  So, we no longer
     * need to do anything special.
     */
    screen->display = wnew->core.screen->display;

    /* prep getVisualInfo() */
    wnew->visInfo = 0;
    wnew->numVisuals = 0;
    (void) getVisualInfo(wnew);

    /*
     * We use the default foreground/background colors to compare/check if a
     * color-resource has been set.
     */
#define MyBlackPixel(dpy) BlackPixel(dpy,DefaultScreen(dpy))
#define MyWhitePixel(dpy) WhitePixel(dpy,DefaultScreen(dpy))

    if (request->misc.re_verse) {
	wnew->dft_foreground = MyWhitePixel(screen->display);
	wnew->dft_background = MyBlackPixel(screen->display);
    } else {
	wnew->dft_foreground = MyBlackPixel(screen->display);
	wnew->dft_background = MyWhitePixel(screen->display);
    }

    init_Tres(TEXT_FG);
    init_Tres(TEXT_BG);
    repairColors(wnew);

    wnew->old_foreground = T_COLOR(screen, TEXT_FG);
    wnew->old_background = T_COLOR(screen, TEXT_BG);

    TRACE(("Color resource initialization:\n"));
    TRACE(("   Default foreground 0x%06lx\n", wnew->dft_foreground));
    TRACE(("   Default background 0x%06lx\n", wnew->dft_background));
    TRACE(("   Screen foreground  0x%06lx\n", T_COLOR(screen, TEXT_FG)));
    TRACE(("   Screen background  0x%06lx\n", T_COLOR(screen, TEXT_BG)));
    TRACE(("   Actual  foreground 0x%06lx\n", wnew->old_foreground));
    TRACE(("   Actual  background 0x%06lx\n", wnew->old_background));

    screen->mouse_button = 0;
    screen->mouse_row = -1;
    screen->mouse_col = -1;

#if OPT_BOX_CHARS
    init_Bres(screen.force_box_chars);
    init_Bres(screen.force_packed);
    init_Bres(screen.force_all_chars);
    init_Bres(screen.assume_all_chars);
#endif
    init_Bres(screen.free_bold_box);
    init_Bres(screen.allowBoldFonts);

    init_Bres(screen.c132);
    init_Bres(screen.curses);
    init_Bres(screen.hp_ll_bc);
#if OPT_XMC_GLITCH
    init_Ires(screen.xmc_glitch);
    init_Ires(screen.xmc_attributes);
    init_Bres(screen.xmc_inline);
    init_Bres(screen.move_sgr_ok);
#endif
#if OPT_BLINK_CURS
    init_Sres(screen.cursor_blink_s);
    wnew->screen.cursor_blink =
	extendedBoolean(wnew->screen.cursor_blink_s,
			tblBlinkOps, cbLAST);
    init_Bres(screen.cursor_blink_xor);
    init_Ires(screen.blink_on);
    init_Ires(screen.blink_off);
#endif
    init_Bres(screen.cursor_underline);
    /* resources allow for underline or block, not (yet) bar */
    InitCursorShape(screen, TScreenOf(request));
#if OPT_BLINK_CURS
    TRACE(("cursor_shape:%d blinks:%d\n",
	   screen->cursor_shape,
	   screen->cursor_blink));
#endif
#if OPT_BLINK_TEXT
    init_Ires(screen.blink_as_bold);
#endif
    init_Ires(screen.border);
    init_Bres(screen.jumpscroll);
    init_Bres(screen.fastscroll);

    init_Bres(screen.old_fkeys);
    wnew->screen.old_fkeys0 = wnew->screen.old_fkeys;
    wnew->keyboard.type = screen->old_fkeys
	? keyboardIsLegacy
	: keyboardIsDefault;

    init_Mres(screen.delete_is_del);
#ifdef ALLOWLOGGING
    init_Bres(misc.logInhibit);
    init_Bres(misc.log_on);
    init_Sres(screen.logfile);
#endif
    init_Bres(screen.bellIsUrgent);
    init_Bres(screen.bellOnReset);
    init_Bres(screen.marginbell);
    init_Bres(screen.multiscroll);
    init_Ires(screen.nmarginbell);
    init_Ires(screen.savelines);
    init_Ires(screen.scrollBarBorder);
    init_Ires(screen.scrolllines);
    init_Bres(screen.alternateScroll);
    init_Bres(screen.scrollttyoutput);
    init_Bres(screen.scrollkey);

    init_Dres(screen.scale_height);
    if (screen->scale_height < 0.9)
	screen->scale_height = (float) 0.9;
    if (screen->scale_height > 1.5)
	screen->scale_height = (float) 1.5;

    init_Bres(misc.autoWrap);
    init_Bres(misc.login_shell);
    init_Bres(misc.reverseWrap);
    init_Bres(misc.scrollbar);
    init_Sres(misc.geo_metry);
    init_Sres(misc.T_geometry);

    init_Sres(screen.term_id);
    for (s = TScreenOf(request)->term_id; *s; s++) {
	if (!isalpha(CharOf(*s)))
	    break;
    }
    screen->terminal_id = atoi(s);
    if (screen->terminal_id < MIN_DECID)
	screen->terminal_id = MIN_DECID;
    if (screen->terminal_id > MAX_DECID)
	screen->terminal_id = MAX_DECID;
    TRACE(("term_id '%s' -> terminal_id %d\n",
	   screen->term_id,
	   screen->terminal_id));

    screen->vtXX_level = (screen->terminal_id / 100);

    init_Ires(screen.title_modes);
    screen->title_modes0 = screen->title_modes;

    init_Ires(screen.nextEventDelay);
    if (screen->nextEventDelay <= 0)
	screen->nextEventDelay = 1;

    init_Bres(screen.visualbell);
    init_Bres(screen.flash_line);
    init_Ires(screen.visualBellDelay);
    init_Bres(screen.poponbell);

    init_Bres(screen.eraseSavedLines0);
    screen->eraseSavedLines = screen->eraseSavedLines0;

    init_Ires(misc.limit_resize);

#if OPT_NUM_LOCK
    init_Bres(misc.real_NumLock);
    init_Bres(misc.alwaysUseMods);
#endif

#if OPT_INPUT_METHOD
    init_Bres(misc.open_im);
    init_Ires(misc.retry_im);
    init_Sres(misc.f_x);
    init_Sres(misc.input_method);
    init_Sres(misc.preedit_type);
#endif

#if OPT_SHIFT_FONTS
    init_Bres(misc.shift_fonts);
#endif
#if OPT_SUNPC_KBD
    init_Ires(misc.ctrl_fkeys);
#endif
#if OPT_TEK4014
    TEK4014_SHOWN(wnew) = False;	/* not a resource... */
    init_Bres(misc.tekInhibit);
    init_Bres(misc.tekSmall);
    init_Bres(misc.TekEmu);
#endif
#if OPT_TCAP_QUERY
    screen->tc_query_code = -1;
#endif
    wnew->misc.re_verse0 = request->misc.re_verse;
    init_Bres(misc.re_verse);
    init_Ires(screen.multiClickTime);
    init_Ires(screen.bellSuppressTime);
    init_Sres(screen.charClass);

    init_Bres(screen.always_highlight);
    init_Bres(screen.brokenSelections);
    init_Bres(screen.cutNewline);
    init_Bres(screen.cutToBeginningOfLine);
    init_Bres(screen.highlight_selection);
    init_Bres(screen.show_wrap_marks);
    init_Bres(screen.i18nSelections);
    init_Bres(screen.keepClipboard);
    init_Bres(screen.keepSelection);
    init_Bres(screen.selectToClipboard);
    init_Bres(screen.trim_selection);

    screen->pointer_cursor = TScreenOf(request)->pointer_cursor;
    init_Ires(screen.pointer_mode);
    wnew->screen.pointer_mode0 = wnew->screen.pointer_mode;

    init_Sres(screen.answer_back);

    wnew->SPS.printer_checked = False;
    init_Sres(SPS.printer_command);
    init_Bres(SPS.printer_autoclose);
    init_Bres(SPS.printer_extent);
    init_Bres(SPS.printer_formfeed);
    init_Bres(SPS.printer_newline);
    init_Ires(SPS.printer_controlmode);
#if OPT_PRINT_COLORS
    init_Ires(SPS.print_attributes);
#endif

    init_Sres(screen.keyboard_dialect);

    init_Bres(screen.input_eight_bits);
    init_Bres(screen.output_eight_bits);
    init_Bres(screen.control_eight_bits);
    init_Bres(screen.backarrow_key);
    init_Bres(screen.alt_is_not_meta);
    init_Bres(screen.alt_sends_esc);
    init_Bres(screen.meta_sends_esc);

    init_Bres(screen.allowPasteControl0);
    init_Bres(screen.allowSendEvent0);
    init_Bres(screen.allowColorOp0);
    init_Bres(screen.allowFontOp0);
    init_Bres(screen.allowMouseOp0);
    init_Bres(screen.allowTcapOp0);
    init_Bres(screen.allowTitleOp0);
    init_Bres(screen.allowWindowOp0);

#if OPT_SCROLL_LOCK
    init_Bres(screen.allowScrollLock0);
#endif

    init_Sres(screen.disallowedColorOps);

    set_flags_from_list(screen->disallow_color_ops,
			screen->disallowedColorOps,
			tblColorOps);

    init_Sres(screen.disallowedFontOps);

    set_flags_from_list(screen->disallow_font_ops,
			screen->disallowedFontOps,
			tblFontOps);

    init_Sres(screen.disallowedMouseOps);

    set_flags_from_list(screen->disallow_mouse_ops,
			screen->disallowedMouseOps,
			tblMouseOps);

    init_Sres(screen.disallowedTcapOps);

    set_flags_from_list(screen->disallow_tcap_ops,
			screen->disallowedTcapOps,
			tblTcapOps);

    init_Sres(screen.disallowedWinOps);

    set_flags_from_list(screen->disallow_win_ops,
			screen->disallowedWinOps,
			tblWindowOps);

    init_Sres(screen.default_string);
    init_Sres(screen.eightbit_select_types);
#if OPT_WIDE_CHARS
    init_Sres(screen.utf8_select_types);
#endif

    /* make a copy so that editres cannot change the resource after startup */
    screen->allowPasteControls = screen->allowPasteControl0;
    screen->allowSendEvents = screen->allowSendEvent0;
    screen->allowColorOps = screen->allowColorOp0;
    screen->allowFontOps = screen->allowFontOp0;
    screen->allowMouseOps = screen->allowMouseOp0;
    screen->allowTcapOps = screen->allowTcapOp0;
    screen->allowTitleOps = screen->allowTitleOp0;
    screen->allowWindowOps = screen->allowWindowOp0;

#if OPT_SCROLL_LOCK
    screen->allowScrollLock = screen->allowScrollLock0;
#endif

    init_Bres(screen.quiet_grab);

#ifndef NO_ACTIVE_ICON
    init_Sres(screen.icon_fontname);
    getIconicFont(screen)->fs = XLoadQueryFont(screen->display,
					       screen->icon_fontname);
    TRACE(("iconFont '%s' %sloaded successfully\n",
	   screen->icon_fontname,
	   getIconicFont(screen)->fs ? "" : "NOT "));
    init_Sres(misc.active_icon_s);
    wnew->work.active_icon =
	(Boolean) extendedBoolean(wnew->misc.active_icon_s,
				  tblAIconOps, eiLAST);
    init_Ires(misc.icon_border_width);
    wnew->misc.icon_border_pixel = request->misc.icon_border_pixel;
#endif /* NO_ACTIVE_ICON */

    init_Bres(misc.signalInhibit);
    init_Bres(misc.titeInhibit);
    init_Bres(misc.tiXtraScroll);
    init_Bres(misc.cdXtraScroll);
    init_Bres(misc.dynamicColors);

#if OPT_DEC_CHRSET
    for (i = 0; i < NUM_CHRSET; i++) {
	screen->double_fonts[i].warn = fwResource;
    }
#endif
    for (i = fontMenu_font1; i <= fontMenu_lastBuiltin; i++) {
	init_Sres2(screen.MenuFontName, i);
    }
    for (i = 0; i < fMAX; i++) {
	screen->fnts[i].warn = fwResource;
#if OPT_WIDE_ATTRS
	screen->ifnts[i].warn = fwResource;
#endif
    }
#ifndef NO_ACTIVE_ICON
    screen->fnt_icon.warn = fwResource;
#endif

    init_Ires(misc.fontWarnings);

    initFontLists(wnew);

#define DefaultFontNames screen->menu_font_names[fontMenu_default]

    /*
     * Process Xft font resources first, since faceName may contain X11 fonts
     * that should override the "font" resource.
     */
#if OPT_RENDERFONT
    for (i = 0; i <= fontMenu_lastBuiltin; ++i) {
	init_Dres2(misc.face_size, i);
    }

#define ALLOC_FONTLIST(name,which,field) \
    init_Sres(misc.default_xft.field);\
    allocFontList(wnew,\
		  name,\
		  &(wnew->work.fonts),\
		  which,\
		  wnew->misc.default_xft.field,\
		  True)

    ALLOC_FONTLIST(XtNfaceName, fNorm, f_n);

#if OPT_WIDE_CHARS
    ALLOC_FONTLIST(XtNfaceNameDoublesize, fWide, f_w);
#endif

#undef ALLOC_FONTLIST

#endif

    /*
     * Process X11 (XLFD) font specifications.
     */
#define ALLOC_FONTLIST(name,which,field) \
    init_Sres(misc.default_font.field);\
    allocFontList(wnew,\
		  name,\
		  &(wnew->work.fonts),\
		  which,\
		  wnew->misc.default_font.field,\
		  False)

    ALLOC_FONTLIST(XtNfont, fNorm, f_n);
    ALLOC_FONTLIST(XtNboldFont, fBold, f_b);

    DefaultFontNames[fNorm] = x_strdup(DefaultFontN(wnew));
    DefaultFontNames[fBold] = x_strdup(DefaultFontB(wnew));

#if OPT_WIDE_CHARS
    ALLOC_FONTLIST(XtNwideFont, fWide, f_w);
    ALLOC_FONTLIST(XtNwideBoldFont, fWBold, f_wb);

    DefaultFontNames[fWide] = x_strdup(DefaultFontW(wnew));
    DefaultFontNames[fWBold] = x_strdup(DefaultFontWB(wnew));
#endif

#undef ALLOC_FONTLIST

    screen->EscapeFontName() = NULL;
    screen->SelectFontName() = NULL;

    screen->menu_font_number = fontMenu_default;
    init_Sres(screen.initial_font);
    if (screen->initial_font != 0) {
	int result = xtermGetFont(screen->initial_font);
	if (result >= 0)
	    screen->menu_font_number = result;
    }
#if OPT_BROKEN_OSC
    init_Bres(screen.brokenLinuxOSC);
#endif

#if OPT_BROKEN_ST
    init_Bres(screen.brokenStringTerm);
#endif

#if OPT_C1_PRINT
    init_Bres(screen.c1_printable);
#endif

#if OPT_CLIP_BOLD
    init_Bres(screen.use_clipping);
#endif

#if OPT_DEC_CHRSET
    init_Bres(screen.font_doublesize);
    init_Ires(screen.cache_doublesize);
    if (screen->cache_doublesize > NUM_CHRSET)
	screen->cache_doublesize = NUM_CHRSET;
    if (screen->cache_doublesize == 0)
	screen->font_doublesize = False;
    TRACE(("Doublesize%s enabled, up to %d fonts\n",
	   screen->font_doublesize ? "" : " not",
	   screen->cache_doublesize));
#endif

#if OPT_ISO_COLORS
    init_Ires(screen.veryBoldColors);
    init_Bres(screen.boldColors);
    init_Bres(screen.colorAttrMode);
    init_Bres(screen.colorBDMode);
    init_Bres(screen.colorBLMode);
    init_Bres(screen.colorMode);
    init_Bres(screen.colorULMode);
    init_Bres(screen.italicULMode);
    init_Bres(screen.colorRVMode);

#if OPT_WIDE_ATTRS
    init_Bres(screen.colorITMode);
#endif
#if OPT_DIRECT_COLOR
    init_Bres(screen.direct_color);
#endif

#if OPT_COLOR_RES2
    TRACE(("...will fake resources for color%d to color%d\n",
	   MIN_ANSI_COLORS,
	   NUM_ANSI_COLORS - 1));
#endif
    for (i = 0, color_ok = False; i < MAXCOLORS; i++) {

#if OPT_COLOR_RES2
	/*
	 * Xt has a hardcoded limit on the maximum number of resources that can
	 * be used in a widget.  If we configure both luit (which implies
	 * wide-characters) and 256-colors, it goes over that limit.  Most
	 * people would not need a resource-file with 256-colors; the default
	 * values in our table are sufficient.  In that case, fake the resource
	 * setting by copying the default value from the table.  The #define's
	 * can be overridden to make these true resources.
	 */
	if (i >= MIN_ANSI_COLORS && i < NUM_ANSI_COLORS) {
	    screen->Acolors[i].resource =
		x_strtrim(fake_resources[i - MIN_ANSI_COLORS].default_addr);
	    if (screen->Acolors[i].resource == 0)
		screen->Acolors[i].resource = XtDefaultForeground;
	} else
#endif /* OPT_COLOR_RES2 */
	{
	    screen->Acolors[i] = TScreenOf(request)->Acolors[i];
	    screen->Acolors[i].resource =
		x_strtrim(screen->Acolors[i].resource);
	}

#if OPT_COLOR_RES
	TRACE(("Acolors[%d] = %s\n", i, screen->Acolors[i].resource));
	screen->Acolors[i].mode = False;
	if (DftFg(Acolors[i])) {
	    screen->Acolors[i].value = T_COLOR(screen, TEXT_FG);
	    screen->Acolors[i].mode = True;
	} else if (DftBg(Acolors[i])) {
	    screen->Acolors[i].value = T_COLOR(screen, TEXT_BG);
	    screen->Acolors[i].mode = True;
	} else {
	    color_ok = True;
	}
#else
	TRACE(("Acolors[%d] = %#lx\n", i, TScreenOf(request)->Acolors[i]));
	if (screen->Acolors[i] != wnew->dft_foreground &&
	    screen->Acolors[i] != T_COLOR(screen, TEXT_FG) &&
	    screen->Acolors[i] != T_COLOR(screen, TEXT_BG))
	    color_ok = True;
#endif
    }

    /*
     * Check if we're trying to use color in a monochrome screen.  Disable
     * color in that case, since that would make ANSI colors unusable.  A 4-bit
     * or 8-bit display is usable, so we do not have to check for anything more
     * specific.
     */
    if (color_ok) {
	if (getVisualDepth(wnew) <= 1) {
	    TRACE(("disabling color since screen is monochrome\n"));
	    color_ok = False;
	}
    }

    /* If none of the colors are anything other than the foreground or
     * background, we'll assume this isn't color, no matter what the colorMode
     * resource says.  (There doesn't seem to be any good way to determine if
     * the resource lookup failed versus the user having misconfigured this).
     */
    if (!color_ok) {
	screen->colorMode = False;
	TRACE(("All colors are foreground or background: disable colorMode\n"));
    }
    wnew->sgr_foreground = -1;
    wnew->sgr_background = -1;
    clrDirectFG(wnew->flags);
    clrDirectFG(wnew->flags);
#endif /* OPT_ISO_COLORS */

    /*
     * Decode the resources that control the behavior on multiple mouse clicks.
     * A single click is always bound to normal character selection, but the
     * other flavors can be changed.
     */
    for (i = 0; i < NSELECTUNITS; ++i) {
	int ck = (i + 1);
	screen->maxClicks = ck;
	if (i == Select_CHAR)
	    screen->selectMap[i] = Select_CHAR;
	else if (TScreenOf(request)->onClick[i] != 0)
	    ParseOnClicks(wnew, request, (unsigned) i);
	else if (i <= Select_LINE)
	    screen->selectMap[i] = (SelectUnit) i;
	else
	    break;
	TRACE(("on%dClicks %s=%d\n", ck,
	       NonNull(TScreenOf(request)->onClick[i]),
	       screen->selectMap[i]));
	if (screen->selectMap[i] == NSELECTUNITS)
	    break;
    }
    TRACE(("maxClicks %d\n", screen->maxClicks));

    init_Tres(MOUSE_FG);
    init_Tres(MOUSE_BG);
    init_Tres(TEXT_CURSOR);
#if OPT_HIGHLIGHT_COLOR
    init_Tres(HIGHLIGHT_BG);
    init_Tres(HIGHLIGHT_FG);
    init_Bres(screen.hilite_reverse);
    init_Mres(screen.hilite_color);
    if (screen->hilite_color == Maybe) {
	screen->hilite_color = False;
#if OPT_COLOR_RES
	/*
	 * If the highlight text/background are both set, and if they are
	 * not equal to either the text/background or background/text, then
	 * set the highlightColorMode automatically.
	 */
	if (!DftFg(Tcolors[HIGHLIGHT_BG])
	    && !DftBg(Tcolors[HIGHLIGHT_FG])
	    && !TxtFg(Tcolors[HIGHLIGHT_BG])
	    && !TxtBg(Tcolors[HIGHLIGHT_FG])
	    && !TxtBg(Tcolors[HIGHLIGHT_BG])
	    && !TxtFg(Tcolors[HIGHLIGHT_FG])) {
	    TRACE(("...setting hilite_color automatically\n"));
	    screen->hilite_color = True;
	}
#endif
    }
#endif

#if OPT_TEK4014
    /*
     * The Tek4014 window has no separate resources for foreground, background
     * and cursor color.  Since xterm always creates the vt100 widget first, we
     * can set the Tektronix colors here.  That lets us use escape sequences to
     * set its dynamic colors and get consistent behavior whether or not the
     * window is displayed.
     */
    screen->Tcolors[TEK_BG] = screen->Tcolors[TEXT_BG];
    screen->Tcolors[TEK_FG] = screen->Tcolors[TEXT_FG];
    screen->Tcolors[TEK_CURSOR] = screen->Tcolors[TEXT_CURSOR];
#endif

#ifdef SCROLLBAR_RIGHT
    init_Bres(misc.useRight);
#endif

#if OPT_RENDERFONT
    init_Sres(misc.render_font_s);
    wnew->work.render_font =
	(Boolean) extendedBoolean(wnew->misc.render_font_s,
				  tblRenderFont, erLast);
    if (wnew->work.render_font == erDefault) {
	if (IsEmpty(CurrentXftFont(wnew))) {
	    free((void *) CurrentXftFont(wnew));
	    CurrentXftFont(wnew) = x_strdup(DEFFACENAME_AUTO);
	    TRACE(("will allow runtime switch to render_font using \"%s\"\n",
		   CurrentXftFont(wnew)));
	} else {
	    wnew->work.render_font = erTrue;
	    TRACE(("initially using TrueType font\n"));
	}
    }
    /* minor tweak to make debug traces consistent: */
    if (wnew->work.render_font) {
	if (IsEmpty(CurrentXftFont(wnew))) {
	    wnew->work.render_font = False;
	    TRACE(("reset render_font since there is no face_name\n"));
	}
    }
#endif

#if OPT_WIDE_CHARS
    /* setup data for next call */
    request->screen.utf8_mode =
	extendedBoolean(request->screen.utf8_mode_s, tblUtf8Mode, uLast);
    request->screen.utf8_fonts =
	extendedBoolean(request->screen.utf8_fonts_s, tblUtf8Mode, uLast);

    /*
     * Make a copy in the input/request so that DefaultFontN() works for
     * the "CHECKFONT" option.
     */
    copyFontList(&(request->work.fonts.x11.list_n),
		 wnew->work.fonts.x11.list_n);

    VTInitialize_locale(request);
    init_Bres(screen.normalized_c);
    init_Bres(screen.utf8_latin1);
    init_Bres(screen.utf8_title);

#if OPT_LUIT_PROG
    init_Bres(misc.callfilter);
    init_Bres(misc.use_encoding);
    init_Sres(misc.locale_str);
    init_Sres(misc.localefilter);
#endif

    init_Ires(screen.utf8_inparse);
    init_Ires(screen.utf8_mode);
    init_Ires(screen.utf8_fonts);
    init_Ires(screen.max_combining);

    init_Ires(screen.utf8_always);	/* from utf8_mode, used in doparse */

    if (screen->max_combining < 0) {
	screen->max_combining = 0;
    }
    if (screen->max_combining > 5) {
	screen->max_combining = 5;
    }

    init_Bres(screen.vt100_graphics);
    init_Bres(screen.wide_chars);
    init_Bres(misc.mk_width);
    init_Bres(misc.cjk_width);

    init_Ires(misc.mk_samplesize);
    init_Ires(misc.mk_samplepass);

    if (wnew->misc.mk_samplesize > 0xffff)
	wnew->misc.mk_samplesize = 0xffff;
    if (wnew->misc.mk_samplesize < 0)
	wnew->misc.mk_samplesize = 0;

    if (wnew->misc.mk_samplepass > wnew->misc.mk_samplesize)
	wnew->misc.mk_samplepass = wnew->misc.mk_samplesize;
    if (wnew->misc.mk_samplepass < 0)
	wnew->misc.mk_samplepass = 0;

    if (TScreenOf(request)->utf8_mode) {
	TRACE(("setting wide_chars on\n"));
	screen->wide_chars = True;
    } else {
	TRACE(("setting utf8_mode to 0\n"));
	screen->utf8_mode = uFalse;
    }
    mk_wcwidth_init(screen->utf8_mode);
    TRACE(("initialized UTF-8 mode to %d\n", screen->utf8_mode));

#if OPT_MINI_LUIT
    if (TScreenOf(request)->latin9_mode) {
	screen->latin9_mode = True;
    }
    if (TScreenOf(request)->unicode_font) {
	screen->unicode_font = True;
    }
    TRACE(("initialized Latin9 mode to %d\n", screen->latin9_mode));
    TRACE(("initialized unicode_font to %d\n", screen->unicode_font));
#endif

    decode_wcwidth(wnew);
    xtermSaveVTFonts(wnew);
#endif /* OPT_WIDE_CHARS */

    init_Sres(screen.eight_bit_meta_s);
    wnew->screen.eight_bit_meta =
	extendedBoolean(request->screen.eight_bit_meta_s, tbl8BitMeta, uLast);
    if (wnew->screen.eight_bit_meta == ebLocale) {
#if OPT_WIDE_CHARS
	if (xtermEnvUTF8()) {
	    wnew->screen.eight_bit_meta = ebFalse;
	    TRACE(("...eightBitMeta is false due to locale\n"));
	} else
#endif /* OPT_WIDE_CHARS */
	{
	    wnew->screen.eight_bit_meta = ebTrue;
	    TRACE(("...eightBitMeta is true due to locale\n"));
	}
    }

    init_Bres(screen.always_bold_mode);
    init_Bres(screen.bold_mode);
    init_Bres(screen.underline);

    wnew->cur_foreground = 0;
    wnew->cur_background = 0;

    wnew->keyboard.flags = MODE_SRM;

    if (screen->backarrow_key)
	wnew->keyboard.flags |= MODE_DECBKM;
    TRACE(("initialized DECBKM %s\n",
	   BtoS(wnew->keyboard.flags & MODE_DECBKM)));

#if OPT_SIXEL_GRAPHICS
    init_Bres(screen.sixel_scrolling);
    if (screen->sixel_scrolling)
	wnew->keyboard.flags |= MODE_DECSDM;
    TRACE(("initialized DECSDM %s\n",
	   BtoS(wnew->keyboard.flags & MODE_DECSDM)));
#endif

#if OPT_GRAPHICS
    init_Ires(screen.numcolorregisters);
    TRACE(("initialized NUM_COLOR_REGISTERS to resource default: %d\n",
	   screen->numcolorregisters));

    init_Bres(screen.privatecolorregisters);	/* FIXME: should this be off unconditionally here? */
    TRACE(("initialized PRIVATE_COLOR_REGISTERS to resource default: %s\n",
	   BtoS(screen->privatecolorregisters)));
#endif

#if OPT_GRAPHICS
    {
	int native_w, native_h;

	switch (screen->terminal_id) {
	case 125:
	    native_w = 768;
	    native_h = 460;
	    break;
	case 240:
	    native_w = 800;
	    native_h = 460;
	    break;
	case 241:
	    native_w = 800;
	    native_h = 460;
	    break;
	case 330:
	    native_w = 800;
	    native_h = 480;
	    break;
	case 340:
	    /* FALLTHRU */
	default:
	    native_w = 800;
	    native_h = 480;
	    break;
	case 382:
	    native_w = 960;
	    native_h = 750;
	    break;
	}

# if OPT_REGIS_GRAPHICS
	init_Sres(screen.graphics_regis_default_font);
	TRACE(("default ReGIS font: %s\n",
	       screen->graphics_regis_default_font));

	init_Sres(screen.graphics_regis_screensize);
	screen->graphics_regis_def_high = 1000;
	screen->graphics_regis_def_wide = 1000;
	if (!x_strcasecmp(screen->graphics_regis_screensize, "auto")) {
	    TRACE(("setting default ReGIS screensize based on terminal_id %d\n",
		   screen->terminal_id));
	    screen->graphics_regis_def_high = (Dimension) native_w;
	    screen->graphics_regis_def_wide = (Dimension) native_h;
	} else {
	    int conf_high;
	    int conf_wide;
	    char ignore;

	    if (sscanf(screen->graphics_regis_screensize,
		       "%dx%d%c",
		       &conf_wide,
		       &conf_high,
		       &ignore) == 2) {
		if (conf_high > 0 && conf_wide > 0) {
		    screen->graphics_regis_def_high =
			(Dimension) conf_high;
		    screen->graphics_regis_def_wide =
			(Dimension) conf_wide;
		} else {
		    TRACE(("ignoring invalid regisScreenSize %s\n",
			   screen->graphics_regis_screensize));
		}
	    } else {
		TRACE(("ignoring invalid regisScreenSize %s\n",
		       screen->graphics_regis_screensize));
	    }
	}
	TRACE(("default ReGIS graphics screensize %dx%d\n",
	       (int) screen->graphics_regis_def_wide,
	       (int) screen->graphics_regis_def_high));
# endif

	init_Sres(screen.graphics_max_size);
	screen->graphics_max_high = 1000;
	screen->graphics_max_wide = 1000;
	if (!x_strcasecmp(screen->graphics_max_size, "auto")) {
	    TRACE(("setting max graphics screensize based on terminal_id %d\n",
		   screen->terminal_id));
	    screen->graphics_max_high = (Dimension) native_w;
	    screen->graphics_max_wide = (Dimension) native_h;
	} else {
	    int conf_high;
	    int conf_wide;
	    char ignore;

	    if (sscanf(screen->graphics_max_size,
		       "%dx%d%c",
		       &conf_wide,
		       &conf_high,
		       &ignore) == 2) {
		if (conf_high > 0 && conf_wide > 0) {
		    screen->graphics_max_high = (Dimension) conf_high;
		    screen->graphics_max_wide = (Dimension) conf_wide;
		} else {
		    TRACE(("ignoring invalid maxGraphicSize %s\n",
			   screen->graphics_max_size));
		}
	    } else {
		TRACE(("ignoring invalid maxGraphicSize %s\n",
		       screen->graphics_max_size));
	    }
	}
# if OPT_REGIS_GRAPHICS
	/* Make sure the max is large enough for the default ReGIS size. */
	if (screen->graphics_regis_def_high >
	    screen->graphics_max_high) {
	    screen->graphics_max_high =
		screen->graphics_regis_def_high;
	}
	if (screen->graphics_regis_def_wide >
	    screen->graphics_max_wide) {
	    screen->graphics_max_wide =
		screen->graphics_regis_def_wide;
	}
# endif
	TRACE(("max graphics screensize %dx%d\n",
	       (int) screen->graphics_max_wide,
	       (int) screen->graphics_max_high));
    }
#endif

#if OPT_SIXEL_GRAPHICS
    init_Bres(screen.sixel_scrolls_right);
    TRACE(("initialized SIXEL_SCROLLS_RIGHT to resource default: %s\n",
	   BtoS(screen->sixel_scrolls_right)));
#endif

    /* look for focus related events on the shell, because we need
     * to care about the shell's border being part of our focus.
     */
    TRACE(("adding event handlers for my_parent %p\n", (void *) my_parent));
    XtAddEventHandler(my_parent, EnterWindowMask, False,
		      HandleEnterWindow, (Opaque) NULL);
    XtAddEventHandler(my_parent, LeaveWindowMask, False,
		      HandleLeaveWindow, (Opaque) NULL);
    XtAddEventHandler(my_parent, FocusChangeMask, False,
		      HandleFocusChange, (Opaque) NULL);
    XtAddEventHandler((Widget) wnew, 0L, True,
		      VTNonMaskableEvent, (Opaque) NULL);
    XtAddEventHandler((Widget) wnew, PropertyChangeMask, False,
		      HandleBellPropertyChange, (Opaque) NULL);

#if HANDLE_STRUCT_NOTIFY
#if OPT_TOOLBAR
    wnew->VT100_TB_INFO(menu_bar) = request->VT100_TB_INFO(menu_bar);
    init_Ires(VT100_TB_INFO(menu_height));
#else
    /* Flag icon name with "***"  on window output when iconified.
     * Put in a handler that will tell us when we get Map/Unmap events.
     */
    if (resource.zIconBeep)
#endif
	XtAddEventHandler(my_parent, StructureNotifyMask, False,
			  HandleStructNotify, (Opaque) 0);
#endif /* HANDLE_STRUCT_NOTIFY */

    screen->bellInProgress = False;

    set_character_class(screen->charClass);
#if OPT_REPORT_CCLASS
    if (resource.reportCClass)
	report_char_class(wnew);
#endif

    /* create it, but don't realize it */
    ScrollBarOn(wnew, True);

    /* make sure that the resize gravity acceptable */
    if (!GravityIsNorthWest(wnew) &&
	!GravityIsSouthWest(wnew)) {
	char value[80];
	String temp[2];
	Cardinal nparams = 1;

	sprintf(value, "%d", wnew->misc.resizeGravity);
	temp[0] = value;
	temp[1] = 0;
	XtAppWarningMsg(app_con, "rangeError", "resizeGravity", "XTermError",
			"unsupported resizeGravity resource value (%s)",
			temp, &nparams);
	wnew->misc.resizeGravity = SouthWestGravity;
    }
#ifndef NO_ACTIVE_ICON
    screen->whichVwin = &screen->fullVwin;
#endif /* NO_ACTIVE_ICON */

    if (screen->savelines < 0)
	screen->savelines = 0;

    init_Bres(screen.awaitInput);

    wnew->flags = 0;
    if (!screen->jumpscroll)
	wnew->flags |= SMOOTHSCROLL;
    if (wnew->misc.reverseWrap)
	wnew->flags |= REVERSEWRAP;
    if (wnew->misc.autoWrap)
	wnew->flags |= WRAPAROUND;
    if (wnew->misc.re_verse != wnew->misc.re_verse0)
	wnew->flags |= REVERSE_VIDEO;
    if (screen->c132)
	wnew->flags |= IN132COLUMNS;

    wnew->initflags = wnew->flags;

#if OPT_MOD_FKEYS
    init_Ires(keyboard.modify_1st.allow_keys);
    init_Ires(keyboard.modify_1st.cursor_keys);
    init_Ires(keyboard.modify_1st.function_keys);
    init_Ires(keyboard.modify_1st.keypad_keys);
    init_Ires(keyboard.modify_1st.other_keys);
    init_Ires(keyboard.modify_1st.string_keys);
    init_Ires(keyboard.format_keys);
    wnew->keyboard.modify_now = wnew->keyboard.modify_1st;
#endif

    init_Ires(misc.appcursorDefault);
    if (wnew->misc.appcursorDefault)
	wnew->keyboard.flags |= MODE_DECCKM;

    init_Ires(misc.appkeypadDefault);
    if (wnew->misc.appkeypadDefault)
	wnew->keyboard.flags |= MODE_DECKPAM;

    initLineData(wnew);
#if OPT_WIDE_CHARS
    freeFontList(&(request->work.fonts.x11.list_n));
#endif
    return;
}

void
releaseCursorGCs(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    VTwin *win = WhichVWin(screen);
    int n;

    for_each_curs_gc(n) {
	freeCgs(xw, win, (CgsEnum) n);
    }
}

void
releaseWindowGCs(XtermWidget xw, VTwin *win)
{
    int n;

    for_each_text_gc(n) {
	freeCgs(xw, win, (CgsEnum) n);
    }
}

#define TRACE_FREE_LEAK(name) \
	if (name) { \
	    TRACE(("freed " #name ": %p\n", (const void *) name)); \
	    free((void *) name); \
	    name = 0; \
	}

#define FREE_LEAK(name) \
	if (name) { \
	    free((void *) name); \
	    name = 0; \
	}

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
static void
cleanupInputMethod(XtermWidget xw)
{
    TInput *input = lookupTInput(xw, (Widget) xw);

    if (input && input->xim) {
	XCloseIM(input->xim);
	input->xim = 0;
	TRACE(("freed screen->xim\n"));
    }
}
#else
#define cleanupInputMethod(xw)	/* nothing */
#endif

static void
VTDestroy(Widget w GCC_UNUSED)
{
#ifdef NO_LEAKS
    XtermWidget xw = (XtermWidget) w;
    TScreen *screen = TScreenOf(xw);
    Cardinal n, k;

    StopBlinking(screen);

    if (screen->scrollWidget) {
	XtUninstallTranslations(screen->scrollWidget);
	XtDestroyWidget(screen->scrollWidget);
    }
#if OPT_FIFO_LINES
    while (screen->saved_fifo > 0) {
	deleteScrollback(screen);
    }
#endif
    while (screen->save_title != 0) {
	SaveTitle *last = screen->save_title;
	screen->save_title = last->next;
	free(last->iconName);
	free(last->windowName);
	free(last);
    }
#ifndef NO_ACTIVE_ICON
    TRACE_FREE_LEAK(xw->misc.active_icon_s);
#endif
#if OPT_ISO_COLORS
    TRACE_FREE_LEAK(screen->cmap_data);
    for (n = 0; n < MAXCOLORS; n++) {
	TRACE_FREE_LEAK(screen->Acolors[n].resource);
    }
#endif
#if OPT_COLOR_RES
    for (n = 0; n < NCOLORS; n++) {
	switch (n) {
#if OPT_TEK4014
	case TEK_BG:
	    /* FALLTHRU */
	case TEK_FG:
	    /* FALLTHRU */
	case TEK_CURSOR:
	    break;
#endif
	default:
	    TRACE_FREE_LEAK(screen->Tcolors[n].resource);
	    break;
	}
    }
#endif
    TRACE_FREE_LEAK(screen->save_ptr);
    TRACE_FREE_LEAK(screen->saveBuf_data);
    TRACE_FREE_LEAK(screen->saveBuf_index);
    for (n = 0; n < 2; ++n) {
	TRACE_FREE_LEAK(screen->editBuf_data[n]);
	TRACE_FREE_LEAK(screen->editBuf_index[n]);
    }
    TRACE_FREE_LEAK(screen->keyboard_dialect);
    TRACE_FREE_LEAK(screen->term_id);
#if OPT_WIDE_CHARS
#if OPT_LUIT_PROG
    TRACE_FREE_LEAK(xw->misc.locale_str);
    TRACE_FREE_LEAK(xw->misc.localefilter);
#endif
#endif
    TRACE_FREE_LEAK(xw->misc.T_geometry);
    TRACE_FREE_LEAK(xw->misc.geo_metry);
    TRACE_FREE_LEAK(xw->screen.term_id);
#if OPT_INPUT_METHOD
    cleanupInputMethod(xw);
    TRACE_FREE_LEAK(xw->misc.f_x);
    TRACE_FREE_LEAK(xw->misc.input_method);
    TRACE_FREE_LEAK(xw->misc.preedit_type);
#endif
    releaseCursorGCs(xw);
    releaseWindowGCs(xw, &(screen->fullVwin));
#ifndef NO_ACTIVE_ICON
    XFreeFont(screen->display, getIconicFont(screen)->fs);
    releaseWindowGCs(xw, &(screen->iconVwin));
#endif
    XtUninstallTranslations((Widget) xw);
#if OPT_TOOLBAR
    XtUninstallTranslations((Widget) XtParent(xw));
#endif
    XtUninstallTranslations((Widget) SHELL_OF(xw));

    if (screen->hidden_cursor)
	XFreeCursor(screen->display, screen->hidden_cursor);

    xtermCloseFonts(xw, screen->fnts);
#if OPT_WIDE_ATTRS
    xtermCloseFonts(xw, screen->ifnts);
#endif
    noleaks_cachedCgs(xw);

    TRACE_FREE_LEAK(screen->selection_targets_8bit);
#if OPT_SELECT_REGEX
    for (n = 0; n < NSELECTUNITS; ++n) {
	if (screen->selectMap[n] == Select_REGEX) {
	    TRACE_FREE_LEAK(screen->selectExpr[n]);
	}
    }
#endif

#if OPT_RENDERFONT
    for (n = 0; n < NMENUFONTS; ++n) {
	int e;
	for (e = 0; e < fMAX; ++e) {
	    xtermCloseXft(screen, getMyXftFont(xw, e, (int) n));
	}
    }
    if (screen->renderDraw)
	XftDrawDestroy(screen->renderDraw);
    {
	ListXftFonts *p;
	while ((p = screen->list_xft_fonts) != 0) {
	    screen->list_xft_fonts = p->next;
	    free(p);
	}
    }
#endif

    /* free things allocated via init_Sres or Init_Sres2 */
#ifndef NO_ACTIVE_ICON
    TRACE_FREE_LEAK(screen->icon_fontname);
#endif
#ifdef ALLOWLOGGING
    TRACE_FREE_LEAK(screen->logfile);
#endif
    TRACE_FREE_LEAK(screen->eight_bit_meta_s);
    TRACE_FREE_LEAK(screen->term_id);
    TRACE_FREE_LEAK(screen->charClass);
    TRACE_FREE_LEAK(screen->answer_back);
    TRACE_FREE_LEAK(screen->printer_state.printer_command);
    TRACE_FREE_LEAK(screen->keyboard_dialect);
    TRACE_FREE_LEAK(screen->disallowedColorOps);
    TRACE_FREE_LEAK(screen->disallowedFontOps);
    TRACE_FREE_LEAK(screen->disallowedMouseOps);
    TRACE_FREE_LEAK(screen->disallowedTcapOps);
    TRACE_FREE_LEAK(screen->disallowedWinOps);
    TRACE_FREE_LEAK(screen->default_string);
    TRACE_FREE_LEAK(screen->eightbit_select_types);

#if OPT_WIDE_CHARS
    TRACE_FREE_LEAK(screen->utf8_select_types);
#endif

#if 0
    for (n = fontMenu_font1; n <= fontMenu_lastBuiltin; n++) {
	TRACE_FREE_LEAK(screen->MenuFontName(n));
    }
#endif

    TRACE_FREE_LEAK(screen->initial_font);

#if OPT_LUIT_PROG
    TRACE_FREE_LEAK(xw->misc.locale_str);
    TRACE_FREE_LEAK(xw->misc.localefilter);
#endif

#if OPT_RENDERFONT
    TRACE_FREE_LEAK(xw->misc.default_xft.f_n);
#if OPT_WIDE_CHARS
    TRACE_FREE_LEAK(xw->misc.default_xft.f_w);
#endif
    TRACE_FREE_LEAK(xw->misc.render_font_s);
#endif

    TRACE_FREE_LEAK(xw->misc.default_font.f_n);
    TRACE_FREE_LEAK(xw->misc.default_font.f_b);

#if OPT_WIDE_CHARS
    TRACE_FREE_LEAK(xw->misc.default_font.f_w);
    TRACE_FREE_LEAK(xw->misc.default_font.f_wb);
#endif

    freeFontLists(&(xw->work.fonts.x11));
#if OPT_RENDERFONT
    freeFontLists(&(xw->work.fonts.xft));
#endif

    xtermFontName(NULL);
#if OPT_LOAD_VTFONTS || OPT_WIDE_CHARS
    TRACE_FREE_LEAK(screen->cacheVTFonts.default_font.f_n);
    TRACE_FREE_LEAK(screen->cacheVTFonts.default_font.f_b);
#if OPT_WIDE_CHARS
    TRACE_FREE_LEAK(screen->cacheVTFonts.default_font.f_w);
    TRACE_FREE_LEAK(screen->cacheVTFonts.default_font.f_wb);
#endif
    freeFontLists(&(screen->cacheVTFonts.fonts.x11));
    for (n = 0; n < NMENUFONTS; ++n) {
	for (k = 0; k < fMAX; ++k) {
	    if (screen->menu_font_names[n][k] !=
		screen->cacheVTFonts.menu_font_names[n][k]) {
		TRACE_FREE_LEAK(screen->menu_font_names[n][k]);
		TRACE_FREE_LEAK(screen->cacheVTFonts.menu_font_names[n][k]);
	    } else {
		TRACE_FREE_LEAK(screen->menu_font_names[n][k]);
	    }
	}
    }
#endif

#if OPT_SELECT_REGEX
    for (n = 0; n < NSELECTUNITS; ++n) {
	FREE_LEAK(screen->selectExpr[n]);
    }
#endif

    if (screen->selection_atoms)
	XtFree((void *) (screen->selection_atoms));

    XtFree((void *) (screen->selection_data));

    TRACE_FREE_LEAK(xtermClassRec.core_class.tm_table);
    TRACE_FREE_LEAK(xw->keyboard.extra_translations);
    TRACE_FREE_LEAK(xw->keyboard.shell_translations);
    TRACE_FREE_LEAK(xw->keyboard.xterm_translations);

    XtFree((void *) (xw->visInfo));

#if OPT_WIDE_CHARS
    FreeTypedBuffer(XChar2b);
    FreeTypedBuffer(char);
#endif
#if OPT_RENDERFONT
#if OPT_RENDERWIDE
    FreeTypedBuffer(XftCharSpec);
#else
    FreeTypedBuffer(XftChar8);
#endif
#endif

    TRACE_FREE_LEAK(myState.print_area);
    TRACE_FREE_LEAK(myState.string_area);
    memset(&myState, 0, sizeof(myState));

#endif /* defined(NO_LEAKS) */
}

#ifndef NO_ACTIVE_ICON
static void *
getProperty(Display *dpy,
	    Window w,
	    Atom req_type,
	    const char *prop_name)
{
    Atom property;
    Atom actual_return_type;
    int actual_format_return = 0;
    unsigned long nitems_return = 0;
    unsigned long bytes_after_return = 0;
    unsigned char *prop_return = 0;
    long long_length = 1024;
    size_t limit;
    char *result = 0;

    TRACE(("getProperty %s(%s)\n", prop_name,
	   req_type ? TraceAtomName(dpy, req_type) : "?"));
    property = XInternAtom(dpy, prop_name, False);

    if (!xtermGetWinProp(dpy,
			 w,
			 property,
			 0L,
			 long_length,
			 req_type,
			 &actual_return_type,
			 &actual_format_return,
			 &nitems_return,
			 &bytes_after_return,
			 &prop_return)) {
	TRACE((".. Cannot get %s property.\n", prop_name));
    } else if (prop_return != 0) {

	if (nitems_return != 0 &&
	    actual_format_return != 0 &&
	    actual_return_type == req_type) {
	    /*
	     * Null-terminate the result to make string handling easier.
	     * The format==8 corresponds to strings, and the number of items
	     * is the number of characters.
	     */
	    if (actual_format_return == 8) {
		limit = nitems_return;
	    } else {
		/* manpage is misleading - X really uses 'long', not 32-bits */
		limit = sizeof(long) * nitems_return;
	    }
	    if ((result = malloc(limit + 1)) != 0) {
		memcpy(result, prop_return, limit);
		result[limit] = '\0';
	    }
	    TRACE(("... result %s\n", result ? ("ok") : "null"));
	}
	XFree(prop_return);
    } else {
	TRACE((".. no property returned\n"));
    }
    return (void *) result;
}

/*
 * Active icons are supported by fvwm.  This feature is not supported by
 * metacity (gnome) or kwin (kde).  Both metacity and kwin support (in
 * incompatible ways, e.g., one uses the icon theme as a fallback for window
 * decorations but the other does not, etc, ...) an icon as part of the window
 * decoration (usually on the upper-left of the window).
 *
 * In either case, xterm's icon will only be shown in the window decorations if
 * xterm does not use the active icon feature.
 *
 * This function (tries to) determine the window manager's name, so that we can
 * provide a useful automatic default for active icons.  It is based on reading
 * wmctrl, which covers most of EWMH and ICCM.
 */
static char *
getWindowManagerName(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    Display *dpy = screen->display;
    Window *sup_window = NULL;
    char *result = 0;

    TRACE(("getWindowManagerName\n"));
#define getWinProp(type, name) \
    (Window *)getProperty(dpy, DefaultRootWindow(dpy), type, name)
    if ((sup_window = getWinProp(XA_WINDOW, "_NET_SUPPORTING_WM_CHECK")) == 0) {
	sup_window = getWinProp(XA_CARDINAL, "_WIN_SUPPORTING_WM_CHECK");
    }

    /*
     * If we found the supporting window, get the property containing the
     * window manager's name.  EWMH defines _NET_WM_NAME, while ICCM defines
     * WM_CLASS.  There is no standard for the names stored there;
     * conventionally it is mixed case.  In practice, the former is more often
     * set; the latter is not given (or is a lowercased version of the former).
     */
    if (sup_window != 0) {
#define getStringProp(type,name) \
	(char *)getProperty(dpy, *sup_window, type, name)
	if ((result = getStringProp(XA_UTF8_STRING(dpy), "_NET_WM_NAME")) == 0
	    && (result = getStringProp(XA_STRING, "_NET_WM_NAME")) == 0
	    && (result = getStringProp(XA_STRING, "WM_CLASS")) == 0) {
	    TRACE(("... window manager does not tell its name\n"));
	}
	free(sup_window);
    } else {
	TRACE(("... Cannot get window manager info properties\n"));
    }
    if (result == 0)
	result = x_strdup("unknown");
    TRACE(("... window manager name is %s\n", result));
    return result;
}
#endif

/*ARGSUSED*/
static void
VTRealize(Widget w,
	  XtValueMask * valuemask,
	  XSetWindowAttributes * values)
{
    XtermWidget xw = (XtermWidget) w;
    TScreen *screen = TScreenOf(xw);

    const VTFontNames *myfont;
    struct Xinerama_geometry pos;
    int pr;
    Atom pid_atom;
    int i;

    TRACE(("VTRealize {{\n"));

#if OPT_TOOLBAR
    /*
     * Layout for the toolbar confuses the Shell widget.  Remind it that we
     * would like to be iconified if the corresponding resource was set.
     */
    if (XtIsRealized(toplevel)) {
	Boolean iconic = 0;

	XtVaGetValues(toplevel,
		      XtNiconic, &iconic,
		      (XtPointer) 0);

	if (iconic) {
	    XIconifyWindow(XtDisplay(toplevel),
			   XtWindow(toplevel),
			   DefaultScreen(XtDisplay(toplevel)));
	}
    }
#endif

    TabReset(xw->tabs);

    if (screen->menu_font_number == fontMenu_default) {
	myfont = defaultVTFontNames(xw);
    } else {
	myfont = xtermFontName(screen->MenuFontName(screen->menu_font_number));
    }
    memset(screen->fnts, 0, sizeof(screen->fnts));

    if (!xtermLoadFont(xw,
		       myfont,
		       False,
		       screen->menu_font_number)) {
	if (XmuCompareISOLatin1(myfont->f_n, DEFFONT) != 0) {
	    char *use_font = x_strdup(DEFFONT);
	    xtermWarning("unable to open font \"%s\", trying \"%s\"....\n",
			 myfont->f_n, use_font);
	    (void) xtermLoadFont(xw,
				 xtermFontName(use_font),
				 False,
				 screen->menu_font_number);
	    screen->MenuFontName(screen->menu_font_number) = use_font;
	}
    }

    /* really screwed if we couldn't open default font */
    if (!getNormalFont(screen, fNorm)->fs) {
	xtermWarning("unable to locate a suitable font\n");
	Exit(1);
    }
#if OPT_WIDE_CHARS
    if (screen->utf8_mode) {
	TRACE(("check if this is a wide font, if not try again\n"));
	if (xtermLoadWideFonts(xw, False)) {
	    SetVTFont(xw, screen->menu_font_number, True, NULL);
	    /* we will not be able to switch to ISO-8859-1 */
	    if (!screen->mergedVTFonts) {
		screen->utf8_fonts = uAlways;
		update_font_utf8_fonts();
	    }
	}
    }
#endif

    /* making cursor */
    if (!screen->pointer_cursor) {
	screen->pointer_cursor =
	    make_colored_cursor(XC_xterm,
				T_COLOR(screen, MOUSE_FG),
				T_COLOR(screen, MOUSE_BG));
    } else {
	recolor_cursor(screen,
		       screen->pointer_cursor,
		       T_COLOR(screen, MOUSE_FG),
		       T_COLOR(screen, MOUSE_BG));
    }

    /* set defaults */
    pos.x = 1;
    pos.y = 1;
    pos.w = 80;
    pos.h = 24;

    TRACE(("parsing geo_metry %s\n", NonNull(xw->misc.geo_metry)));
    pr = XParseXineramaGeometry(screen->display, xw->misc.geo_metry, &pos);
    TRACE(("... position %d,%d size %dx%d\n", pos.y, pos.x, pos.h, pos.w));

    set_max_col(screen, (int) (pos.w - 1));	/* units in character cells */
    set_max_row(screen, (int) (pos.h - 1));	/* units in character cells */
    xtermUpdateFontInfo(xw, False);

    pos.w = screen->fullVwin.fullwidth;
    pos.h = screen->fullVwin.fullheight;

    TRACE(("... border widget %d parent %d shell %d\n",
	   BorderWidth(xw),
	   BorderWidth(XtParent(xw)),
	   BorderWidth(SHELL_OF(xw))));

    if ((pr & XValue) && (XNegative & pr)) {
	pos.x = (Position) (pos.x + (pos.scr_w
				     - (int) pos.w
				     - (BorderWidth(XtParent(xw)) * 2)));
    }
    if ((pr & YValue) && (YNegative & pr)) {
	pos.y = (Position) (pos.y + (pos.scr_h
				     - (int) pos.h
				     - (BorderWidth(XtParent(xw)) * 2)));
    }
    pos.x = (Position) (pos.x + pos.scr_x);
    pos.y = (Position) (pos.y + pos.scr_y);

    /* set up size hints for window manager; min 1 char by 1 char */
    getXtermSizeHints(xw);
    xtermSizeHints(xw, (xw->misc.scrollbar
			? (screen->scrollWidget->core.width
			   + BorderWidth(screen->scrollWidget))
			: 0));

    xw->hints.x = pos.x;
    xw->hints.y = pos.y;
#if OPT_MAXIMIZE
    /* assure single-increment resize for fullscreen */
    if (term->work.ewmh[0].mode) {
	xw->hints.width_inc = 1;
	xw->hints.height_inc = 1;
    }
#endif
    if ((XValue & pr) || (YValue & pr)) {
	xw->hints.flags |= USSize | USPosition;
	xw->hints.flags |= PWinGravity;
	switch (pr & (XNegative | YNegative)) {
	case 0:
	    xw->hints.win_gravity = NorthWestGravity;
	    break;
	case XNegative:
	    xw->hints.win_gravity = NorthEastGravity;
	    break;
	case YNegative:
	    xw->hints.win_gravity = SouthWestGravity;
	    break;
	default:
	    xw->hints.win_gravity = SouthEastGravity;
	    break;
	}
    } else {
	/* set a default size, but do *not* set position */
	xw->hints.flags |= PSize;
    }
    xw->hints.height = xw->hints.base_height
	+ xw->hints.height_inc * MaxRows(screen);
    xw->hints.width = xw->hints.base_width
	+ xw->hints.width_inc * MaxCols(screen);

    if ((WidthValue & pr) || (HeightValue & pr))
	xw->hints.flags |= USSize;
    else
	xw->hints.flags |= PSize;

    /*
     * Note that the size-hints are for the shell, while the resize-request
     * is for the vt100 widget.  They are not the same size.
     */
    (void) REQ_RESIZE((Widget) xw,
		      (Dimension) pos.w, (Dimension) pos.h,
		      &xw->core.width, &xw->core.height);

    /* XXX This is bogus.  We are parsing geometries too late.  This
     * is information that the shell widget ought to have before we get
     * realized, so that it can do the right thing.
     */
    if (xw->hints.flags & USPosition)
	XMoveWindow(XtDisplay(xw), VShellWindow(xw),
		    xw->hints.x, xw->hints.y);

    TRACE(("%s@%d -- ", __FILE__, __LINE__));
    TRACE_HINTS(&xw->hints);
    XSetWMNormalHints(XtDisplay(xw), VShellWindow(xw), &xw->hints);
    TRACE(("%s@%d -- ", __FILE__, __LINE__));
    TRACE_WM_HINTS(xw);

    if ((pid_atom = XInternAtom(XtDisplay(xw), "_NET_WM_PID", False)) != None) {
	/* XChangeProperty format 32 really is "long" */
	unsigned long pid_l = (unsigned long) getpid();
	TRACE(("Setting _NET_WM_PID property to %lu\n", pid_l));
	XChangeProperty(XtDisplay(xw), VShellWindow(xw),
			pid_atom, XA_CARDINAL, 32, PropModeReplace,
			(unsigned char *) &pid_l, 1);
    }

    XFlush(XtDisplay(xw));	/* get it out to window manager */

    /* use ForgetGravity instead of SouthWestGravity because translating
       the Expose events for ConfigureNotifys is too hard */
    values->bit_gravity = (GravityIsNorthWest(xw)
			   ? NorthWestGravity
			   : ForgetGravity);
    screen->fullVwin.window = XtWindow(xw) =
	XCreateWindow(XtDisplay(xw), XtWindow(XtParent(xw)),
		      xw->core.x, xw->core.y,
		      xw->core.width, xw->core.height, BorderWidth(xw),
		      (int) xw->core.depth,
		      InputOutput, CopyFromParent,
		      *valuemask | CWBitGravity, values);
#if OPT_DOUBLE_BUFFER
    screen->fullVwin.drawable = screen->fullVwin.window;

    {
	Window win = screen->fullVwin.window;
	Drawable d;
	int major, minor;
	if (!XdbeQueryExtension(XtDisplay(xw), &major, &minor)) {
	    fprintf(stderr, "XdbeQueryExtension returned zero!\n");
	    exit(3);
	}
	d = XdbeAllocateBackBufferName(XtDisplay(xw), win, XdbeCopied);
	if (d == None) {
	    fprintf(stderr, "Couldn't allocate a back buffer!\n");
	    exit(3);
	}
	screen->fullVwin.drawable = d;
	screen->needSwap = 1;
    }
#endif /* OPT_DOUBLE_BUFFER */
    screen->event_mask = values->event_mask;

#ifndef NO_ACTIVE_ICON
    /*
     * Normally, the font-number for icon fonts does not correspond with any of
     * the menu-selectable fonts.  If we cannot load the font given for the
     * iconFont resource, try with font1 aka "Unreadable".
     */
    screen->icon_fontnum = -1;
    if (getIconicFont(screen)->fs == 0) {
	getIconicFont(screen)->fs =
	    XLoadQueryFont(screen->display,
			   screen->MenuFontName(fontMenu_font1));
	TRACE(("%susing font1 '%s' as iconFont\n",
	       (getIconicFont(screen)->fs
		? ""
		: "NOT "),
	       screen->MenuFontName(fontMenu_font1)));
    }
#if OPT_RENDERFONT
    /*
     * If we still have no result from iconFont resource (perhaps because fonts
     * are missing) but are using Xft, try to use that instead.  We prefer
     * bitmap fonts in any case, since scaled fonts are usually less readable,
     * particularly at small sizes.
     */
    if (UsingRenderFont(xw)
	&& getIconicFont(screen)->fs == 0) {
	screen->icon_fontnum = fontMenu_default;
	getIconicFont(screen)->fs = getNormalFont(screen, fNorm)->fs;	/* need for next-if */
	TRACE(("using TrueType font as iconFont\n"));
    }
#endif
    if ((xw->work.active_icon == eiDefault) && getIconicFont(screen)->fs) {
	char *wm_name = getWindowManagerName(xw);
	if (x_strncasecmp(wm_name, "fvwm", 4) &&
	    x_strncasecmp(wm_name, "window maker", 12))
	    xw->work.active_icon = eiFalse;
	free(wm_name);
    }
    if (xw->work.active_icon && getIconicFont(screen)->fs) {
	int iconX = 0, iconY = 0;
	Widget shell = SHELL_OF(xw);
	VTwin *win = &(screen->iconVwin);
	int save_fontnum = screen->menu_font_number;

	TRACE(("Initializing active-icon %d\n", screen->icon_fontnum));
	screen->menu_font_number = screen->icon_fontnum;
	XtVaGetValues(shell,
		      XtNiconX, &iconX,
		      XtNiconY, &iconY,
		      (XtPointer) 0);
	xtermComputeFontInfo(xw, &(screen->iconVwin),
			     getIconicFont(screen)->fs, 0);
	screen->menu_font_number = save_fontnum;

	/* since only one client is permitted to select for Button
	 * events, we have to let the window manager get 'em...
	 */
	values->event_mask &= ~(ButtonPressMask | ButtonReleaseMask);
	values->border_pixel = xw->misc.icon_border_pixel;

	screen->iconVwin.window =
	    XCreateWindow(XtDisplay(xw),
			  RootWindowOfScreen(XtScreen(shell)),
			  iconX, iconY,
			  screen->iconVwin.fullwidth,
			  screen->iconVwin.fullheight,
			  xw->misc.icon_border_width,
			  (int) xw->core.depth,
			  InputOutput, CopyFromParent,
			  *valuemask | CWBitGravity | CWBorderPixel,
			  values);
#if OPT_DOUBLE_BUFFER
	screen->iconVwin.drawable = screen->iconVwin.window;
#endif
	XtVaSetValues(shell,
		      XtNiconWindow, screen->iconVwin.window,
		      (XtPointer) 0);
	XtRegisterDrawable(XtDisplay(xw), screen->iconVwin.window, w);

	setCgsFont(xw, win, gcNorm, getIconicFont(screen));
	setCgsFore(xw, win, gcNorm, T_COLOR(screen, TEXT_FG));
	setCgsBack(xw, win, gcNorm, T_COLOR(screen, TEXT_BG));

	copyCgs(xw, win, gcBold, gcNorm);

	setCgsFont(xw, win, gcNormReverse, getIconicFont(screen));
	setCgsFore(xw, win, gcNormReverse, T_COLOR(screen, TEXT_BG));
	setCgsBack(xw, win, gcNormReverse, T_COLOR(screen, TEXT_FG));

	copyCgs(xw, win, gcBoldReverse, gcNormReverse);

#if OPT_TOOLBAR
	/*
	 * Toolbar is initialized before we get here.  Enable the menu item
	 * and set it properly.
	 */
	SetItemSensitivity(vtMenuEntries[vtMenu_activeicon].widget, True);
	update_activeicon();
#endif
    } else {
	TRACE(("Disabled active-icon\n"));
	xw->work.active_icon = eiFalse;
    }
#endif /* NO_ACTIVE_ICON */

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
    VTInitI18N(xw);
#endif
#if OPT_NUM_LOCK
    VTInitModifiers(xw);
#if OPT_EXTRA_PASTE
    if (xw->keyboard.extra_translations) {
	XtOverrideTranslations((Widget) xw,
			       XtParseTranslationTable(xw->keyboard.extra_translations));
    }
#endif
#endif

    set_cursor_gcs(xw);

    /* Reset variables used by ANSI emulation. */

    resetCharsets(screen);

    XDefineCursor(screen->display, VShellWindow(xw), screen->pointer_cursor);

    set_cur_col(screen, 0);
    set_cur_row(screen, 0);
    set_max_col(screen, Width(screen) / screen->fullVwin.f_width - 1);
    set_max_row(screen, Height(screen) / screen->fullVwin.f_height - 1);
    reset_margins(screen);

    memset(screen->sc, 0, sizeof(screen->sc));

    /* Mark screen buffer as unallocated.  We wait until the run loop so
       that the child process does not fork and exec with all the dynamic
       memory it will never use.  If we were to do it here, the
       swap space for new process would be huge for huge savelines. */
#if OPT_TEK4014
    if (!tekWidget)		/* if not called after fork */
#endif
    {
	screen->visbuf = NULL;
	screen->saveBuf_index = NULL;
    }

    ResetWrap(screen);
    screen->scrolls = screen->incopy = 0;
    xtermSetCursorBox(screen);

    screen->savedlines = 0;

    for (i = 0; i < 2; ++i) {
	screen->whichBuf = !screen->whichBuf;
	CursorSave(xw);
    }

#ifndef NO_ACTIVE_ICON
    if (!xw->work.active_icon)
#endif
	xtermLoadIcon(xw);

    /*
     * Do this last, since it may change the layout via a resize.
     */
    if (xw->misc.scrollbar) {
	screen->fullVwin.sb_info.width = 0;
	ScrollBarOn(xw, False);
    }

    xtermSetWinSize(xw);
    TRACE(("}} VTRealize\n"));
}

#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD

/* limit this feature to recent XFree86 since X11R6.x core dumps */
#if defined(XtSpecificationRelease) && XtSpecificationRelease >= 6 && defined(X_HAVE_UTF8_STRING)
#define USE_XIM_INSTANTIATE_CB

static void
xim_instantiate_cb(Display *display,
		   XPointer client_data GCC_UNUSED,
		   XPointer call_data GCC_UNUSED)
{
    TRACE(("xim_instantiate_cb client=%p, call=%p\n", client_data, call_data));

    if (display == XtDisplay(term)) {
	VTInitI18N(term);
    }
}

static void
xim_destroy_cb(XIM im GCC_UNUSED,
	       XPointer client_data GCC_UNUSED,
	       XPointer call_data GCC_UNUSED)
{
    XtermWidget xw = term;
    TInput *input = lookupTInput(xw, (Widget) xw);

    TRACE(("xim_destroy_cb im=%lx, client=%p, call=%p\n",
	   (long) im, client_data, call_data));
    if (input)
	input->xic = NULL;
    XRegisterIMInstantiateCallback(XtDisplay(xw), NULL, NULL, NULL,
				   xim_instantiate_cb, NULL);
}
#endif /* X11R6+ */

static Boolean
xim_create_fs(XtermWidget xw)
{
    XFontStruct **fonts;
    char **font_name_list;
    char **missing_charset_list;
    char *def_string;
    int missing_charset_count;
    unsigned i, j;

    if (xw->work.xim_fs == 0) {
	xw->work.xim_fs = XCreateFontSet(XtDisplay(xw),
					 xw->misc.f_x,
					 &missing_charset_list,
					 &missing_charset_count,
					 &def_string);
	if (xw->work.xim_fs == NULL) {
	    xtermWarning("Preparation of font set "
			 "\"%s\" for XIM failed.\n", xw->misc.f_x);
	    xw->work.xim_fs = XCreateFontSet(XtDisplay(xw),
					     DEFXIMFONT,
					     &missing_charset_list,
					     &missing_charset_count,
					     &def_string);
	}
    }
    if (xw->work.xim_fs == NULL) {
	xtermWarning("Preparation of default font set "
		     "\"%s\" for XIM failed.\n", DEFXIMFONT);
	cleanupInputMethod(xw);
	xw->work.cannot_im = True;
    } else {
	(void) XExtentsOfFontSet(xw->work.xim_fs);
	j = (unsigned) XFontsOfFontSet(xw->work.xim_fs, &fonts, &font_name_list);
	for (i = 0, xw->work.xim_fs_ascent = 0; i < j; i++) {
	    if (xw->work.xim_fs_ascent < (*fonts)->ascent)
		xw->work.xim_fs_ascent = (*fonts)->ascent;
	}
    }
    return (Boolean) !(xw->work.cannot_im);
}

static void
xim_create_xic(XtermWidget xw, Widget theInput)
{
    Display *myDisplay = XtDisplay(theInput);
    Window myWindow = XtWindow(theInput);
    unsigned i, j;
    char *p = NULL, *s, *t, *ns, *end, buf[32];
    XIMStyles *xim_styles;
    XIMStyle input_style = 0;
    Bool found;
    static struct {
	const char *name;
	unsigned long code;
    } known_style[] = {
	{
	    "OverTheSpot", (XIMPreeditPosition | XIMStatusNothing)
	},
	{
	    "OffTheSpot", (XIMPreeditArea | XIMStatusArea)
	},
	{
	    "Root", (XIMPreeditNothing | XIMStatusNothing)
	},
    };
    TInput *input = lookupTInput(xw, theInput);

    if (xw->work.cannot_im) {
	return;
    }

    if (input == 0) {
	for (i = 0; i < NINPUTWIDGETS; ++i) {
	    if (xw->work.inputs[i].w == 0) {
		input = xw->work.inputs + i;
		input->w = theInput;
		break;
	    }
	}
    }

    if (input == 0) {
	xtermWarning("attempted to add too many input widgets\n");
	return;
    }

    TRACE(("xim_real_init\n"));

    if (IsEmpty(xw->misc.input_method)) {
	if ((p = XSetLocaleModifiers("")) != NULL && *p) {
	    input->xim = XOpenIM(myDisplay, NULL, NULL, NULL);
	}
    } else {
	s = xw->misc.input_method;
	i = 5 + (unsigned) strlen(s);

	t = (char *) MyStackAlloc(i, buf);
	if (t == NULL) {
	    SysError(ERROR_VINIT);
	} else {

	    for (ns = s; ns && *s;) {
		while (*s && isspace(CharOf(*s)))
		    s++;
		if (!*s)
		    break;
		if ((ns = end = strchr(s, ',')) == 0)
		    end = s + strlen(s);
		while ((end != s) && isspace(CharOf(end[-1])))
		    end--;

		if (end != s) {
		    strcpy(t, "@im=");
		    strncat(t, s, (size_t) (end - s));

		    if ((p = XSetLocaleModifiers(t)) != 0 && *p
			&& (input->xim = XOpenIM(myDisplay,
						 NULL,
						 NULL,
						 NULL)) != 0) {
			break;
		    }

		}
		s = ns + 1;
	    }
	    MyStackFree(t, buf);
	}
    }

    if (input->xim == NULL
	&& (p = XSetLocaleModifiers("@im=none")) != NULL
	&& *p) {
	input->xim = XOpenIM(myDisplay, NULL, NULL, NULL);
    }

    if (!input->xim) {
	xtermWarning("Failed to open input method\n");
	return;
    }
    TRACE(("VTInitI18N opened input method:%s\n", NonNull(p)));

    if (XGetIMValues(input->xim, XNQueryInputStyle, &xim_styles, (void *) 0)
	|| !xim_styles
	|| !xim_styles->count_styles) {
	xtermWarning("input method doesn't support any style\n");
	cleanupInputMethod(xw);
	xw->work.cannot_im = True;
	return;
    }

    found = False;
    for (s = xw->misc.preedit_type; s && !found;) {
	while (*s && isspace(CharOf(*s)))
	    s++;
	if (!*s)
	    break;
	if ((ns = end = strchr(s, ',')) != 0)
	    ns++;
	else
	    end = s + strlen(s);
	while ((end != s) && isspace(CharOf(end[-1])))
	    end--;

	if (end != s) {		/* just in case we have a spurious comma */
	    TRACE(("looking for style '%.*s'\n", (int) (end - s), s));
	    for (i = 0; i < XtNumber(known_style); i++) {
		if ((int) strlen(known_style[i].name) == (end - s)
		    && !strncmp(s, known_style[i].name, (size_t) (end - s))) {
		    input_style = known_style[i].code;
		    for (j = 0; j < xim_styles->count_styles; j++) {
			if (input_style == xim_styles->supported_styles[j]) {
			    found = True;
			    break;
			}
		    }
		    if (found)
			break;
		}
	    }
	}

	s = ns;
    }
    XFree(xim_styles);

    if (!found) {
	xtermWarning("input method doesn't support my preedit type (%s)\n",
		     xw->misc.preedit_type);
	cleanupInputMethod(xw);
	xw->work.cannot_im = True;
	return;
    }

    /*
     * Check for styles we do not yet support.
     */
    TRACE(("input_style %#lx\n", input_style));
    if (input_style == (XIMPreeditArea | XIMStatusArea)) {
	xtermWarning("This program doesn't support the 'OffTheSpot' preedit type\n");
	cleanupInputMethod(xw);
	xw->work.cannot_im = True;
	return;
    }

    /*
     * For XIMPreeditPosition (or OverTheSpot), XIM client has to
     * prepare a font.
     * The font has to be locale-dependent XFontSet, whereas
     * XTerm use Unicode font.  This leads a problem that the
     * same font cannot be used for XIM preedit.
     */
    if (input_style != (XIMPreeditNothing | XIMStatusNothing)) {
	XVaNestedList p_list;
	XPoint spot =
	{0, 0};

	if (xim_create_fs(xw)) {
	    p_list = XVaCreateNestedList(0,
					 XNSpotLocation, &spot,
					 XNFontSet, xw->work.xim_fs,
					 (void *) 0);
	    input->xic = XCreateIC(input->xim,
				   XNInputStyle, input_style,
				   XNClientWindow, myWindow,
				   XNFocusWindow, myWindow,
				   XNPreeditAttributes, p_list,
				   (void *) 0);
	}
    } else {
	input->xic = XCreateIC(input->xim, XNInputStyle, input_style,
			       XNClientWindow, myWindow,
			       XNFocusWindow, myWindow,
			       (void *) 0);
    }

    if (!input->xic) {
	xtermWarning("Failed to create input context\n");
	cleanupInputMethod(xw);
    }
#if defined(USE_XIM_INSTANTIATE_CB)
    else {
	XIMCallback destroy_cb;

	destroy_cb.callback = xim_destroy_cb;
	destroy_cb.client_data = NULL;
	if (XSetIMValues(input->xim,
			 XNDestroyCallback,
			 &destroy_cb,
			 (void *) 0)) {
	    xtermWarning("Could not set destroy callback to IM\n");
	}
    }
#endif

    return;
}

static void
xim_real_init(XtermWidget xw)
{
    xim_create_xic(xw, (Widget) xw);
}

static void
VTInitI18N(XtermWidget xw)
{
    if (xw->misc.open_im) {
	xim_real_init(xw);

#if defined(USE_XIM_INSTANTIATE_CB)
	if (lookupTInput(xw, (Widget) xw) == NULL
	    && !xw->work.cannot_im
	    && xw->misc.retry_im-- > 0) {
	    sleep(3);
	    XRegisterIMInstantiateCallback(XtDisplay(xw), NULL, NULL, NULL,
					   xim_instantiate_cb, NULL);
	}
#endif
    }
}

TInput *
lookupTInput(XtermWidget xw, Widget w)
{
    TInput *result = 0;
    unsigned n;

    for (n = 0; n < NINPUTWIDGETS; ++n) {
	if (xw->work.inputs[n].w == w) {
	    result = xw->work.inputs + n;
	    break;
	}
    }

    return result;
}
#endif /* OPT_I18N_SUPPORT && OPT_INPUT_METHOD */

static void
set_cursor_outline_gc(XtermWidget xw,
		      Bool filled,
		      Pixel fg,
		      Pixel bg,
		      Pixel cc)
{
    TScreen *screen = TScreenOf(xw);
    VTwin *win = WhichVWin(screen);
    CgsEnum cgsId = gcVTcursOutline;

    if (cc == bg)
	cc = fg;

    if (filled) {
	setCgsFore(xw, win, cgsId, bg);
	setCgsBack(xw, win, cgsId, cc);
    } else {
	setCgsFore(xw, win, cgsId, cc);
	setCgsBack(xw, win, cgsId, bg);
    }
}

static Boolean
VTSetValues(Widget cur,
	    Widget request GCC_UNUSED,
	    Widget wnew,
	    ArgList args GCC_UNUSED,
	    Cardinal *num_args GCC_UNUSED)
{
    XtermWidget curvt = (XtermWidget) cur;
    XtermWidget newvt = (XtermWidget) wnew;
    Boolean refresh_needed = False;
    Boolean fonts_redone = False;

    if ((T_COLOR(TScreenOf(curvt), TEXT_BG) !=
	 T_COLOR(TScreenOf(newvt), TEXT_BG)) ||
	(T_COLOR(TScreenOf(curvt), TEXT_FG) !=
	 T_COLOR(TScreenOf(newvt), TEXT_FG)) ||
	(TScreenOf(curvt)->MenuFontName(TScreenOf(curvt)->menu_font_number) !=
	 TScreenOf(newvt)->MenuFontName(TScreenOf(newvt)->menu_font_number)) ||
	strcmp(DefaultFontN(curvt), DefaultFontN(newvt))) {
	if (strcmp(DefaultFontN(curvt), DefaultFontN(newvt))) {
	    TScreenOf(newvt)->MenuFontName(fontMenu_default) = DefaultFontN(newvt);
	}
	if (xtermLoadFont(newvt,
			  xtermFontName(TScreenOf(newvt)->MenuFontName(TScreenOf(curvt)->menu_font_number)),
			  True, TScreenOf(newvt)->menu_font_number)) {
	    /* resizing does the redisplay, so don't ask for it here */
	    refresh_needed = True;
	    fonts_redone = True;
	} else if (strcmp(DefaultFontN(curvt), DefaultFontN(newvt))) {
	    TScreenOf(newvt)->MenuFontName(fontMenu_default) = DefaultFontN(curvt);
	}
    }
    if (!fonts_redone
	&& (T_COLOR(TScreenOf(curvt), TEXT_CURSOR) !=
	    T_COLOR(TScreenOf(newvt), TEXT_CURSOR))) {
	if (set_cursor_gcs(newvt))
	    refresh_needed = True;
    }
    if (curvt->misc.re_verse != newvt->misc.re_verse) {
	newvt->flags ^= REVERSE_VIDEO;
	ReverseVideo(newvt);
	/* ReverseVideo toggles */
	newvt->misc.re_verse = (Boolean) (!newvt->misc.re_verse);
	refresh_needed = True;
    }
    if ((T_COLOR(TScreenOf(curvt), MOUSE_FG) !=
	 T_COLOR(TScreenOf(newvt), MOUSE_FG)) ||
	(T_COLOR(TScreenOf(curvt), MOUSE_BG) !=
	 T_COLOR(TScreenOf(newvt), MOUSE_BG))) {
	recolor_cursor(TScreenOf(newvt),
		       TScreenOf(newvt)->pointer_cursor,
		       T_COLOR(TScreenOf(newvt), MOUSE_FG),
		       T_COLOR(TScreenOf(newvt), MOUSE_BG));
	refresh_needed = True;
    }
    if (curvt->misc.scrollbar != newvt->misc.scrollbar) {
	ToggleScrollBar(newvt);
    }

    return refresh_needed;
}

/*
 * Given a font-slot and information about selection/reverse, find the
 * corresponding cached-GC slot.
 */
#if OPT_WIDE_ATTRS
static int
reverseCgs(XtermWidget xw, unsigned attr_flags, Bool hilite, int font)
{
    TScreen *screen = TScreenOf(xw);
    CgsEnum result = gcMAX;

    if (ReverseOrHilite(screen, attr_flags, hilite)) {
	switch (font) {
	case fNorm:
	    result = gcNormReverse;
	    break;
	case fBold:
	    result = gcBoldReverse;
	    break;
#if OPT_WIDE_ATTRS || OPT_RENDERWIDE
	case fItal:
	    result = gcNormReverse;	/* FIXME */
	    break;
#endif
#if OPT_WIDE_CHARS
	case fWide:
	    result = gcWideReverse;
	    break;
	case fWBold:
	    result = gcWBoldReverse;
	    break;
	case fWItal:
	    result = gcWideReverse;	/* FIXME */
	    break;
#endif
	}
    } else {
	switch (font) {
	case fNorm:
	    result = gcNorm;
	    break;
	case fBold:
	    result = gcBold;
	    break;
#if OPT_WIDE_ATTRS || OPT_RENDERWIDE
	case fItal:
	    result = gcNorm;	/* FIXME */
	    break;
#endif
#if OPT_WIDE_CHARS
	case fWide:
	    result = gcWide;
	    break;
	case fWBold:
	    result = gcWBold;
	    break;
	case fWItal:
	    result = gcWide;	/* FIXME */
	    break;
#endif
	}
    }
    return result;
}
#endif

#define setGC(code) set_at = __LINE__, currentCgs = code

#define OutsideSelection(screen,srow,scol)  \
	 ((srow) > (screen)->endH.row || \
	  ((srow) == (screen)->endH.row && \
	   (scol) >= (screen)->endH.col) || \
	  (srow) < (screen)->startH.row || \
	  ((srow) == (screen)->startH.row && \
	   (scol) < (screen)->startH.col))

/*
 * Shows cursor at new cursor position in screen.
 */
void
ShowCursor(void)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    IChar base;
    unsigned flags;
    CellColor fg_bg = initCColor;
    GC currentGC;
    GC outlineGC;
    CgsEnum currentCgs = gcMAX;
    VTwin *currentWin = WhichVWin(screen);
    int set_at;
    Bool in_selection;
    Bool reversed;
    Bool filled;
    Pixel fg_pix;
    Pixel bg_pix;
    Pixel tmp;
#if OPT_HIGHLIGHT_COLOR
    Pixel selbg_pix = T_COLOR(screen, HIGHLIGHT_BG);
    Pixel selfg_pix = T_COLOR(screen, HIGHLIGHT_FG);
    Boolean use_selbg;
    Boolean use_selfg;
#endif
#if OPT_WIDE_CHARS
    int my_col = 0;
#endif
    int cursor_col;
    CLineData *ld = 0;

    if (screen->cursor_state == BLINKED_OFF)
	return;

    if (screen->eventMode != NORMAL)
	return;

    if (INX2ROW(screen, screen->cur_row) > screen->max_row)
	return;

    screen->cursorp.row = screen->cur_row;
    cursor_col = screen->cursorp.col = screen->cur_col;
    screen->cursor_moved = False;

#ifndef NO_ACTIVE_ICON
    if (IsIcon(screen)) {
	screen->cursor_state = ON;
	return;
    }
#endif /* NO_ACTIVE_ICON */

    ld = getLineData(screen, screen->cur_row);

    base = ld->charData[cursor_col];
    flags = ld->attribs[cursor_col];

    if_OPT_WIDE_CHARS(screen, {
	if (base == HIDDEN_CHAR && cursor_col > 0) {
	    /* if cursor points to non-initial part of wide character,
	     * back it up
	     */
	    --cursor_col;
	    base = ld->charData[cursor_col];
	}
	my_col = cursor_col;
	if (base == 0)
	    base = ' ';
	if (isWide((int) base))
	    my_col += 1;
    });

    if (base == 0) {
	base = ' ';
    }
#if OPT_ISO_COLORS
#ifdef EXP_BOGUS_FG
    /*
     * If the cursor happens to be on blanks, and we have not set both
     * foreground and background color, do not treat it as a colored cell.
     */
    if (base == ' ') {
	if ((flags & (FG_COLOR | BG_COLOR)) == BG_COLOR) {
	    TRACE(("ShowCursor - do not treat as a colored cell\n"));
	    flags &= ~(FG_COLOR | BG_COLOR);
	} else if ((flags & (FG_COLOR | BG_COLOR)) == FG_COLOR) {
	    TRACE(("ShowCursor - should we treat as a colored cell?\n"));
	    if (!(xw->flags & FG_COLOR))
		if (CheckBogusForeground(screen, "ShowCursor"))
		    flags &= ~(FG_COLOR | BG_COLOR);
	}
    }
#else /* !EXP_BOGUS_FG */
    /*
     * If the cursor happens to be on blanks, and the foreground color is set
     * but not the background, do not treat it as a colored cell.
     */
    if ((flags & TERM_COLOR_FLAGS(xw)) == FG_COLOR
	&& base == ' ') {
	flags &= ~TERM_COLOR_FLAGS(xw);
    }
#endif
#endif

    /*
     * Compare the current cell to the last set of colors used for the
     * cursor and update the GC's if needed.
     */
    if_OPT_ISO_COLORS(screen, {
	fg_bg = ld->color[cursor_col];
    });

    fg_pix = getXtermFG(xw, flags, (int) extract_fg(xw, fg_bg, flags));
    bg_pix = getXtermBG(xw, flags, (int) extract_bg(xw, fg_bg, flags));

    /*
     * If we happen to have the same foreground/background colors, choose
     * a workable foreground color from which we can obtain a visible cursor.
     */
    if (fg_pix == bg_pix) {
	long bg_diff = (long) (bg_pix - T_COLOR(TScreenOf(xw), TEXT_BG));
	long fg_diff = (long) (bg_pix - T_COLOR(TScreenOf(xw), TEXT_FG));
	if (bg_diff < 0)
	    bg_diff = -bg_diff;
	if (fg_diff < 0)
	    fg_diff = -fg_diff;
	if (bg_diff < fg_diff) {
	    fg_pix = T_COLOR(TScreenOf(xw), TEXT_FG);
	} else {
	    fg_pix = T_COLOR(TScreenOf(xw), TEXT_BG);
	}
    }

    if (OutsideSelection(screen, screen->cur_row, screen->cur_col))
	in_selection = False;
    else
	in_selection = True;

    reversed = ReverseOrHilite(screen, flags, in_selection);

    /* This is like updatedXtermGC(), except that we have to worry about
     * whether the window has focus, since in that case we want just an
     * outline for the cursor.
     */
    filled = (screen->select || screen->always_highlight) && isCursorBlock(screen);
#if OPT_HIGHLIGHT_COLOR
    use_selbg = isNotForeground(xw, fg_pix, bg_pix, selbg_pix);
    use_selfg = isNotBackground(xw, fg_pix, bg_pix, selfg_pix);
#endif
    if (filled) {
	if (reversed) {		/* text is reverse video */
	    if (getCgsGC(xw, currentWin, gcVTcursNormal)) {
		setGC(gcVTcursNormal);
	    } else {
		if (flags & BOLDATTR(screen)) {
		    setGC(gcBold);
		} else {
		    setGC(gcNorm);
		}
	    }
	    EXCHANGE(fg_pix, bg_pix, tmp);
#if OPT_HIGHLIGHT_COLOR
	    if (screen->hilite_reverse) {
		if (use_selbg && !use_selfg)
		    fg_pix = bg_pix;
		if (use_selfg && !use_selbg)
		    bg_pix = fg_pix;
		if (use_selbg)
		    bg_pix = selbg_pix;
		if (use_selfg)
		    fg_pix = selfg_pix;
	    }
#endif
	} else {		/* normal video */
	    if (getCgsGC(xw, currentWin, gcVTcursReverse)) {
		setGC(gcVTcursReverse);
	    } else {
		if (flags & BOLDATTR(screen)) {
		    setGC(gcBoldReverse);
		} else {
		    setGC(gcNormReverse);
		}
	    }
	}

#define CUR_XX T_COLOR(screen, TEXT_CURSOR)
#define CGS_FG getCgsFore(xw, currentWin, getCgsGC(xw, currentWin, currentCgs))
#define CGS_BG getCgsBack(xw, currentWin, getCgsGC(xw, currentWin, currentCgs))

#define FIX_311 (CUR_XX == (reversed ? xw->dft_background : xw->dft_foreground))
#define FIX_328 (CUR_XX == bg_pix)
#define FIX_330 (FIX_328 && reversed && in_selection)

	if (FIX_330 || FIX_311) {
	    setCgsBack(xw, currentWin, currentCgs, fg_pix);
	}
	setCgsFore(xw, currentWin, currentCgs, bg_pix);
    } else {			/* not selected */
	if (reversed) {		/* text is reverse video */
	    EXCHANGE(fg_pix, bg_pix, tmp);
	    setGC(gcNormReverse);
	} else {		/* normal video */
	    setGC(gcNorm);
	}
#if OPT_HIGHLIGHT_COLOR
	if (screen->hilite_reverse) {
	    if (in_selection && !reversed) {
		/* EMPTY */
		/* really INVERSE ... */
		;
	    } else if (in_selection || reversed) {
		if (use_selbg) {
		    if (use_selfg) {
			bg_pix = fg_pix;
		    } else {
			fg_pix = bg_pix;
		    }
		}
		if (use_selbg) {
		    bg_pix = selbg_pix;
		}
		if (use_selfg) {
		    fg_pix = selfg_pix;
		}
	    }
	} else {
	    if (in_selection) {
		if (use_selbg) {
		    bg_pix = selbg_pix;
		}
		if (use_selfg) {
		    fg_pix = selfg_pix;
		}
	    }
	}
#endif
	setCgsFore(xw, currentWin, currentCgs, fg_pix);
	setCgsBack(xw, currentWin, currentCgs, bg_pix);
    }

    if (screen->cursor_busy == 0
	&& (screen->cursor_state != ON || screen->cursor_GC != set_at)) {
	int x, y;

	screen->cursor_GC = set_at;
	TRACE(("ShowCursor calling drawXtermText cur(%d,%d) %s-%s, set_at %d\n",
	       screen->cur_row, screen->cur_col,
	       (filled ? "filled" : "outline"),
	       (isCursorBlock(screen) ? "box" :
		isCursorUnderline(screen) ? "underline" : "bar"),
	       set_at));

	currentGC = getCgsGC(xw, currentWin, currentCgs);
	x = LineCursorX(screen, ld, cursor_col);
	y = CursorY(screen, screen->cur_row);

	if (!isCursorBlock(screen)) {
	    /*
	     * Overriding the combination of filled, reversed, in_selection is
	     * too complicated since the underline or bar and the text-cell use
	     * different rules.  Just redraw the text-cell, and draw the
	     * underline or bar on top of it.
	     */
	    HideCursor();

	    /*
	     * Our current-GC is likely to have been modified in HideCursor().
	     * Set up a new request.
	     */
	    if (filled) {
		if (FIX_330 || FIX_311) {
		    setCgsBack(xw, currentWin, currentCgs, fg_pix);
		}
		setCgsFore(xw, currentWin, currentCgs, bg_pix);
	    } else {
		setCgsFore(xw, currentWin, currentCgs, fg_pix);
		setCgsBack(xw, currentWin, currentCgs, bg_pix);
	    }
	}

	/*
	 * Update the outline-gc, to keep the cursor color distinct from the
	 * background color.
	 */
	set_cursor_outline_gc(xw,
			      filled,
			      fg_pix,
			      bg_pix,
			      T_COLOR(screen, TEXT_CURSOR));

	outlineGC = getCgsGC(xw, currentWin, gcVTcursOutline);
	if (outlineGC == 0)
	    outlineGC = currentGC;

	if (isCursorUnderline(screen)) {

	    /*
	     * Finally, draw the underline.
	     */
	    screen->box->x = (short) x;
	    screen->box->y = (short) (y + FontHeight(screen) - 2);
	    XDrawLines(screen->display, VDrawable(screen), outlineGC,
		       screen->box, NBOX, CoordModePrevious);
	} else if (isCursorBar(screen)) {

	    /*
	     * Or draw the bar.
	     */
	    screen->box->x = (short) x;
	    screen->box->y = (short) y;
	    XDrawLines(screen->display, VWindow(screen), outlineGC,
		       screen->box, NBOX, CoordModePrevious);
	} else {
#if OPT_WIDE_ATTRS
	    int italics_on = ((ld->attribs[cursor_col] & ATR_ITALIC) != 0);
	    int italics_off = ((xw->flags & ATR_ITALIC) != 0);
	    int fix_italics = (italics_on != italics_off);
	    int which_font = ((xw->flags & BOLD) ? fBold : fNorm);

	    if_OPT_WIDE_CHARS(screen, {
		if (isWide((int) base)) {
		    which_font = ((xw->flags & BOLD) ? fWBold : fWide);
		}
	    });

	    if (fix_italics && UseItalicFont(screen)) {
		xtermLoadItalics(xw);
		if (italics_on) {
		    setCgsFont(xw, currentWin, currentCgs,
			       getItalicFont(screen, which_font));
		} else {
		    setCgsFont(xw, currentWin, currentCgs,
			       getNormalFont(screen, which_font));
		}
	    }
	    currentGC = getCgsGC(xw, currentWin, currentCgs);
#endif /* OPT_WIDE_ATTRS */

	    drawXtermText(xw,
			  flags & DRAWX_MASK,
			  0,
			  currentGC, x, y,
			  LineCharSet(screen, ld),
			  &base, 1, 0);

#if OPT_WIDE_CHARS
	    if_OPT_WIDE_CHARS(screen, {
		size_t off;
		for_each_combData(off, ld) {
		    if (!(ld->combData[off][my_col]))
			break;
		    drawXtermText(xw,
				  (flags & DRAWX_MASK),
				  NOBACKGROUND,
				  currentGC, x, y,
				  LineCharSet(screen, ld),
				  ld->combData[off] + my_col,
				  1, isWide((int) base));
		}
	    });
#endif

	    if (!filled) {
		screen->box->x = (short) x;
		screen->box->y = (short) y;
		XDrawLines(screen->display, VDrawable(screen), outlineGC,
			   screen->box, NBOX, CoordModePrevious);
	    }
#if OPT_WIDE_ATTRS
	    if (fix_italics && UseItalicFont(screen)) {
		if (italics_on) {
		    setCgsFont(xw, currentWin, currentCgs,
			       getNormalFont(screen, which_font));
		} else {
		    setCgsFont(xw, currentWin, currentCgs,
			       getItalicFont(screen, which_font));
		}
	    }
#endif
	}
    }
    screen->cursor_state = ON;

    return;
}

/*
 * hide cursor at previous cursor position in screen.
 */
void
HideCursor(void)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    GC currentGC;
    int x, y;
    IChar base;
    unsigned flags;
    CellColor fg_bg;
    Bool in_selection;
#if OPT_WIDE_CHARS
    int my_col = 0;
#endif
    int cursor_col;
    CLineData *ld = 0;
#if OPT_WIDE_ATTRS
    int which_Cgs = gcMAX;
    unsigned attr_flags;
    int which_font = fNorm;
#endif

    if (screen->cursor_state == OFF)
	return;
    if (INX2ROW(screen, screen->cursorp.row) > screen->max_row)
	return;

    cursor_col = screen->cursorp.col;

#ifndef NO_ACTIVE_ICON
    if (IsIcon(screen)) {
	screen->cursor_state = OFF;
	return;
    }
#endif /* NO_ACTIVE_ICON */

    ld = getLineData(screen, screen->cursorp.row);

    base = ld->charData[cursor_col];
    flags = ld->attribs[cursor_col];

    if_OPT_WIDE_CHARS(screen, {
	if (base == HIDDEN_CHAR && cursor_col > 0) {
	    /* if cursor points to non-initial part of wide character,
	     * back it up
	     */
	    --cursor_col;
	    base = ld->charData[cursor_col];
	}
	my_col = cursor_col;
	if (base == 0)
	    base = ' ';
	if (isWide((int) base))
	    my_col += 1;
    });

    if (base == 0) {
	base = ' ';
    }
#ifdef EXP_BOGUS_FG
    /*
     * If the cursor happens to be on blanks, and we have not set both
     * foreground and background color, do not treat it as a colored cell.
     */
#if OPT_ISO_COLORS
    if (base == ' ') {
	if ((flags & (FG_COLOR | BG_COLOR)) == BG_COLOR) {
	    TRACE(("HideCursor - do not treat as a colored cell\n"));
	    flags &= ~(FG_COLOR | BG_COLOR);
	} else if ((flags & (FG_COLOR | BG_COLOR)) == FG_COLOR) {
	    TRACE(("HideCursor - should we treat as a colored cell?\n"));
	    if (!(xw->flags & FG_COLOR))
		if (CheckBogusForeground(screen, "HideCursor"))
		    flags &= ~(FG_COLOR | BG_COLOR);
	}
    }
#endif
#endif
#if OPT_ISO_COLORS
    fg_bg = initCColor;
#endif

    /*
     * Compare the current cell to the last set of colors used for the
     * cursor and update the GC's if needed.
     */
    if_OPT_ISO_COLORS(screen, {
	fg_bg = ld->color[cursor_col];
    });

    if (OutsideSelection(screen, screen->cursorp.row, screen->cursorp.col))
	in_selection = False;
    else
	in_selection = True;

#if OPT_WIDE_ATTRS
    attr_flags = ld->attribs[cursor_col];
    if ((attr_flags & ATR_ITALIC) ^ (xw->flags & ATR_ITALIC)) {
	which_font = ((attr_flags & BOLD) ? fBold : fNorm);

	if_OPT_WIDE_CHARS(screen, {
	    if (isWide((int) base)) {
		which_font = ((attr_flags & BOLD) ? fWBold : fWide);
	    }
	});

	which_Cgs = reverseCgs(xw, attr_flags, in_selection, which_font);
	if (which_Cgs != gcMAX) {
	    setCgsFont(xw, WhichVWin(screen),
		       (CgsEnum) which_Cgs,
		       (((attr_flags & ATR_ITALIC) && UseItalicFont(screen))
			? getItalicFont(screen, which_font)
			: getNormalFont(screen, which_font)));
	}
    }
#endif

    currentGC = updatedXtermGC(xw, flags, fg_bg, in_selection);

    TRACE(("HideCursor calling drawXtermText cur(%d,%d)\n",
	   screen->cursorp.row, screen->cursorp.col));

    x = LineCursorX(screen, ld, cursor_col);
    y = CursorY(screen, screen->cursorp.row);

    drawXtermText(xw,
		  flags & DRAWX_MASK,
		  0,
		  currentGC, x, y,
		  LineCharSet(screen, ld),
		  &base, 1, 0);

#if OPT_WIDE_CHARS
    if_OPT_WIDE_CHARS(screen, {
	size_t off;
	for_each_combData(off, ld) {
	    if (!(ld->combData[off][my_col]))
		break;
	    drawXtermText(xw,
			  (flags & DRAWX_MASK),
			  NOBACKGROUND,
			  currentGC, x, y,
			  LineCharSet(screen, ld),
			  ld->combData[off] + my_col,
			  1, isWide((int) base));
	}
    });
#endif
    screen->cursor_state = OFF;

#if OPT_WIDE_ATTRS
    if (which_Cgs != gcMAX) {
	setCgsFont(xw, WhichVWin(screen),
		   (CgsEnum) which_Cgs,
		   (((xw->flags & ATR_ITALIC) && UseItalicFont(screen))
		    ? getItalicFont(screen, which_font)
		    : getNormalFont(screen, which_font)));
    }
#endif
    resetXtermGC(xw, flags, in_selection);

    refresh_displayed_graphics(xw,
			       screen->cursorp.col,
			       screen->cursorp.row,
			       1, 1);

    return;
}

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
static void
StartBlinking(TScreen *screen)
{
    if (screen->blink_timer == 0) {
	unsigned long interval = (unsigned long) ((screen->cursor_state == ON)
						  ? screen->blink_on
						  : screen->blink_off);
	if (interval == 0)	/* wow! */
	    interval = 1;	/* let's humor him anyway */
	screen->blink_timer = XtAppAddTimeOut(app_con,
					      interval,
					      HandleBlinking,
					      screen);
    }
}

static void
StopBlinking(TScreen *screen)
{
    if (screen->blink_timer) {
	XtRemoveTimeOut(screen->blink_timer);
	screen->blink_timer = 0;
	reallyStopBlinking(screen);
    } else {
	screen->blink_timer = 0;
    }
}

#if OPT_BLINK_TEXT
Bool
LineHasBlinking(TScreen *screen, CLineData *ld)
{
    Bool result = False;
    if (ld != 0) {
	int col;

	for (col = 0; col < MaxCols(screen); ++col) {
	    if (ld->attribs[col] & BLINK) {
		result = True;
		break;
	    }
	}
    }
    return result;
}
#endif

/*
 * Blink the cursor by alternately showing/hiding cursor.  We leave the timer
 * running all the time (even though that's a little inefficient) to make the
 * logic simple.
 */
static void
HandleBlinking(XtPointer closure, XtIntervalId * id GCC_UNUSED)
{
    TScreen *screen = (TScreen *) closure;
    Bool resume = False;

    screen->blink_timer = 0;
    screen->blink_state = !screen->blink_state;

#if OPT_BLINK_CURS
    if (DoStartBlinking(screen)) {
	if (screen->cursor_state == ON) {
	    if (screen->select || screen->always_highlight) {
		HideCursor();
		if (screen->cursor_state == OFF)
		    screen->cursor_state = BLINKED_OFF;
	    }
	} else if (screen->cursor_state == BLINKED_OFF) {
	    screen->cursor_state = OFF;
	    ShowCursor();
	    if (screen->cursor_state == OFF)
		screen->cursor_state = BLINKED_OFF;
	}
	resume = True;
    }
#endif

#if OPT_BLINK_TEXT
    /*
     * Inspect the lines on the current screen to see if any have the BLINK flag
     * associated with them.  Prune off any that have had the corresponding
     * cells reset.  If any are left, repaint those lines with ScrnRefresh().
     */
    if (!(screen->blink_as_bold)) {
	int row;
	int first_row = screen->max_row;
	int last_row = -1;

	for (row = screen->max_row; row >= 0; row--) {
	    LineData *ld = getLineData(screen, ROW2INX(screen, row));

	    if (ld != 0 && LineTstBlinked(ld)) {
		if (LineHasBlinking(screen, ld)) {
		    resume = True;
		    if (row > last_row)
			last_row = row;
		    if (row < first_row)
			first_row = row;
		} else {
		    LineClrBlinked(ld);
		}
	    }
	}
	/*
	 * FIXME: this could be a little more efficient, e.g,. by limiting the
	 * columns which are updated.
	 */
	if (first_row <= last_row) {
	    ScrnRefresh(term,
			first_row,
			0,
			last_row + 1 - first_row,
			MaxCols(screen),
			True);
	}
    }
#endif

    /*
     * If either the cursor or text is blinking, restart the timer.
     */
    if (resume)
	StartBlinking(screen);
}
#endif /* OPT_BLINK_CURS || OPT_BLINK_TEXT */

void
RestartBlinking(TScreen *screen GCC_UNUSED)
{
#if OPT_BLINK_CURS || OPT_BLINK_TEXT
    if (screen->blink_timer == 0) {
	Bool resume = False;

#if OPT_BLINK_CURS
	if (DoStartBlinking(screen)) {
	    resume = True;
	}
#endif
#if OPT_BLINK_TEXT
	if (!resume) {
	    int row;

	    for (row = screen->max_row; row >= 0; row--) {
		CLineData *ld = getLineData(screen, ROW2INX(screen, row));

		if (ld != 0 && LineTstBlinked(ld)) {
		    if (LineHasBlinking(screen, ld)) {
			resume = True;
			break;
		    }
		}
	    }
	}
#endif
	if (resume)
	    StartBlinking(screen);
    }
#endif
}

/*
 * Implement soft or hard (full) reset of the VTxxx emulation.  There are a
 * couple of differences from real DEC VTxxx terminals (to avoid breaking
 * applications which have come to rely on xterm doing this):
 *
 *	+ autowrap mode should be reset (instead it's reset to the resource
 *	  default).
 *	+ the popup menu offers a choice of resetting the savedLines, or not.
 *	  (but the control sequence does this anyway).
 */
static void
ReallyReset(XtermWidget xw, Bool full, Bool saved)
{
    TScreen *screen = TScreenOf(xw);

    if (!XtIsRealized((Widget) xw) || (CURRENT_EMU() != (Widget) xw)) {
	Bell(xw, XkbBI_MinorError, 0);
	return;
    }

    if (saved) {
	screen->savedlines = 0;
	ScrollBarDrawThumb(screen->scrollWidget);
    }

    /* make cursor visible */
    screen->cursor_set = ON;
    InitCursorShape(screen, screen);
#if OPT_BLINK_CURS
    TRACE(("cursor_shape:%d blinks:%d\n",
	   screen->cursor_shape,
	   screen->cursor_blink));
#endif

    /* reset scrolling region */
    reset_margins(screen);

    bitclr(&xw->flags, ORIGIN);

    if_OPT_ISO_COLORS(screen, {
	static char empty[1];
	reset_SGR_Colors(xw);
	if (ResetAnsiColorRequest(xw, empty, 0))
	    xtermRepaint(xw);
    });

    /* Reset character-sets to initial state */
    resetCharsets(screen);

#if OPT_MOD_FKEYS
    /* Reset modifier-resources to initial state */
    xw->keyboard.modify_now = xw->keyboard.modify_1st;
#endif

    /* Reset DECSCA */
    bitclr(&xw->flags, PROTECTED);
    screen->protected_mode = OFF_PROTECT;

    reset_displayed_graphics(screen);

    if (full) {			/* RIS */
	if (screen->bellOnReset)
	    Bell(xw, XkbBI_TerminalBell, 0);

	/* reset the mouse mode */
	screen->send_mouse_pos = MOUSE_OFF;
	screen->send_focus_pos = OFF;
	screen->extend_coords = 0;
	screen->waitingForTrackInfo = False;
	screen->eventMode = NORMAL;

	xtermShowPointer(xw, True);

	TabReset(xw->tabs);
	xw->keyboard.flags = MODE_SRM;

	guard_keyboard_type = False;
	screen->old_fkeys = screen->old_fkeys0;
	decode_keyboard_type(xw, &resource);
	update_keyboard_type();

#if OPT_INITIAL_ERASE
	if (xw->keyboard.reset_DECBKM == 1)
	    xw->keyboard.flags |= MODE_DECBKM;
	else if (xw->keyboard.reset_DECBKM == 2)
#endif
	    if (TScreenOf(xw)->backarrow_key)
		xw->keyboard.flags |= MODE_DECBKM;
	TRACE(("full reset DECBKM %s\n",
	       BtoS(xw->keyboard.flags & MODE_DECBKM)));

#if OPT_SCROLL_LOCK
	xtermClearLEDs(screen);
#endif
	screen->title_modes = screen->title_modes0;
	screen->pointer_mode = screen->pointer_mode0;
#if OPT_SIXEL_GRAPHICS
	if (TScreenOf(xw)->sixel_scrolling)
	    xw->keyboard.flags |= MODE_DECSDM;
	TRACE(("full reset DECSDM to %s (resource default is %s)\n",
	       BtoS(xw->keyboard.flags & MODE_DECSDM),
	       BtoS(TScreenOf(xw)->sixel_scrolling)));
#endif

#if OPT_GRAPHICS
	screen->privatecolorregisters = TScreenOf(xw)->privatecolorregisters;
	TRACE(("full reset PRIVATE_COLOR_REGISTERS to %s (resource default is %s)\n",
	       BtoS(screen->privatecolorregisters),
	       BtoS(TScreenOf(xw)->privatecolorregisters)));
#endif

#if OPT_SIXEL_GRAPHICS
	screen->sixel_scrolls_right = TScreenOf(xw)->sixel_scrolls_right;
	TRACE(("full reset SIXEL_SCROLLS_RIGHT to %s (resource default is %s)\n",
	       BtoS(screen->sixel_scrolls_right),
	       BtoS(TScreenOf(xw)->sixel_scrolls_right)));
#endif

	update_appcursor();
	update_appkeypad();
	update_decbkm();
	update_decsdm();
	show_8bit_control(False);
	reset_decudk(xw);

	FromAlternate(xw);
	ClearScreen(xw);
	screen->cursor_state = OFF;
	if (xw->flags & REVERSE_VIDEO)
	    ReverseVideo(xw);

	xw->flags = xw->initflags;
	update_reversevideo();
	update_autowrap();
	update_reversewrap();
	update_autolinefeed();

	screen->jumpscroll = (Boolean) (!(xw->flags & SMOOTHSCROLL));
	update_jumpscroll();

#if OPT_DEC_RECTOPS
	screen->cur_decsace = 0;
#endif
#if OPT_READLINE
	screen->click1_moves = OFF;
	screen->paste_moves = OFF;
	screen->dclick3_deletes = OFF;
	screen->paste_brackets = OFF;
	screen->paste_quotes = OFF;
	screen->paste_literal_nl = OFF;
#endif /* OPT_READLINE */

	if (screen->c132 && (xw->flags & IN132COLUMNS)) {
	    Dimension reqWidth = (Dimension) (80 * FontWidth(screen)
					      + 2 * screen->border
					      + ScrollbarWidth(screen));
	    Dimension reqHeight = (Dimension) (FontHeight(screen)
					       * MaxRows(screen)
					       + 2 * screen->border);
	    Dimension replyWidth;
	    Dimension replyHeight;

	    TRACE(("Making resize-request to restore 80-columns %dx%d\n",
		   reqHeight, reqWidth));
	    REQ_RESIZE((Widget) xw,
		       reqWidth,
		       reqHeight,
		       &replyWidth, &replyHeight);
	    repairSizeHints();
	    XSync(screen->display, False);	/* synchronize */
	    if (xtermAppPending())
		xevents();
	}

	CursorSet(screen, 0, 0, xw->flags);
	CursorSave(xw);
    } else {			/* DECSTR */
	/*
	 * There's a tiny difference, to accommodate usage of xterm.
	 * We reset autowrap to the resource values rather than turning
	 * it off.
	 */
	UIntClr(xw->keyboard.flags, (MODE_DECCKM | MODE_KAM | MODE_DECKPAM));
	bitcpy(&xw->flags, xw->initflags, WRAPAROUND | REVERSEWRAP);
	bitclr(&xw->flags, INSERT | INVERSE | BOLD | BLINK | UNDERLINE | INVISIBLE);
	if_OPT_ISO_COLORS(screen, {
	    reset_SGR_Colors(xw);
	});
	update_appcursor();
	update_autowrap();
	update_reversewrap();

	CursorSave(xw);
	screen->sc[screen->whichBuf].row =
	    screen->sc[screen->whichBuf].col = 0;
    }
}

void
VTReset(XtermWidget xw, Bool full, Bool saved)
{
    ReallyReset(xw, full, saved);
    longjmp(vtjmpbuf, 1);	/* force ground state in parser */
}

typedef enum {
    ccLO,
    ccDASH,
    ccHI,
    ccCOLON,
    ccID,
    ccCOMMA
} CCLASS;

/*
 * set_character_class - takes a string of the form
 *
 *   low[-high][:id][,low[-high][:id][...]]
 *
 * and sets the indicated ranges to the indicated values.
 */
static int
set_character_class(char *s)
{
#define FMT "%s in range string \"%s\" (position %d)\n"

    TRACE(("set_character_class(%s) {{\n", NonNull(s)));
    if (IsEmpty(s)) {
	return -1;
    } else {
	CCLASS state = ccLO;
	int arg[3];
	int i;
	int len = (int) strlen(s);

	arg[0] =
	    arg[1] =
	    arg[2] = -1;

	for (i = 0; i < len; ++i) {
	    int ch = CharOf(s[i]);
	    char *t = 0;
	    long value = 0;

	    if (isspace(ch))
		continue;

	    switch (state) {
	    case ccLO:
	    case ccHI:
	    case ccID:
		if (!isdigit(ch)) {
		    xtermWarning(FMT, "missing number", s, i);
		    return (-1);
		}
		value = strtol(s + i, &t, 0);
		i = (int) (t - s - 1);
		break;
	    case ccDASH:
	    case ccCOLON:
	    case ccCOMMA:
		break;
	    }

	    switch (state) {
	    case ccLO:
		arg[0] =
		    arg[1] = (int) value;
		arg[2] = -1;
		state = ccDASH;
		break;

	    case ccDASH:
		if (ch == '-') {
		    state = ccHI;
		} else {
		    goto parse_class;
		}
		break;

	    case ccHI:
		arg[1] = (int) value;
		state = ccCOLON;
		break;

	      parse_class:
	    case ccCOLON:
		if (ch == ':') {
		    state = ccID;
		} else if (ch == ',') {
		    goto apply_class;
		} else {
		    xtermWarning(FMT, "unexpected character", s, i);
		    return (-1);
		}
		break;

	    case ccID:
		arg[2] = (int) value;
		state = ccCOMMA;
		break;

	      apply_class:
	    case ccCOMMA:
		if (SetCharacterClassRange(arg[0], arg[1], arg[2]) != 0) {
		    xtermWarning(FMT, "bad range", s, i);
		    return -1;
		}
		state = ccLO;
		break;
	    }
	}
	if (state >= ccDASH) {
	    if (SetCharacterClassRange(arg[0], arg[1], arg[2]) != 0) {
		xtermWarning(FMT, "bad range", s, i);
		return -1;
	    }
	}
    }

    TRACE(("}} set_character_class\n"));
    return (0);
#undef FMT
}

void
getKeymapResources(Widget w,
		   const char *mapName,
		   const char *mapClass,
		   const char *type,
		   void *result,
		   size_t size)
{
    XtResource key_resources[1];
    key_resources[0].resource_name = XtNtranslations;
    key_resources[0].resource_class = XtCTranslations;
    key_resources[0].resource_type = (char *) type;
    key_resources[0].resource_size = (Cardinal) size;
    key_resources[0].resource_offset = 0;
    key_resources[0].default_type = key_resources[0].resource_type;
    key_resources[0].default_addr = 0;
    XtGetSubresources(w, (XtPointer) result, mapName, mapClass,
		      key_resources, (Cardinal) 1, NULL, (Cardinal) 0);
}

/* ARGSUSED */
static void
HandleKeymapChange(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    static XtTranslations keymap, original;

    TRACE(("HandleKeymapChange(%#lx, %s)\n",
	   (unsigned long) w,
	   (*param_count
	    ? params[0]
	    : "missing")));

    if (*param_count != 1)
	return;

    if (original == NULL) {
	TRACE(("...saving original keymap-translations\n"));
	original = w->core.tm.translations;
    }

    if (strcmp(params[0], "None") == 0) {
	TRACE(("...restoring original keymap-translations\n"));
	XtOverrideTranslations(w, original);
    } else {
	char mapName[1000];
	char mapClass[1000];
	char *pmapName;
	char *pmapClass;
	size_t len;

	len = strlen(params[0]) + 7;

	pmapName = (char *) MyStackAlloc(len, mapName);
	pmapClass = (char *) MyStackAlloc(len, mapClass);
	if (pmapName == NULL
	    || pmapClass == NULL) {
	    SysError(ERROR_KMMALLOC1);
	} else {

	    (void) sprintf(pmapName, "%sKeymap", params[0]);
	    (void) strcpy(pmapClass, pmapName);
	    if (islower(CharOf(pmapClass[0])))
		pmapClass[0] = x_toupper(pmapClass[0]);
	    getKeymapResources(w, pmapName, pmapClass, XtRTranslationTable,
			       &keymap, sizeof(keymap));
	    if (keymap != NULL) {
		TRACE(("...applying keymap \"%s\"\n", pmapName));
		XtOverrideTranslations(w, keymap);
	    } else {
		TRACE(("...found no match for keymap \"%s\"\n", pmapName));
	    }

	    MyStackFree(pmapName, mapName);
	    MyStackFree(pmapClass, mapClass);
	}
    }
}

/* ARGSUSED */
static void
HandleBell(Widget w GCC_UNUSED,
	   XEvent *event GCC_UNUSED,
	   String *params,	/* [0] = volume */
	   Cardinal *param_count)	/* 0 or 1 */
{
    int percent = (*param_count) ? atoi(params[0]) : 0;

    Bell(term, XkbBI_TerminalBell, percent);
}

/* ARGSUSED */
static void
HandleVisualBell(Widget w GCC_UNUSED,
		 XEvent *event GCC_UNUSED,
		 String *params GCC_UNUSED,
		 Cardinal *param_count GCC_UNUSED)
{
    VisualBell();
}

/* ARGSUSED */
static void
HandleIgnore(Widget w,
	     XEvent *event,
	     String *params GCC_UNUSED,
	     Cardinal *param_count GCC_UNUSED)
{
    XtermWidget xw;

    TRACE(("Handle ignore for %p %s\n",
	   (void *) w, visibleEventType(event->type)));
    if ((xw = getXtermWidget(w)) != 0) {
	/* do nothing, but check for funny escape sequences */
	(void) SendMousePosition(xw, event);
    }
}

/* ARGSUSED */
static void
DoSetSelectedFont(Widget w,
		  XtPointer client_data GCC_UNUSED,
		  Atom *selection GCC_UNUSED,
		  Atom *type,
		  XtPointer value,
		  unsigned long *length,
		  int *format)
{
    XtermWidget xw = getXtermWidget(w);

    if (xw == 0) {
	xtermWarning("unexpected widget in DoSetSelectedFont\n");
    } else if (*type != XA_STRING || *format != 8) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	Boolean failed = False;
	int oldFont = TScreenOf(xw)->menu_font_number;
	char *save = TScreenOf(xw)->SelectFontName();
	char *val;
	char *test = 0;
	unsigned len = (unsigned) *length;
	unsigned tst;

	/*
	 * Some versions of X deliver null-terminated selections, some do not.
	 */
	for (tst = 0; tst < len; ++tst) {
	    if (((char *) value)[tst] == '\0') {
		len = tst;
		break;
	    }
	}

	if (len > 0 && (val = TypeMallocN(char, len + 1)) != 0) {
	    char *used;

	    memcpy(val, value, (size_t) len);
	    val[len] = '\0';
	    used = x_strtrim(val);
	    TRACE(("DoSetSelectedFont(%s)\n", used));
	    /* Do some sanity checking to avoid sending a long selection
	       back to the server in an OpenFont that is unlikely to succeed.
	       XLFD allows up to 255 characters and no control characters;
	       we are a little more liberal here. */
	    if (len < 1000
		&& used != 0
		&& !strchr(used, '\n')
		&& (test = x_strdup(used)) != 0) {
		TScreenOf(xw)->SelectFontName() = test;
		if (!xtermLoadFont(term,
				   xtermFontName(used),
				   True,
				   fontMenu_fontsel)) {
		    failed = True;
		    free(test);
		    TScreenOf(xw)->SelectFontName() = save;
		}
	    } else {
		failed = True;
	    }
	    if (failed) {
		(void) xtermLoadFont(term,
				     xtermFontName(TScreenOf(xw)->MenuFontName(oldFont)),
				     True,
				     oldFont);
		Bell(xw, XkbBI_MinorError, 0);
	    }
	    free(used);
	    free(val);
	}
    }
}

void
FindFontSelection(XtermWidget xw, const char *atom_name, Bool justprobe)
{
    TScreen *screen = TScreenOf(xw);
    static AtomPtr *atoms;
    static unsigned int atomCount = 0;
    AtomPtr *pAtom;
    unsigned a;
    Atom target;

    if (!atom_name)
	atom_name = ((screen->mappedSelect && atomCount)
		     ? screen->mappedSelect[0]
		     : "PRIMARY");
    TRACE(("FindFontSelection(%s)\n", atom_name));

    for (pAtom = atoms, a = atomCount; a; a--, pAtom++) {
	if (strcmp(atom_name, XmuNameOfAtom(*pAtom)) == 0) {
	    TRACE(("...found atom %d:%s\n", a + 1, atom_name));
	    break;
	}
    }
    if (!a) {
	atoms = TypeXtReallocN(AtomPtr, atoms, atomCount + 1);
	*(pAtom = &atoms[atomCount]) = XmuMakeAtom(atom_name);
	++atomCount;
	TRACE(("...added atom %d:%s\n", atomCount, atom_name));
    }

    target = XmuInternAtom(XtDisplay(xw), *pAtom);
    if (justprobe) {
	screen->SelectFontName() =
	    XGetSelectionOwner(XtDisplay(xw), target) ? _Font_Selected_ : 0;
	TRACE(("...selected fontname '%s'\n",
	       NonNull(screen->SelectFontName())));
    } else {
	XtGetSelectionValue((Widget) xw, target, XA_STRING,
			    DoSetSelectedFont, NULL,
			    XtLastTimestampProcessed(XtDisplay(xw)));
    }
    return;
}

Bool
set_cursor_gcs(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    VTwin *win = WhichVWin(screen);

    Pixel cc = T_COLOR(screen, TEXT_CURSOR);
    Pixel fg = T_COLOR(screen, TEXT_FG);
    Pixel bg = T_COLOR(screen, TEXT_BG);
    Bool changed = False;

    /*
     * Let's see, there are three things that have "color":
     *
     *     background
     *     text
     *     cursorblock
     *
     * And, there are four situations when drawing a cursor, if we decide
     * that we like have a solid block of cursor color with the letter
     * that it is highlighting shown in the background color to make it
     * stand out:
     *
     *     selected window, normal video - background on cursor
     *     selected window, reverse video - foreground on cursor
     *     unselected window, normal video - foreground on background
     *     unselected window, reverse video - background on foreground
     *
     * Since the last two are really just normalGC and reverseGC, we only
     * need two new GC's.  Under monochrome, we get the same effect as
     * above by setting cursor color to foreground.
     */

    TRACE(("set_cursor_gcs cc=%#lx, fg=%#lx, bg=%#lx\n", cc, fg, bg));
    if (win != 0 && (cc != bg)) {
	/* set the fonts to the current one */
	setCgsFont(xw, win, gcVTcursNormal, 0);
	setCgsFont(xw, win, gcVTcursFilled, 0);
	setCgsFont(xw, win, gcVTcursReverse, 0);
	setCgsFont(xw, win, gcVTcursOutline, 0);

	/* we have a colored cursor */
	setCgsFore(xw, win, gcVTcursNormal, fg);
	setCgsBack(xw, win, gcVTcursNormal, cc);

	setCgsFore(xw, win, gcVTcursFilled, cc);
	setCgsBack(xw, win, gcVTcursFilled, fg);

	if (screen->always_highlight) {
	    /* both GC's use the same color */
	    setCgsFore(xw, win, gcVTcursReverse, bg);
	    setCgsBack(xw, win, gcVTcursReverse, cc);
	} else {
	    setCgsFore(xw, win, gcVTcursReverse, bg);
	    setCgsBack(xw, win, gcVTcursReverse, cc);
	}
	set_cursor_outline_gc(xw, screen->always_highlight, fg, bg, cc);
	changed = True;
    }

    if (changed) {
	TRACE(("...set_cursor_gcs - done\n"));
    }
    return changed;
}

/*
 * Build up the default translations string, allowing the user to suppress
 * some of the features.
 */
void
VTInitTranslations(void)
{
    /* *INDENT-OFF* */
    static struct {
	Boolean wanted;
	const char *name;
	const char *value;
    } table[] = {
	{
	    False,
	    "default",
"\
          Shift <KeyPress> Prior:scroll-back(1,halfpage) \n\
           Shift <KeyPress> Next:scroll-forw(1,halfpage) \n\
         Shift <KeyPress> Select:select-cursor-start() select-cursor-end(SELECT, CUT_BUFFER0) \n\
         Shift <KeyPress> Insert:insert-selection(SELECT, CUT_BUFFER0) \n\
"
	},
#if OPT_MAXIMIZE
	{
	    False,
	    "fullscreen",
"\
                 Alt <Key>Return:fullscreen() \n\
"
	},
#endif
#if OPT_SCROLL_LOCK
	{
	    False,
	    "scroll-lock",
"\
        <KeyRelease> Scroll_Lock:scroll-lock() \n\
"
	},
#endif
#if OPT_SHIFT_FONTS
	{
	    False,
	    "shift-fonts",
"\
    Shift~Ctrl <KeyPress> KP_Add:larger-vt-font() \n\
    Shift Ctrl <KeyPress> KP_Add:smaller-vt-font() \n\
    Shift <KeyPress> KP_Subtract:smaller-vt-font() \n\
"
	},
#endif
	/* PROCURA added "Meta <Btn2Down>:clear-saved-lines()" */
	{
	    False,
	    "default",
"\
                ~Meta <KeyPress>:insert-seven-bit() \n\
                 Meta <KeyPress>:insert-eight-bit() \n\
                !Ctrl <Btn1Down>:popup-menu(mainMenu) \n\
           !Lock Ctrl <Btn1Down>:popup-menu(mainMenu) \n\
 !Lock Ctrl @Num_Lock <Btn1Down>:popup-menu(mainMenu) \n\
     ! @Num_Lock Ctrl <Btn1Down>:popup-menu(mainMenu) \n\
                ~Meta <Btn1Down>:select-start() \n\
              ~Meta <Btn1Motion>:select-extend() \n\
                !Ctrl <Btn2Down>:popup-menu(vtMenu) \n\
           !Lock Ctrl <Btn2Down>:popup-menu(vtMenu) \n\
 !Lock Ctrl @Num_Lock <Btn2Down>:popup-menu(vtMenu) \n\
     ! @Num_Lock Ctrl <Btn2Down>:popup-menu(vtMenu) \n\
          ~Ctrl ~Meta <Btn2Down>:ignore() \n\
                 Meta <Btn2Down>:clear-saved-lines() \n\
            ~Ctrl ~Meta <Btn2Up>:insert-selection(SELECT, CUT_BUFFER0) \n\
                !Ctrl <Btn3Down>:popup-menu(fontMenu) \n\
           !Lock Ctrl <Btn3Down>:popup-menu(fontMenu) \n\
 !Lock Ctrl @Num_Lock <Btn3Down>:popup-menu(fontMenu) \n\
     ! @Num_Lock Ctrl <Btn3Down>:popup-menu(fontMenu) \n\
          ~Ctrl ~Meta <Btn3Down>:start-extend() \n\
              ~Meta <Btn3Motion>:select-extend() \n\
"
	},
	{
	    False,
	    "wheel-mouse",
"\
                 Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
            Lock Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
  Lock @Num_Lock Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
       @Num_Lock Ctrl <Btn4Down>:scroll-back(1,halfpage,m) \n\
                      <Btn4Down>:scroll-back(5,line,m)     \n\
                 Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
            Lock Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
  Lock @Num_Lock Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
       @Num_Lock Ctrl <Btn5Down>:scroll-forw(1,halfpage,m) \n\
                      <Btn5Down>:scroll-forw(5,line,m)     \n\
"
	},
	{
	    False,
	    "default",
"\
                         <BtnUp>:select-end(SELECT, CUT_BUFFER0) \n\
                       <BtnDown>:ignore() \
"
	}
    };
    /* *INDENT-ON* */

    char *result = 0;

    int pass;
    Cardinal item;

    TRACE(("VTInitTranslations\n"));
    for (item = 0; item < XtNumber(table); ++item) {
	table[item].wanted = True;
    }
#if OPT_MAXIMIZE
    /*
     * As a special case, allow for disabling the alt-enter translation if
     * the resource settings prevent fullscreen from being used.  We would
     * do the same for scroll-lock and shift-fonts if they were application
     * resources too, rather than in the widget.
     */
    if (resource.fullscreen == esNever) {
	for (item = 0; item < XtNumber(table); ++item) {
	    if (!strcmp(table[item].name, "fullscreen")) {
		table[item].wanted = False;
		TRACE(("omit(%s):\n%s\n", table[item].name, table[item].value));
	    }
	}
    }
#endif
    if (!IsEmpty(resource.omitTranslation)) {
	char *value;
	const char *source = resource.omitTranslation;

	while (*source != '\0' && (value = ParseList(&source)) != 0) {
	    size_t len = strlen(value);

	    TRACE(("parsed:%s\n", value));
	    for (item = 0; item < XtNumber(table); ++item) {
		if (strlen(table[item].name) >= len
		    && x_strncasecmp(table[item].name,
				     value,
				     (unsigned) len) == 0) {
		    table[item].wanted = False;
		    TRACE(("omit(%s):\n%s\n", table[item].name, table[item].value));
		    break;
		}
	    }
	    free(value);
	}
    }

    for (pass = 0; pass < 2; ++pass) {
	size_t needed = 0;
	for (item = 0; item < XtNumber(table); ++item) {
	    if (table[item].wanted) {
		if (pass) {
		    strcat(result, table[item].value);
		} else {
		    needed += strlen(table[item].value) + 1;
		}
	    }
	}
	if (!pass) {
	    result = XtMalloc((Cardinal) needed);
	    *result = '\0';
	}
    }

    TRACE(("result:\n%s\n", result));

    defaultTranslations = result;
    xtermClassRec.core_class.tm_table = result;
}

#ifdef NO_LEAKS
void
noleaks_charproc(void)
{
    if (v_buffer != 0)
	free(v_buffer);
}
#endif
