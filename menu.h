/* $XTermId: menu.h,v 1.139 2016/12/22 02:04:51 tom Exp $ */

/*
 * Copyright 1999-2015,2016 by Thomas E. Dickey
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
 * Copyright 1989  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of The Open Group shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from The Open Group.
 *
 */

#ifndef included_menu_h
#define included_menu_h
/* *INDENT-OFF* */

#include <xterm.h>

typedef struct _MenuEntry {
    const char *name;
    void (*function) PROTO_XT_CALLBACK_ARGS;
    Widget widget;
} MenuEntry;

extern MenuEntry mainMenuEntries[], vtMenuEntries[];
extern MenuEntry fontMenuEntries[];
#if OPT_TEK4014
extern MenuEntry tekMenuEntries[];
#endif

extern void Handle8BitControl      PROTO_XT_ACTIONS_ARGS;
extern void HandleAllow132         PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowBoldFonts   PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowColorOps    PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowFontOps     PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowMouseOps    PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowSends       PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowTcapOps     PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowTitleOps    PROTO_XT_ACTIONS_ARGS;
extern void HandleAllowWindowOps   PROTO_XT_ACTIONS_ARGS;
extern void HandleAltEsc           PROTO_XT_ACTIONS_ARGS;
extern void HandleAltScreen        PROTO_XT_ACTIONS_ARGS;
extern void HandleAppCursor        PROTO_XT_ACTIONS_ARGS;
extern void HandleAppKeypad        PROTO_XT_ACTIONS_ARGS;
extern void HandleAutoLineFeed     PROTO_XT_ACTIONS_ARGS;
extern void HandleAutoWrap         PROTO_XT_ACTIONS_ARGS;
extern void HandleBackarrow        PROTO_XT_ACTIONS_ARGS;
extern void HandleBellIsUrgent     PROTO_XT_ACTIONS_ARGS;
extern void HandleClearSavedLines  PROTO_XT_ACTIONS_ARGS;
extern void HandleCreateMenu       PROTO_XT_ACTIONS_ARGS;
extern void HandleCursesEmul       PROTO_XT_ACTIONS_ARGS;
extern void HandleCursorBlink      PROTO_XT_ACTIONS_ARGS;
extern void HandleDeleteIsDEL      PROTO_XT_ACTIONS_ARGS;
extern void HandleDumpHtml         PROTO_XT_ACTIONS_ARGS;
extern void HandleDumpSvg          PROTO_XT_ACTIONS_ARGS;
extern void HandleFontBoxChars     PROTO_XT_ACTIONS_ARGS;
extern void HandleFontDoublesize   PROTO_XT_ACTIONS_ARGS;
extern void HandleFontLoading      PROTO_XT_ACTIONS_ARGS;
extern void HandleFontPacked       PROTO_XT_ACTIONS_ARGS;
extern void HandleFullscreen       PROTO_XT_ACTIONS_ARGS;
extern void HandleHardReset        PROTO_XT_ACTIONS_ARGS;
extern void HandleHpFunctionKeys   PROTO_XT_ACTIONS_ARGS;
extern void HandleJumpscroll       PROTO_XT_ACTIONS_ARGS;
extern void HandleKeepClipboard    PROTO_XT_ACTIONS_ARGS;
extern void HandleKeepSelection    PROTO_XT_ACTIONS_ARGS;
extern void HandleLogging          PROTO_XT_ACTIONS_ARGS;
extern void HandleMarginBell       PROTO_XT_ACTIONS_ARGS;
extern void HandleMetaEsc          PROTO_XT_ACTIONS_ARGS;
extern void HandleNumLock          PROTO_XT_ACTIONS_ARGS;
extern void HandleOldFunctionKeys  PROTO_XT_ACTIONS_ARGS;
extern void HandlePopupMenu        PROTO_XT_ACTIONS_ARGS;
extern void HandlePrintControlMode PROTO_XT_ACTIONS_ARGS;
extern void HandlePrintEverything  PROTO_XT_ACTIONS_ARGS;
extern void HandlePrintScreen      PROTO_XT_ACTIONS_ARGS;
extern void HandleQuit             PROTO_XT_ACTIONS_ARGS;
extern void HandleRedraw           PROTO_XT_ACTIONS_ARGS;
extern void HandleRenderFont       PROTO_XT_ACTIONS_ARGS;
extern void HandleReverseVideo     PROTO_XT_ACTIONS_ARGS;
extern void HandleReverseWrap      PROTO_XT_ACTIONS_ARGS;
extern void HandleScoFunctionKeys  PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollKey        PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollTtyOutput  PROTO_XT_ACTIONS_ARGS;
extern void HandleScrollbar        PROTO_XT_ACTIONS_ARGS;
extern void HandleSecure           PROTO_XT_ACTIONS_ARGS;
extern void HandleSendSignal       PROTO_XT_ACTIONS_ARGS;
extern void HandleSetPopOnBell     PROTO_XT_ACTIONS_ARGS;
extern void HandleSetPrivateColorRegisters PROTO_XT_ACTIONS_ARGS;
extern void HandleSetSelect        PROTO_XT_ACTIONS_ARGS;
extern void HandleSetTekText       PROTO_XT_ACTIONS_ARGS;
extern void HandleSetTerminalType  PROTO_XT_ACTIONS_ARGS;
extern void HandleSetVisualBell    PROTO_XT_ACTIONS_ARGS;
extern void HandleSixelScrolling   PROTO_XT_ACTIONS_ARGS;
extern void HandleSoftReset        PROTO_XT_ACTIONS_ARGS;
extern void HandleSunFunctionKeys  PROTO_XT_ACTIONS_ARGS;
extern void HandleSunKeyboard      PROTO_XT_ACTIONS_ARGS;
extern void HandleTekCopy          PROTO_XT_ACTIONS_ARGS;
extern void HandleTekPage          PROTO_XT_ACTIONS_ARGS;
extern void HandleTekReset         PROTO_XT_ACTIONS_ARGS;
extern void HandleTiteInhibit      PROTO_XT_ACTIONS_ARGS;
extern void HandleToolbar          PROTO_XT_ACTIONS_ARGS;
extern void HandleUTF8Fonts        PROTO_XT_ACTIONS_ARGS;
extern void HandleUTF8Mode         PROTO_XT_ACTIONS_ARGS;
extern void HandleUTF8Title        PROTO_XT_ACTIONS_ARGS;
extern void HandleVisibility       PROTO_XT_ACTIONS_ARGS;
extern void HandleWriteError       PROTO_XT_ACTIONS_ARGS;
extern void HandleWriteNow         PROTO_XT_ACTIONS_ARGS;

extern void SetupMenus (Widget /*shell*/, Widget */*forms*/, Widget */*menus*/, Dimension * /*menu_high*/);

#if OPT_TOOLBAR
extern void ShowToolbar(Bool);
#endif

/*
 * The following definitions MUST match the order of entries given in
 * the mainMenuEntries, vtMenuEntries, and tekMenuEntries arrays in menu.c.
 */

/*
 * items in primary menu
 */
typedef enum {
#if OPT_TOOLBAR
    mainMenu_toolbar,
#endif
#if OPT_MAXIMIZE
    mainMenu_fullscreen,
#endif
    mainMenu_securekbd,
    mainMenu_allowsends,
    mainMenu_redraw,
    mainMenu_line1,
#ifdef ALLOWLOGGING
    mainMenu_logging,
#endif
#if OPT_PRINT_ON_EXIT
    mainMenu_write_now,
    mainMenu_write_error,
#endif
    mainMenu_print,
    mainMenu_print_redir,
#if OPT_SCREEN_DUMPS
    mainMenu_dump_html,
    mainMenu_dump_svg,
#endif
    mainMenu_line2,
    mainMenu_8bit_ctrl,
    mainMenu_backarrow,
#if OPT_NUM_LOCK
    mainMenu_num_lock,
    mainMenu_alt_esc,
    mainMenu_meta_esc,
#endif
    mainMenu_delete_del,
    mainMenu_old_fkeys,
#if OPT_TCAP_FKEYS
    mainMenu_tcap_fkeys,
#endif
#if OPT_HP_FUNC_KEYS
    mainMenu_hp_fkeys,
#endif
#if OPT_SCO_FUNC_KEYS
    mainMenu_sco_fkeys,
#endif
#if OPT_SUN_FUNC_KEYS
    mainMenu_sun_fkeys,
#endif
#if OPT_SUNPC_KBD
    mainMenu_sun_kbd,
#endif
    mainMenu_line3,
    mainMenu_suspend,
    mainMenu_continue,
    mainMenu_interrupt,
    mainMenu_hangup,
    mainMenu_terminate,
    mainMenu_kill,
    mainMenu_line4,
    mainMenu_quit,
    mainMenu_LAST
} mainMenuIndices;


/*
 * items in vt100 mode menu
 */
typedef enum {
    vtMenu_scrollbar,
    vtMenu_jumpscroll,
    vtMenu_reversevideo,
    vtMenu_autowrap,
    vtMenu_reversewrap,
    vtMenu_autolinefeed,
    vtMenu_appcursor,
    vtMenu_appkeypad,
    vtMenu_scrollkey,
    vtMenu_scrollttyoutput,
    vtMenu_allow132,
    vtMenu_keepSelection,
    vtMenu_selectToClipboard,
    vtMenu_visualbell,
    vtMenu_bellIsUrgent,
    vtMenu_poponbell,
#if OPT_BLINK_CURS
    vtMenu_cursorblink,
#endif
    vtMenu_titeInhibit,
#ifndef NO_ACTIVE_ICON
    vtMenu_activeicon,
#endif /* NO_ACTIVE_ICON */
    vtMenu_line1,
    vtMenu_softreset,
    vtMenu_hardreset,
    vtMenu_clearsavedlines,
    vtMenu_line2,
#if OPT_TEK4014
    vtMenu_tekshow,
    vtMenu_tekmode,
    vtMenu_vthide,
#endif
    vtMenu_altscreen,
#if OPT_SIXEL_GRAPHICS
    vtMenu_sixelscrolling,
#endif
#if OPT_GRAPHICS
    vtMenu_privatecolorregisters,
#endif
    vtMenu_LAST
} vtMenuIndices;

/*
 * items in vt100 font menu
 */
typedef enum {
    fontMenu_default,
    fontMenu_font1,
    fontMenu_font2,
    fontMenu_font3,
    fontMenu_font4,
    fontMenu_font5,
    fontMenu_font6,
#define fontMenu_lastBuiltin fontMenu_font6
    fontMenu_fontescape,
    fontMenu_fontsel,
/* number of non-line items down to here should match NMENUFONTS in ptyx.h */

#if OPT_DEC_CHRSET || OPT_BOX_CHARS || OPT_DEC_SOFTFONT
    fontMenu_line1,
    fontMenu_allowBoldFonts,
#if OPT_BOX_CHARS
    fontMenu_font_boxchars,
    fontMenu_font_packedfont,
#endif
#if OPT_DEC_CHRSET
    fontMenu_font_doublesize,
#endif
#if OPT_DEC_SOFTFONT
    fontMenu_font_loadable,
#endif
#endif

#if OPT_RENDERFONT || OPT_WIDE_CHARS
    fontMenu_line2,
#if OPT_RENDERFONT
    fontMenu_render_font,
#endif
#if OPT_WIDE_CHARS
    fontMenu_utf8_mode,
    fontMenu_utf8_fonts,
    fontMenu_utf8_title,
#endif
#endif
#if OPT_ALLOW_XXX_OPS
    fontMenu_line3,
    fontMenu_allowColorOps,
    fontMenu_allowFontOps,
    fontMenu_allowMouseOps,
    fontMenu_allowTcapOps,
    fontMenu_allowTitleOps,
    fontMenu_allowWindowOps,
#endif

    fontMenu_LAST
} fontMenuIndices;

/*
 * items in tek4014 mode menu
 */
#if OPT_TEK4014
typedef enum {
    tekMenu_tektextlarge,
    tekMenu_tektext2,
    tekMenu_tektext3,
    tekMenu_tektextsmall,
    tekMenu_line1,
    tekMenu_tekpage,
    tekMenu_tekreset,
    tekMenu_tekcopy,
    tekMenu_line2,
    tekMenu_vtshow,
    tekMenu_vtmode,
    tekMenu_tekhide,
    tekMenu_LAST
} tekMenuIndices;
#endif


/*
 * functions for updating menus
 */

extern void SetItemSensitivity(Widget mi, Bool val);

typedef enum {
    toggleErr = -2,
    toggleAll = -1,
    toggleOff = 0,
    toggleOn = 1
} ToggleEnum;

extern int decodeToggle(XtermWidget /* xw */, String * /* params */, Cardinal /* nparams */);

/*
 * there should be one of each of the following for each checkable item
 */
#if OPT_TOOLBAR
extern void update_toolbar(void);
#else
#define update_toolbar() /* nothing */
#endif

#if OPT_MAXIMIZE
extern void update_fullscreen(void);
#else
#define update_fullscreen() /* nothing */
#endif

extern void update_securekbd(void);
extern void update_allowsends(void);

#ifdef ALLOWLOGGING
extern void update_logging(void);
#else
#define update_logging() /*nothing*/
#endif

#if OPT_PRINT_ON_EXIT
extern void update_write_error(void);
#else
#define update_write_error() /*nothing*/
#endif

extern void update_print_redir(void);
extern void update_8bit_control(void);
extern void update_decbkm(void);

#if OPT_NUM_LOCK
extern void update_num_lock(void);
extern void update_alt_esc(void);
extern void update_meta_esc(void);
#else
#define update_num_lock() /*nothing*/
#define update_alt_esc()  /*nothing*/
#define update_meta_esc() /*nothing*/
#endif

extern void update_old_fkeys(void);
extern void update_delete_del(void);

#if OPT_SUNPC_KBD
extern void update_sun_kbd(void);
#endif

#if OPT_HP_FUNC_KEYS
extern void update_hp_fkeys(void);
#else
#define update_hp_fkeys() /*nothing*/
#endif

#if OPT_SCO_FUNC_KEYS
extern void update_sco_fkeys(void);
#else
#define update_sco_fkeys() /*nothing*/
#endif

#if OPT_SUN_FUNC_KEYS
extern void update_sun_fkeys(void);
#else
#define update_sun_fkeys() /*nothing*/
#endif

#if OPT_TCAP_FKEYS
extern void update_tcap_fkeys(void);
#else
#define update_tcap_fkeys() /*nothing*/
#endif

extern void update_scrollbar(void);
extern void update_jumpscroll(void);
extern void update_reversevideo(void);
extern void update_autowrap(void);
extern void update_reversewrap(void);
extern void update_autolinefeed(void);
extern void update_appcursor(void);
extern void update_appkeypad(void);
extern void update_scrollkey(void);
extern void update_keepSelection(void);
extern void update_selectToClipboard(void);
extern void update_scrollttyoutput(void);
extern void update_allow132(void);
extern void update_cursesemul(void);
extern void update_visualbell(void);
extern void update_bellIsUrgent(void);
extern void update_poponbell(void);

#define update_keepClipboard() /* nothing */
#define update_marginbell() /* nothing */

#if OPT_LOAD_VTFONTS
extern void update_font_escape(void);
#else
#define update_font_escape() /* nothing */
#endif

#if OPT_ALLOW_XXX_OPS
extern void update_menu_allowColorOps(void);
extern void update_menu_allowFontOps(void);
extern void update_menu_allowMouseOps(void);
extern void update_menu_allowTcapOps(void);
extern void update_menu_allowTitleOps(void);
extern void update_menu_allowWindowOps(void);
#endif

#if OPT_BLINK_CURS
extern void update_cursorblink(void);
#else
#define update_cursorblink() /* nothing */
#endif

extern void update_altscreen(void);
extern void update_titeInhibit(void);

#ifndef NO_ACTIVE_ICON
extern void update_activeicon(void);
#endif /* NO_ACTIVE_ICON */

#if OPT_DEC_CHRSET
extern void update_font_doublesize(void);
#else
#define update_font_doublesize() /* nothing */
#endif

#if OPT_BOX_CHARS
extern void update_font_boxchars(void);
extern void update_font_packed(void);
#else
#define update_font_boxchars() /* nothing */
#define update_font_packed() /* nothing */
#endif

#if OPT_SIXEL_GRAPHICS
extern void update_decsdm(void);
#else
#define update_decsdm() /* nothing */
#endif

#if OPT_GRAPHICS
extern void update_privatecolorregisters(void);
#else
#define update_privatecolorregisters() /* nothing */
#endif

#if OPT_DEC_SOFTFONT
extern void update_font_loadable(void);
#else
#define update_font_loadable() /* nothing */
#endif

#if OPT_RENDERFONT
extern void update_font_renderfont(void);
#else
#define update_font_renderfont() /* nothing */
#endif

#if OPT_WIDE_CHARS
extern void update_font_utf8_mode(void);
extern void update_font_utf8_fonts(void);
extern void update_font_utf8_title(void);
#else
#define update_font_utf8_mode() /* nothing */
#define update_font_utf8_fonts() /* nothing */
#define update_font_utf8_title() /* nothing */
#endif

#if OPT_TEK4014
extern void update_tekshow(void);
extern void update_vttekmode(void);
extern void update_vtshow(void);
extern void set_vthide_sensitivity(void);
extern void set_tekhide_sensitivity(void);
#else
#define update_tekshow() /*nothing*/
#define update_vttekmode() /*nothing*/
#define update_vtshow() /*nothing*/
#define set_vthide_sensitivity() /*nothing*/
#define set_tekhide_sensitivity() /*nothing*/
#endif

#if OPT_DEC_CHRSET || OPT_BOX_CHARS || OPT_DEC_SOFTFONT
extern void update_menu_allowBoldFonts(void);
#else
#define update_menu_allowBoldFonts() /*nothing*/
#endif

/*
 * macros for mapping font size to tekMenu placement
 */
#define FS2MI(n) (n)			/* font_size_to_menu_item */
#define MI2FS(n) (n)			/* menu_item_to_font_size */

#if OPT_TEK4014
extern void set_tekfont_menu_item(int n,int val);
#else
#define set_tekfont_menu_item(n,val) /*nothing*/
#endif

extern void set_menu_font(int val);

/* *INDENT-ON* */

#endif /* included_menu_h */
