/* $XTermId: menu.c,v 1.350 2017/12/29 20:54:09 tom Exp $ */

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
 * Copyright 1989  The Open Group
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
 */

#include <xterm.h>
#include <data.h>
#include <menu.h>
#include <fontutils.h>
#include <xstrings.h>

#include <locale.h>

#include <X11/Xmu/CharSet.h>

#define app_con Xaw_app_con	/* quiet a warning from SimpleMenu.h */

#if defined(HAVE_LIB_XAW)

#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#if OPT_TOOLBAR
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/Form.h>
#endif

#elif defined(HAVE_LIB_XAW3D)

#include <X11/Xaw3d/SimpleMenu.h>
#include <X11/Xaw3d/Box.h>
#include <X11/Xaw3d/SmeBSB.h>
#include <X11/Xaw3d/SmeLine.h>

#if OPT_TOOLBAR
#include <X11/Xaw3d/MenuButton.h>
#include <X11/Xaw3d/Form.h>
#endif

#elif defined(HAVE_LIB_XAW3DXFT)

#include <X11/Xaw3dxft/SimpleMenu.h>
#include <X11/Xaw3dxft/Box.h>
#include <X11/Xaw3dxft/SmeBSB.h>
#include <X11/Xaw3dxft/SmeLine.h>

#if OPT_TOOLBAR
#include <X11/Xaw3dxft/MenuButton.h>
#include <X11/Xaw3dxft/Form.h>
#endif

#elif defined(HAVE_LIB_NEXTAW)

#include <X11/neXtaw/SimpleMenu.h>
#include <X11/neXtaw/Box.h>
#include <X11/neXtaw/SmeBSB.h>
#include <X11/neXtaw/SmeLine.h>

#if OPT_TOOLBAR
#include <X11/neXtaw/MenuButton.h>
#include <X11/neXtaw/Form.h>
#endif

#elif defined(HAVE_LIB_XAWPLUS)

#include <X11/XawPlus/SimpleMenu.h>
#include <X11/XawPlus/Box.h>
#include <X11/XawPlus/SmeBSB.h>
#include <X11/XawPlus/SmeLine.h>

#if OPT_TOOLBAR
#include <X11/XawPlus/MenuButton.h>
#include <X11/XawPlus/Form.h>
#endif

#endif

#undef app_con

#include <stdio.h>
#include <signal.h>

#if OPT_TRACE
#define UpdateCheckbox(func, mn, mi, val) UpdateMenuItem(func, mn, mi, val)
#else
#define UpdateCheckbox(func, mn, mi, val) UpdateMenuItem(mn, mi, val)
#endif

#define ToggleFlag(flag) flag = (Boolean) !flag
/* *INDENT-OFF* */
static void do_8bit_control    PROTO_XT_CALLBACK_ARGS;
static void do_allow132        PROTO_XT_CALLBACK_ARGS;
static void do_allowBoldFonts  PROTO_XT_CALLBACK_ARGS;
static void do_allowsends      PROTO_XT_CALLBACK_ARGS;
static void do_altscreen       PROTO_XT_CALLBACK_ARGS;
static void do_appcursor       PROTO_XT_CALLBACK_ARGS;
static void do_appkeypad       PROTO_XT_CALLBACK_ARGS;
static void do_autolinefeed    PROTO_XT_CALLBACK_ARGS;
static void do_autowrap        PROTO_XT_CALLBACK_ARGS;
static void do_backarrow       PROTO_XT_CALLBACK_ARGS;
static void do_bellIsUrgent    PROTO_XT_CALLBACK_ARGS;
static void do_clearsavedlines PROTO_XT_CALLBACK_ARGS;
static void do_continue        PROTO_XT_CALLBACK_ARGS;
static void do_delete_del      PROTO_XT_CALLBACK_ARGS;
#if OPT_SCREEN_DUMPS
static void do_dump_html       PROTO_XT_CALLBACK_ARGS;
static void do_dump_svg        PROTO_XT_CALLBACK_ARGS;
#endif
static void do_hardreset       PROTO_XT_CALLBACK_ARGS;
static void do_interrupt       PROTO_XT_CALLBACK_ARGS;
static void do_jumpscroll      PROTO_XT_CALLBACK_ARGS;
static void do_keepClipboard   PROTO_XT_CALLBACK_ARGS;
static void do_keepSelection   PROTO_XT_CALLBACK_ARGS;
static void do_kill            PROTO_XT_CALLBACK_ARGS;
static void do_old_fkeys       PROTO_XT_CALLBACK_ARGS;
static void do_poponbell       PROTO_XT_CALLBACK_ARGS;
static void do_print           PROTO_XT_CALLBACK_ARGS;
static void do_print_redir     PROTO_XT_CALLBACK_ARGS;
static void do_quit            PROTO_XT_CALLBACK_ARGS;
static void do_redraw          PROTO_XT_CALLBACK_ARGS;
static void do_reversevideo    PROTO_XT_CALLBACK_ARGS;
static void do_reversewrap     PROTO_XT_CALLBACK_ARGS;
static void do_scrollbar       PROTO_XT_CALLBACK_ARGS;
static void do_scrollkey       PROTO_XT_CALLBACK_ARGS;
static void do_scrollttyoutput PROTO_XT_CALLBACK_ARGS;
static void do_securekbd       PROTO_XT_CALLBACK_ARGS;
static void do_selectClipboard PROTO_XT_CALLBACK_ARGS;
static void do_softreset       PROTO_XT_CALLBACK_ARGS;
static void do_suspend         PROTO_XT_CALLBACK_ARGS;
static void do_terminate       PROTO_XT_CALLBACK_ARGS;
static void do_titeInhibit     PROTO_XT_CALLBACK_ARGS;
static void do_visualbell      PROTO_XT_CALLBACK_ARGS;
static void do_vtfont          PROTO_XT_CALLBACK_ARGS;

#ifdef ALLOWLOGGING
static void do_logging         PROTO_XT_CALLBACK_ARGS;
#endif

#ifndef NO_ACTIVE_ICON
static void do_activeicon      PROTO_XT_CALLBACK_ARGS;
#endif /* NO_ACTIVE_ICON */

#if OPT_ALLOW_XXX_OPS
static void enable_allow_xxx_ops (Bool);
static void do_allowColorOps   PROTO_XT_CALLBACK_ARGS;
static void do_allowFontOps    PROTO_XT_CALLBACK_ARGS;
static void do_allowMouseOps   PROTO_XT_CALLBACK_ARGS;
static void do_allowTcapOps    PROTO_XT_CALLBACK_ARGS;
static void do_allowTitleOps   PROTO_XT_CALLBACK_ARGS;
static void do_allowWindowOps  PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_BLINK_CURS
static void do_cursorblink     PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_BOX_CHARS
static void do_font_boxchars   PROTO_XT_CALLBACK_ARGS;
static void do_font_packed     PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_DEC_CHRSET
static void do_font_doublesize PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_DEC_SOFTFONT
static void do_font_loadable   PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_HP_FUNC_KEYS
static void do_hp_fkeys        PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_MAXIMIZE
static void do_fullscreen      PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_NUM_LOCK
static void do_alt_esc         PROTO_XT_CALLBACK_ARGS;
static void do_num_lock        PROTO_XT_CALLBACK_ARGS;
static void do_meta_esc        PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_PRINT_ON_EXIT
static void do_write_now       PROTO_XT_CALLBACK_ARGS;
static void do_write_error     PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_RENDERFONT
static void do_font_renderfont PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_SCO_FUNC_KEYS
static void do_sco_fkeys       PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_SIXEL_GRAPHICS
static void do_sixelscrolling  PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_GRAPHICS
static void do_privatecolorregisters PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_SUN_FUNC_KEYS
static void do_sun_fkeys       PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_SUNPC_KBD
static void do_sun_kbd         PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_TCAP_FKEYS
static void do_tcap_fkeys      PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_TEK4014
static void do_tekcopy         PROTO_XT_CALLBACK_ARGS;
static void do_tekhide         PROTO_XT_CALLBACK_ARGS;
static void do_tekmode         PROTO_XT_CALLBACK_ARGS;
static void do_tekonoff        PROTO_XT_CALLBACK_ARGS;
static void do_tekpage         PROTO_XT_CALLBACK_ARGS;
static void do_tekreset        PROTO_XT_CALLBACK_ARGS;
static void do_tekshow         PROTO_XT_CALLBACK_ARGS;
static void do_tektext2        PROTO_XT_CALLBACK_ARGS;
static void do_tektext3        PROTO_XT_CALLBACK_ARGS;
static void do_tektextlarge    PROTO_XT_CALLBACK_ARGS;
static void do_tektextsmall    PROTO_XT_CALLBACK_ARGS;
static void do_vthide          PROTO_XT_CALLBACK_ARGS;
static void do_vtmode          PROTO_XT_CALLBACK_ARGS;
static void do_vtonoff         PROTO_XT_CALLBACK_ARGS;
static void do_vtshow          PROTO_XT_CALLBACK_ARGS;
static void handle_tekshow     (Widget gw, Bool allowswitch);
static void handle_vtshow      (Widget gw, Bool allowswitch);
#endif

#if OPT_TOOLBAR
static void do_toolbar         PROTO_XT_CALLBACK_ARGS;
#endif

#if OPT_WIDE_CHARS
static void do_font_utf8_mode  PROTO_XT_CALLBACK_ARGS;
static void do_font_utf8_fonts PROTO_XT_CALLBACK_ARGS;
static void do_font_utf8_title PROTO_XT_CALLBACK_ARGS;
#endif

/*
 * The order of entries MUST match the values given in menu.h
 */
MenuEntry mainMenuEntries[] = {
#if OPT_TOOLBAR
    { "toolbar",	do_toolbar,	NULL },
#endif
#if OPT_MAXIMIZE
    { "fullscreen",	do_fullscreen,	NULL },
#endif
    { "securekbd",	do_securekbd,	NULL },
    { "allowsends",	do_allowsends,	NULL },
    { "redraw",		do_redraw,	NULL },
    { "line1",		NULL,		NULL },
#ifdef ALLOWLOGGING
    { "logging",	do_logging,	NULL },
#endif
#ifdef OPT_PRINT_ON_EXIT
    { "print-immediate", do_write_now,	NULL },
    { "print-on-error",	do_write_error,	NULL },
#endif
    { "print",		do_print,	NULL },
    { "print-redir",	do_print_redir,	NULL },
#if OPT_SCREEN_DUMPS
    { "dump-html",	do_dump_html,	NULL },
    { "dump-svg",	do_dump_svg,	NULL },
#endif
    { "line2",		NULL,		NULL },
    { "8-bit control",	do_8bit_control,NULL },
    { "backarrow key",	do_backarrow,	NULL },
#if OPT_NUM_LOCK
    { "num-lock",	do_num_lock,	NULL },
    { "alt-esc",	do_alt_esc,	NULL },
    { "meta-esc",	do_meta_esc,	NULL },
#endif
    { "delete-is-del",	do_delete_del,	NULL },
    { "oldFunctionKeys",do_old_fkeys,	NULL },
#if OPT_TCAP_FKEYS
    { "tcapFunctionKeys",do_tcap_fkeys,	NULL },
#endif
#if OPT_HP_FUNC_KEYS
    { "hpFunctionKeys",	do_hp_fkeys,	NULL },
#endif
#if OPT_SCO_FUNC_KEYS
    { "scoFunctionKeys",do_sco_fkeys,	NULL },
#endif
#if OPT_SUN_FUNC_KEYS
    { "sunFunctionKeys",do_sun_fkeys,	NULL },
#endif
#if OPT_SUNPC_KBD
    { "sunKeyboard",	do_sun_kbd,	NULL },
#endif
    { "line3",		NULL,		NULL },
    { "suspend",	do_suspend,	NULL },
    { "continue",	do_continue,	NULL },
    { "interrupt",	do_interrupt,	NULL },
    { "hangup",		do_hangup,	NULL },
    { "terminate",	do_terminate,	NULL },
    { "kill",		do_kill,	NULL },
    { "line4",		NULL,		NULL },
    { "quit",		do_quit,	NULL }};

MenuEntry vtMenuEntries[] = {
    { "scrollbar",	do_scrollbar,	NULL },
    { "jumpscroll",	do_jumpscroll,	NULL },
    { "reversevideo",	do_reversevideo, NULL },
    { "autowrap",	do_autowrap,	NULL },
    { "reversewrap",	do_reversewrap, NULL },
    { "autolinefeed",	do_autolinefeed, NULL },
    { "appcursor",	do_appcursor,	NULL },
    { "appkeypad",	do_appkeypad,	NULL },
    { "scrollkey",	do_scrollkey,	NULL },
    { "scrollttyoutput",do_scrollttyoutput, NULL },
    { "allow132",	do_allow132,	NULL },
    { "keepSelection",	do_keepSelection, NULL },
    { "selectToClipboard",do_selectClipboard, NULL },
    { "visualbell",	do_visualbell,	NULL },
    { "bellIsUrgent",	do_bellIsUrgent, NULL },
    { "poponbell",	do_poponbell,	NULL },
#if OPT_BLINK_CURS
    { "cursorblink",	do_cursorblink,	NULL },
#endif
    { "titeInhibit",	do_titeInhibit,	NULL },
#ifndef NO_ACTIVE_ICON
    { "activeicon",	do_activeicon,	NULL },
#endif /* NO_ACTIVE_ICON */
    { "line1",		NULL,		NULL },
    { "softreset",	do_softreset,	NULL },
    { "hardreset",	do_hardreset,	NULL },
    { "clearsavedlines",do_clearsavedlines, NULL },
    { "line2",		NULL,		NULL },
#if OPT_TEK4014
    { "tekshow",	do_tekshow,	NULL },
    { "tekmode",	do_tekmode,	NULL },
    { "vthide",		do_vthide,	NULL },
#endif
    { "altscreen",	do_altscreen,	NULL },
#if OPT_SIXEL_GRAPHICS
    { "sixelScrolling",	do_sixelscrolling,	NULL },
#endif
#if OPT_GRAPHICS
    { "privateColorRegisters", do_privatecolorregisters, NULL },
#endif
    };

MenuEntry fontMenuEntries[] = {
    { "fontdefault",	do_vtfont,	NULL },
    { "font1",		do_vtfont,	NULL },
    { "font2",		do_vtfont,	NULL },
    { "font3",		do_vtfont,	NULL },
    { "font4",		do_vtfont,	NULL },
    { "font5",		do_vtfont,	NULL },
    { "font6",		do_vtfont,	NULL },
    /* this is after the last builtin font; the other entries are special */
    { "fontescape",	do_vtfont,	NULL },
    { "fontsel",	do_vtfont,	NULL },
    /* down to here should match NMENUFONTS in ptyx.h */

#if OPT_DEC_CHRSET || OPT_BOX_CHARS || OPT_DEC_SOFTFONT
    { "line1",		NULL,		NULL },
    { "allow-bold-fonts", do_allowBoldFonts, NULL },
#if OPT_BOX_CHARS
    { "font-linedrawing",do_font_boxchars,NULL },
    { "font-packed",	do_font_packed,NULL },
#endif
#if OPT_DEC_CHRSET
    { "font-doublesize",do_font_doublesize,NULL },
#endif
#if OPT_DEC_SOFTFONT
    { "font-loadable",	do_font_loadable,NULL },
#endif
#endif /* toggles for DEC font extensions */

#if OPT_RENDERFONT || OPT_WIDE_CHARS
    { "line2",		NULL,		NULL },
#if OPT_RENDERFONT
    { "render-font",	do_font_renderfont,NULL },
#endif
#if OPT_WIDE_CHARS
    { "utf8-mode",	do_font_utf8_mode,NULL },
    { "utf8-fonts",	do_font_utf8_fonts,NULL },
    { "utf8-title",	do_font_utf8_title,NULL },
#endif
#endif /* toggles for other font extensions */

#if OPT_ALLOW_XXX_OPS
    { "line3",		NULL,		NULL },
    { "allow-color-ops",do_allowColorOps,NULL },
    { "allow-font-ops",	do_allowFontOps,NULL },
    { "allow-mouse-ops",do_allowMouseOps,NULL },
    { "allow-tcap-ops",	do_allowTcapOps,NULL },
    { "allow-title-ops",do_allowTitleOps,NULL },
    { "allow-window-ops",do_allowWindowOps,NULL },
#endif

    };

#if OPT_TEK4014
MenuEntry tekMenuEntries[] = {
    { "tektextlarge",	do_tektextlarge, NULL },
    { "tektext2",	do_tektext2,	NULL },
    { "tektext3",	do_tektext3,	NULL },
    { "tektextsmall",	do_tektextsmall, NULL },
    { "line1",		NULL,		NULL },
    { "tekpage",	do_tekpage,	NULL },
    { "tekreset",	do_tekreset,	NULL },
    { "tekcopy",	do_tekcopy,	NULL },
    { "line2",		NULL,		NULL },
    { "vtshow",		do_vtshow,	NULL },
    { "vtmode",		do_vtmode,	NULL },
    { "tekhide",	do_tekhide,	NULL }};
#endif

typedef struct {
    char *internal_name;
    MenuEntry *entry_list;
    Cardinal entry_len;
} MenuHeader;

    /* This table is ordered to correspond with MenuIndex */
#define DATA(name) { (char *)#name, name ## Entries, XtNumber(name ## Entries ) }
static const MenuHeader menu_names[] = {
    DATA( mainMenu),
    DATA( vtMenu),
    DATA( fontMenu),
#if OPT_TEK4014
    DATA( tekMenu),
#endif
    { NULL, 0, 0 },
};
#undef DATA
/* *INDENT-ON* */

/*
 * FIXME:  These are global data rather than in the xterm widget because they
 * are initialized before the widget is created.
 */
typedef struct {
    Widget b;			/* the toolbar's buttons */
    Widget w;			/* the popup shell activated by the button */
    Cardinal entries;
} MenuList;

static MenuList vt_shell[NUM_POPUP_MENUS];

#if OPT_TEK4014 && OPT_TOOLBAR
static MenuList tek_shell[NUM_POPUP_MENUS];
#endif

static String
setMenuLocale(Bool before, String substitute)
{
    String result = setlocale(LC_CTYPE, 0);

    if (before) {
	result = x_strdup(result);
    }
    (void) setlocale(LC_CTYPE, substitute);
    TRACE(("setMenuLocale %s:%s\n",
	   (before
	    ? "before"
	    : "after"),
	   NonNull(result)));
    if (!before) {
	free((void *) substitute);
    }
    return result;
}

/*
 * Returns a pointer to the MenuList entry that matches the popup menu.
 */
static MenuList *
select_menu(Widget w GCC_UNUSED, MenuIndex num)
{
#if OPT_TEK4014 && OPT_TOOLBAR
    while (w != 0) {
	if (w == tekshellwidget) {
	    return &tek_shell[num];
	}
	w = XtParent(w);
    }
#endif
    return &vt_shell[num];
}

/*
 * Returns a pointer to the given popup menu shell
 */
static Widget
obtain_menu(Widget w, MenuIndex num)
{
    return select_menu(w, num)->w;
}

/*
 * Returns the number of entries in the given popup menu shell
 */
static Cardinal
sizeof_menu(Widget w, MenuIndex num)
{
    return select_menu(w, num)->entries;
}

/*
 * Return an array of flags telling if a given menu item is never going to
 * be used, so we can reduce the size of menus.
 */
static Boolean *
unusedEntries(XtermWidget xw, MenuIndex num)
{
    static Boolean result[XtNumber(mainMenuEntries)
			  + XtNumber(vtMenuEntries)
			  + XtNumber(fontMenuEntries)
#if OPT_TEK4014
			  + XtNumber(tekMenuEntries)
#endif
    ];
    TScreen *screen = TScreenOf(xw);

    memset(result, 0, sizeof(result));
    switch (num) {
    case mainMenu:
#if OPT_MAXIMIZE
	if (resource.fullscreen > 1) {
	    result[mainMenu_fullscreen] = True;
	}
#endif
#if OPT_NUM_LOCK
	if (!screen->alt_is_not_meta) {
	    result[mainMenu_alt_esc] = True;
	}
#endif
	if (!xtermHasPrinter(xw)) {
	    result[mainMenu_print] = True;
	    result[mainMenu_print_redir] = True;
	}
	if (screen->terminal_id < 200) {
	    result[mainMenu_8bit_ctrl] = True;
	}
#if !defined(SIGTSTP)
	result[mainMenu_suspend] = True;
#endif
#if !defined(SIGCONT)
	result[mainMenu_continue] = True;
#endif
#ifdef ALLOWLOGGING
	if (screen->inhibit & I_LOG) {
	    result[mainMenu_logging] = True;
	}
#endif
	if (screen->inhibit & I_SIGNAL) {
	    int n;
	    for (n = (int) mainMenu_suspend; n <= (int) mainMenu_quit; ++n) {
		result[n] = True;
	    }
	}
	break;
    case vtMenu:
#ifndef NO_ACTIVE_ICON
	if (!getIconicFont(screen)->fs || !screen->iconVwin.window) {
	    result[vtMenu_activeicon] = True;
	}
#endif /* NO_ACTIVE_ICON */
#if OPT_TEK4014
	if (screen->inhibit & I_TEK) {
	    int n;
	    for (n = (int) vtMenu_tekshow; n <= (int) vtMenu_vthide; ++n) {
		result[n] = True;
	    }
	}
#endif
	break;
    case fontMenu:
	break;
#if OPT_TEK4014
    case tekMenu:
	break;
#endif
    case noMenu:
	break;
    }
    return result;
}

/*
 * create_menu - create a popup shell and stuff the menu into it.
 */
static Widget
create_menu(Widget w, XtermWidget xw, MenuIndex num)
{
    static XtCallbackRec cb[2] =
    {
	{NULL, NULL},
	{NULL, NULL}};
    static Arg arg =
    {XtNcallback, (XtArgVal) cb};

    TScreen *screen = TScreenOf(xw);
    const MenuHeader *data = &menu_names[num];
    MenuList *list = select_menu(w, num);
    struct _MenuEntry *entries = data->entry_list;
    Cardinal nentries = data->entry_len;
#if !OPT_TOOLBAR
    String saveLocale;
#endif

    if (screen->menu_item_bitmap == None) {
	/*
	 * we really want to do these dynamically
	 */
#define check_width 9
#define check_height 8
	static unsigned char check_bits[] =
	{
	    0x00, 0x01, 0x80, 0x01, 0xc0, 0x00, 0x60, 0x00,
	    0x31, 0x00, 0x1b, 0x00, 0x0e, 0x00, 0x04, 0x00
	};

	screen->menu_item_bitmap =
	    XCreateBitmapFromData(XtDisplay(xw),
				  RootWindowOfScreen(XtScreen(xw)),
				  (char *) check_bits, check_width, check_height);
    }
#if !OPT_TOOLBAR
    saveLocale = setMenuLocale(True, resource.menuLocale);
    list->w = XtCreatePopupShell(data->internal_name,
				 simpleMenuWidgetClass,
				 toplevel,
				 NULL, 0);
#endif
    if (list->w != 0) {
	Boolean *unused = unusedEntries(xw, num);
	Cardinal n;

	list->entries = 0;

	for (n = 0; n < nentries; ++n) {
	    if (!unused[n]) {
		cb[0].callback = (XtCallbackProc) entries[n].function;
		cb[0].closure = (XtPointer) entries[n].name;
		entries[n].widget = XtCreateManagedWidget(entries[n].name,
							  (entries[n].function
							   ? smeBSBObjectClass
							   : smeLineObjectClass),
							  list->w,
							  &arg, (Cardinal) 1);
		list->entries++;
	    }
	}
    }
#if !OPT_TOOLBAR
    (void) setMenuLocale(False, saveLocale);
#endif

    /* do not realize at this point */
    return list->w;
}

static MenuIndex
indexOfMenu(String menuName)
{
    MenuIndex me;
    switch (*menuName) {
    case 'm':
	me = mainMenu;
	break;
    case 'v':
	me = vtMenu;
	break;
    case 'f':
	me = fontMenu;
	break;
#if OPT_TEK4014
    case 't':
	me = tekMenu;
	break;
#endif
    default:
	me = noMenu;
    }
    return (me);
}

/* ARGSUSED */
static Bool
domenu(Widget w,
       XEvent *event GCC_UNUSED,
       String *params,		/* mainMenu, vtMenu, or tekMenu */
       Cardinal *param_count)	/* 0 or 1 */
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    MenuIndex me;
    Bool created = False;
    Widget mw;

    if (*param_count != 1) {
	Bell(xw, XkbBI_MinorError, 0);
	return False;
    }

    if ((me = indexOfMenu(params[0])) == noMenu) {
	Bell(xw, XkbBI_MinorError, 0);
	return False;
    }

    if ((mw = obtain_menu(w, me)) == 0
	|| sizeof_menu(w, me) == 0) {
	mw = create_menu(w, xw, me);
	created = (mw != 0);
    }
    if (mw == 0)
	return False;

    TRACE(("domenu(%s) %s\n", params[0], created ? "create" : "update"));
    switch (me) {
    case mainMenu:
	if (created) {
	    update_toolbar();
	    update_fullscreen();
	    update_securekbd();
	    update_allowsends();
	    update_logging();
	    update_print_redir();
	    update_8bit_control();
	    update_decbkm();
	    update_num_lock();
	    update_alt_esc();
	    update_meta_esc();
	    update_delete_del();
	    update_keyboard_type();
#ifdef OPT_PRINT_ON_EXIT
	    screen->write_error = !IsEmpty(resource.printFileOnXError);
	    SetItemSensitivity(mainMenuEntries[mainMenu_write_now].widget, True);
	    SetItemSensitivity(mainMenuEntries[mainMenu_write_error].widget, screen->write_error);
#endif
	}
	break;

    case vtMenu:
	if (created) {
	    update_scrollbar();
	    update_jumpscroll();
	    update_reversevideo();
	    update_autowrap();
	    update_reversewrap();
	    update_autolinefeed();
	    update_appcursor();
	    update_appkeypad();
	    update_scrollkey();
	    update_scrollttyoutput();
	    update_allow132();
	    update_cursesemul();
	    update_keepSelection();
	    update_selectToClipboard();
	    update_visualbell();
	    update_poponbell();
	    update_bellIsUrgent();
	    update_cursorblink();
	    update_altscreen();
	    update_decsdm();	/* Sixel Display Mode */
	    update_titeInhibit();
#ifndef NO_ACTIVE_ICON
	    update_activeicon();
#endif /* NO_ACTIVE_ICON */
	    update_privatecolorregisters();
	}
	break;

    case fontMenu:
	if (created) {
	    int n;

	    set_menu_font(True);
	    for (n = fontMenu_font1; n <= fontMenu_font6; ++n) {
		if (IsEmpty(screen->menu_font_names[n][fNorm]))
		    SetItemSensitivity(fontMenuEntries[n].widget, False);
	    }
	    update_font_escape();
	    update_menu_allowBoldFonts();
#if OPT_BOX_CHARS
	    update_font_boxchars();
	    update_font_packed();
	    SetItemSensitivity(
				  fontMenuEntries[fontMenu_font_packedfont].widget,
				  True);
#endif
#if OPT_DEC_SOFTFONT		/* FIXME: not implemented */
	    update_font_loadable();
	    SetItemSensitivity(
				  fontMenuEntries[fontMenu_font_loadable].widget,
				  False);
#endif
#if OPT_DEC_CHRSET
	    update_font_doublesize();
	    if (TScreenOf(xw)->cache_doublesize == 0)
		SetItemSensitivity(
				      fontMenuEntries[fontMenu_font_doublesize].widget,
				      False);
#endif
#if OPT_RENDERFONT
	    update_font_renderfont();
#endif
#if OPT_WIDE_CHARS
	    update_font_utf8_mode();
	    update_font_utf8_fonts();
	    update_font_utf8_title();
#endif
#if OPT_ALLOW_XXX_OPS
	    update_menu_allowColorOps();
	    update_menu_allowFontOps();
	    update_menu_allowMouseOps();
	    update_menu_allowTcapOps();
	    update_menu_allowTitleOps();
	    update_menu_allowWindowOps();
	    enable_allow_xxx_ops(!(screen->allowSendEvents));
#endif
	}
#if OPT_TOOLBAR
	/* menus for toolbar are initialized once only */
	SetItemSensitivity(fontMenuEntries[fontMenu_fontsel].widget, True);
#else
	FindFontSelection(xw, NULL, True);
	SetItemSensitivity(fontMenuEntries[fontMenu_fontsel].widget,
			   (screen->SelectFontName()
			    ? True
			    : False));
#endif
	break;

#if OPT_TEK4014
    case tekMenu:
	if (created && tekWidget) {
	    set_tekfont_menu_item(TekScreenOf(tekWidget)->cur.fontsize, True);
	    update_vtshow();
	}
	break;
#endif
    case noMenu:
    default:
	break;
    }

    return True;
}

/*
 * public interfaces
 */

void
HandleCreateMenu(Widget w,
		 XEvent *event,
		 String *params,	/* mainMenu, vtMenu, or tekMenu */
		 Cardinal *param_count)		/* 0 or 1 */
{
    TRACE(("HandleCreateMenu\n"));
    (void) domenu(w, event, params, param_count);
}

void
HandlePopupMenu(Widget w,
		XEvent *event,
		String *params,	/* mainMenu, vtMenu, or tekMenu */
		Cardinal *param_count)	/* 0 or 1 */
{
    TRACE(("HandlePopupMenu\n"));
    if (domenu(w, event, params, param_count)) {
	XtermWidget xw = term;
	TScreen *screen = TScreenOf(xw);

#if OPT_TOOLBAR
	w = select_menu(w, mainMenu)->w;
#endif
	/*
	 * The action procedure in SimpleMenu.c, PositionMenu does not expect a
	 * key translation event when we are popping up a menu.  In particular,
	 * if the pointer is outside the menu, then the action procedure will
	 * fail in its attempt to determine the location of the pointer within
	 * the menu.  Anticipate that by warping the pointer into the menu when
	 * a key event is detected.
	 */
	switch (event->type) {
	case KeyPress:
	case KeyRelease:
	    XWarpPointer(screen->display, None, XtWindow(w), 0, 0, 0, 0, 0, 0);
	    break;
	default:
	    XtCallActionProc(w, "XawPositionSimpleMenu", event, params, 1);
	    break;
	}
	XtCallActionProc(w, "MenuPopup", event, params, 1);
    }
}

/*
 * private interfaces - keep out!
 */

/* ARGSUSED */
static void
handle_send_signal(Widget gw GCC_UNUSED, int sig)
{
#ifndef VMS
    TScreen *screen = TScreenOf(term);

    if (hold_screen > 1)
	hold_screen = 0;
    if (screen->pid > 1)
	kill_process_group(screen->pid, sig);
#endif
}

static void
UpdateMenuItem(
#if OPT_TRACE
		  const char *func,
#endif
		  MenuEntry * menu,
		  int which,
		  Bool val)
{
    static Arg menuArgs =
    {XtNleftBitmap, (XtArgVal) 0};
    Widget mi = menu[which].widget;

    if (mi) {
	menuArgs.value = (XtArgVal) ((val)
				     ? TScreenOf(term)->menu_item_bitmap
				     : None);
	XtSetValues(mi, &menuArgs, (Cardinal) 1);
    }
    TRACE(("%s(%d): %s\n", func, which, MtoS(val)));
}

void
SetItemSensitivity(Widget mi, Bool val)
{
    static Arg menuArgs =
    {XtNsensitive, (XtArgVal) 0};

    if (mi) {
	menuArgs.value = (XtArgVal) (val);
	XtSetValues(mi, &menuArgs, (Cardinal) 1);
    }
}

/*
 * action routines
 */

static void
do_securekbd(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    Time now = CurrentTime;	/* XXX - wrong */

    if (screen->grabbedKbd) {
	XUngrabKeyboard(screen->display, now);
	ReverseVideo(xw);
	screen->grabbedKbd = False;
    } else {
	if (XGrabKeyboard(screen->display, XtWindow(CURRENT_EMU()),
			  True, GrabModeAsync, GrabModeAsync, now)
	    != GrabSuccess) {
	    Bell(xw, XkbBI_MinorError, 100);
	} else {
	    ReverseVideo(xw);
	    screen->grabbedKbd = True;
	}
    }
    update_securekbd();
}

/* ARGSUSED */
void
HandleSecure(Widget w GCC_UNUSED,
	     XEvent *event GCC_UNUSED,	/* unused */
	     String *params GCC_UNUSED,		/* [0] = volume */
	     Cardinal *param_count GCC_UNUSED)	/* 0 or 1 */
{
    do_securekbd(vt_shell[mainMenu].w, (XtPointer) 0, (XtPointer) 0);
}

static void
do_allowsends(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->allowSendEvents);
    update_allowsends();
#if OPT_ALLOW_XXX_OPS
    enable_allow_xxx_ops(!(screen->allowSendEvents));
#endif
}

static void
do_visualbell(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->visualbell);
    update_visualbell();
}

static void
do_bellIsUrgent(Widget gw GCC_UNUSED,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->bellIsUrgent);
    update_bellIsUrgent();
}

static void
do_poponbell(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->poponbell);
    update_poponbell();
}

#ifdef ALLOWLOGGING
static void
do_logging(Widget gw GCC_UNUSED,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    if (screen->logging) {
	CloseLog(xw);
    } else {
	StartLog(xw);
    }
    /* update_logging done by CloseLog and StartLog */
}
#endif

#ifdef OPT_PRINT_ON_EXIT
static void
do_write_now(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;

    xtermPrintImmediately(xw,
			  (IsEmpty(resource.printFileNow)
			   ? (String) "XTerm"
			   : resource.printFileNow),
			  resource.printOptsNow,
			  resource.printModeNow);
}

static void
do_write_error(Widget gw GCC_UNUSED,
	       XtPointer closure GCC_UNUSED,
	       XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;

    if (IsEmpty(resource.printFileOnXError)) {
	resource.printFileOnXError = "XTermError";
    }
    TScreenOf(xw)->write_error = (Boolean) (!TScreenOf(xw)->write_error);
    update_write_error();
}
#endif

static void
do_print(Widget gw GCC_UNUSED,
	 XtPointer closure GCC_UNUSED,
	 XtPointer data GCC_UNUSED)
{
    xtermPrintScreen(term, True, getPrinterFlags(term, NULL, 0));
}

static void
do_print_redir(Widget gw GCC_UNUSED,
	       XtPointer closure GCC_UNUSED,
	       XtPointer data GCC_UNUSED)
{
    setPrinterControlMode(term,
			  (PrinterOf(TScreenOf(term)).printer_controlmode
			   ? 0
			   : 2));
}

#if OPT_SCREEN_DUMPS
static void
do_dump_html(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    xtermDumpHtml(term);
}

static void
do_dump_svg(Widget gw GCC_UNUSED,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    xtermDumpSvg(term);
}
#endif

static void
do_redraw(Widget gw GCC_UNUSED,
	  XtPointer closure GCC_UNUSED,
	  XtPointer data GCC_UNUSED)
{
    Redraw();
}

void
show_8bit_control(Bool value)
{
    if (TScreenOf(term)->control_eight_bits != value) {
	TScreenOf(term)->control_eight_bits = (Boolean) value;
	update_8bit_control();
    }
}

static void
do_8bit_control(Widget gw GCC_UNUSED,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    show_8bit_control(!TScreenOf(term)->control_eight_bits);
}

static void
do_backarrow(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    term->keyboard.flags ^= MODE_DECBKM;
    update_decbkm();
}

#if OPT_NUM_LOCK
static void
do_num_lock(Widget gw GCC_UNUSED,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    ToggleFlag(term->misc.real_NumLock);
    update_num_lock();
}

static void
do_alt_esc(Widget gw GCC_UNUSED,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    ToggleFlag(TScreenOf(term)->alt_sends_esc);
    update_alt_esc();
}

static void
do_meta_esc(Widget gw GCC_UNUSED,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    ToggleFlag(TScreenOf(term)->meta_sends_esc);
    update_meta_esc();
}
#endif

static void
do_delete_del(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    if (xtermDeleteIsDEL(term))
	TScreenOf(term)->delete_is_del = False;
    else
	TScreenOf(term)->delete_is_del = True;
    update_delete_del();
}

static void
do_old_fkeys(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    toggle_keyboard_type(term, keyboardIsLegacy);
}

#if OPT_HP_FUNC_KEYS
static void
do_hp_fkeys(Widget gw GCC_UNUSED,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    toggle_keyboard_type(term, keyboardIsHP);
}
#endif

#if OPT_SCO_FUNC_KEYS
static void
do_sco_fkeys(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    toggle_keyboard_type(term, keyboardIsSCO);
}
#endif

#if OPT_SUN_FUNC_KEYS
static void
do_sun_fkeys(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    toggle_keyboard_type(term, keyboardIsSun);
}
#endif

#if OPT_SUNPC_KBD
/*
 * This really means "Sun/PC keyboard emulating VT220".
 */
static void
do_sun_kbd(Widget gw GCC_UNUSED,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    toggle_keyboard_type(term, keyboardIsVT220);
}
#endif

#if OPT_TCAP_FKEYS
static void
do_tcap_fkeys(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    toggle_keyboard_type(term, keyboardIsTermcap);
}
#endif

/*
 * The following cases use the pid instead of the process group so that we
 * don't get hosed by programs that change their process group
 */

/* ARGSUSED */
static void
do_suspend(Widget gw,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
#if defined(SIGTSTP)
    handle_send_signal(gw, SIGTSTP);
#endif
}

/* ARGSUSED */
static void
do_continue(Widget gw,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
#if defined(SIGCONT)
    handle_send_signal(gw, SIGCONT);
#endif
}

/* ARGSUSED */
static void
do_interrupt(Widget gw,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    handle_send_signal(gw, SIGINT);
}

/* ARGSUSED */
void
do_hangup(Widget gw,
	  XtPointer closure GCC_UNUSED,
	  XtPointer data GCC_UNUSED)
{
    handle_send_signal(gw, SIGHUP);
}

/* ARGSUSED */
static void
do_terminate(Widget gw,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    handle_send_signal(gw, SIGTERM);
}

/* ARGSUSED */
static void
do_kill(Widget gw,
	XtPointer closure GCC_UNUSED,
	XtPointer data GCC_UNUSED)
{
    handle_send_signal(gw, SIGKILL);
}

static void
do_quit(Widget gw GCC_UNUSED,
	XtPointer closure GCC_UNUSED,
	XtPointer data GCC_UNUSED)
{
    Cleanup(SIGHUP);
}

/*
 * vt menu callbacks
 */

static void
do_scrollbar(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    ToggleScrollBar(term);
}

static void
do_jumpscroll(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    term->flags ^= SMOOTHSCROLL;
    if (term->flags & SMOOTHSCROLL) {
	screen->jumpscroll = False;
	if (screen->scroll_amt)
	    FlushScroll(term);
    } else {
	screen->jumpscroll = True;
    }
    update_jumpscroll();
}

static void
do_reversevideo(Widget gw GCC_UNUSED,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    ReverseVideo(term);
}

static void
do_autowrap(Widget gw GCC_UNUSED,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    term->flags ^= WRAPAROUND;
    update_autowrap();
}

static void
do_reversewrap(Widget gw GCC_UNUSED,
	       XtPointer closure GCC_UNUSED,
	       XtPointer data GCC_UNUSED)
{
    term->flags ^= REVERSEWRAP;
    update_reversewrap();
}

static void
do_autolinefeed(Widget gw GCC_UNUSED,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    term->flags ^= LINEFEED;
    update_autolinefeed();
}

static void
do_appcursor(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    term->keyboard.flags ^= MODE_DECCKM;
    update_appcursor();
}

static void
do_appkeypad(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    term->keyboard.flags ^= MODE_DECKPAM;
    update_appkeypad();
}

static void
do_scrollkey(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->scrollkey);
    update_scrollkey();
}

static void
do_scrollttyoutput(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->scrollttyoutput);
    update_scrollttyoutput();
}

static void
do_keepClipboard(Widget gw GCC_UNUSED,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->keepClipboard);
    update_keepClipboard();
}

static void
do_keepSelection(Widget gw GCC_UNUSED,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->keepSelection);
    update_keepSelection();
}

static void
do_selectClipboard(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->selectToClipboard);
    update_selectToClipboard();
}

static void
do_allow132(Widget gw GCC_UNUSED,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->c132);
    update_allow132();
}

static void
do_cursesemul(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->curses);
    update_cursesemul();
}

static void
do_marginbell(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    if ((ToggleFlag(screen->marginbell)) == 0)
	screen->bellArmed = -1;
    update_marginbell();
}

#if OPT_TEK4014
static void
handle_tekshow(Widget gw GCC_UNUSED, Bool allowswitch)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    TRACE(("Show tek-window\n"));
    if (!TEK4014_SHOWN(xw)) {	/* not showing, turn on */
	set_tek_visibility(True);
    } else if (screen->Vshow || allowswitch) {	/* is showing, turn off */
	set_tek_visibility(False);
	end_tek_mode();		/* WARNING: this does a longjmp */
    } else
	Bell(xw, XkbBI_MinorError, 0);
}

/* ARGSUSED */
static void
do_tekshow(Widget gw,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    handle_tekshow(gw, True);
}

/* ARGSUSED */
static void
do_tekonoff(Widget gw,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    handle_tekshow(gw, False);
}
#endif /* OPT_TEK4014 */

#if OPT_BLINK_CURS
/* ARGSUSED */
static void
do_cursorblink(Widget gw GCC_UNUSED,
	       XtPointer closure GCC_UNUSED,
	       XtPointer data GCC_UNUSED)
{
    ToggleCursorBlink(TScreenOf(term));
}
#endif

/* ARGSUSED */
static void
do_altscreen(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    ToggleAlternate(term);
}

/* ARGSUSED */
static void
do_titeInhibit(Widget gw GCC_UNUSED,
	       XtPointer closure GCC_UNUSED,
	       XtPointer data GCC_UNUSED)
{
    ToggleFlag(term->misc.titeInhibit);
    update_titeInhibit();
}

#ifndef NO_ACTIVE_ICON
/* ARGSUSED */
static void
do_activeicon(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    if (screen->iconVwin.window) {
	Widget shell = XtParent(term);
	ToggleFlag(term->work.active_icon);
	XtVaSetValues(shell, XtNiconWindow,
		      term->work.active_icon ? screen->iconVwin.window : None,
		      (XtPointer) 0);
	update_activeicon();
    }
}
#endif /* NO_ACTIVE_ICON */

static void
do_softreset(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    VTReset(term, False, False);
}

static void
do_hardreset(Widget gw GCC_UNUSED,
	     XtPointer closure GCC_UNUSED,
	     XtPointer data GCC_UNUSED)
{
    VTReset(term, True, False);
}

static void
do_clearsavedlines(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    VTReset(term, True, True);
}

#if OPT_TEK4014
static void
do_tekmode(Widget gw GCC_UNUSED,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    switch_modes(TEK4014_ACTIVE(term));		/* switch to tek mode */
}

/* ARGSUSED */
static void
do_vthide(Widget gw GCC_UNUSED,
	  XtPointer closure GCC_UNUSED,
	  XtPointer data GCC_UNUSED)
{
    hide_vt_window();
}
#endif /* OPT_TEK4014 */

/*
 * vtfont menu
 */

static void
do_vtfont(Widget gw GCC_UNUSED,
	  XtPointer closure,
	  XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;
    char *entryname = (char *) closure;
    int i;

    TRACE(("do_vtfont(%s)\n", entryname));
    for (i = 0; i < NMENUFONTS; i++) {
	if (strcmp(entryname, fontMenuEntries[i].name) == 0) {
	    SetVTFont(xw, i, True, NULL);
	    return;
	}
    }
    Bell(xw, XkbBI_MinorError, 0);
}

#if OPT_DEC_CHRSET
static void
do_font_doublesize(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;

    if (TScreenOf(xw)->cache_doublesize != 0)
	ToggleFlag(TScreenOf(xw)->font_doublesize);
    update_font_doublesize();
    Redraw();
}
#endif

#if OPT_BOX_CHARS
static void
do_font_boxchars(Widget gw GCC_UNUSED,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    ToggleFlag(TScreenOf(term)->force_box_chars);
    update_font_boxchars();
    Redraw();
}

static void
do_font_packed(Widget gw GCC_UNUSED,
	       XtPointer closure GCC_UNUSED,
	       XtPointer data GCC_UNUSED)
{
    ToggleFlag(TScreenOf(term)->force_packed);
    update_font_packed();
    SetVTFont(term, TScreenOf(term)->menu_font_number, True, NULL);
}
#endif

#if OPT_DEC_SOFTFONT
static void
do_font_loadable(Widget gw GCC_UNUSED,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    ToggleFlag(term->misc.font_loadable);
    update_font_loadable();
}
#endif

#if OPT_RENDERFONT
static void
do_font_renderfont(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    XtermWidget xw = (XtermWidget) term;
    TScreen *screen = TScreenOf(xw);
    int fontnum = screen->menu_font_number;
    String name = TScreenOf(xw)->MenuFontName(fontnum);

    DefaultRenderFont(xw);
    ToggleFlag(xw->work.render_font);
    update_font_renderfont();
    xtermLoadFont(xw, xtermFontName(name), True, fontnum);
    ScrnRefresh(term, 0, 0,
		MaxRows(screen),
		MaxCols(screen), True);
}
#endif

#if OPT_WIDE_CHARS
static void
setup_wide_fonts(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->wide_chars) {
	if (xtermLoadWideFonts(xw, True)) {
	    SetVTFont(xw, screen->menu_font_number, True, NULL);
	}
    } else {
	ChangeToWide(xw);
    }
}

static void
setup_narrow_fonts(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (xtermLoadDefaultFonts(xw)) {
	SetVTFont(xw, screen->menu_font_number, True, NULL);
    }
}

static void
do_font_utf8_mode(Widget gw GCC_UNUSED,
		  XtPointer closure GCC_UNUSED,
		  XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    /*
     * If xterm was started with -wc option, it might not have the wide fonts.
     * If xterm was not started with -wc, it might not have wide cells.
     */
    if (!screen->utf8_mode) {
	setup_wide_fonts(xw);
    }
    switchPtyData(screen, !screen->utf8_mode);
    /*
     * We don't repaint the screen when switching UTF-8 on/off.  When switching
     * on - the Latin-1 codes should paint as-is.  When switching off, that's
     * hard to do properly.
     */
}

static void
do_font_utf8_fonts(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    ToggleFlag(screen->utf8_fonts);
    update_font_utf8_fonts();

    if (screen->utf8_fonts) {
	setup_wide_fonts(xw);
    } else {
	setup_narrow_fonts(xw);
    }
}

static void
do_font_utf8_title(Widget gw GCC_UNUSED,
		   XtPointer closure GCC_UNUSED,
		   XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->utf8_title);
    update_font_utf8_title();
}
#endif

/*
 * tek menu
 */

#if OPT_TEK4014
static void
do_tektextlarge(Widget gw,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    TekSetFontSize(getTekWidget(gw), True, tekMenu_tektextlarge);
}

static void
do_tektext2(Widget gw,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    TekSetFontSize(getTekWidget(gw), True, tekMenu_tektext2);
}

static void
do_tektext3(Widget gw,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    TekSetFontSize(getTekWidget(gw), True, tekMenu_tektext3);
}

static void
do_tektextsmall(Widget gw,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    TekSetFontSize(getTekWidget(gw), True, tekMenu_tektextsmall);
}

static void
do_tekpage(Widget gw,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    TekSimulatePageButton(getTekWidget(gw), False);
}

static void
do_tekreset(Widget gw,
	    XtPointer closure GCC_UNUSED,
	    XtPointer data GCC_UNUSED)
{
    TekSimulatePageButton(getTekWidget(gw), True);
}

static void
do_tekcopy(Widget gw,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    TekCopy(getTekWidget(gw));
}

static void
handle_vtshow(Widget gw GCC_UNUSED, Bool allowswitch)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    TRACE(("Show vt-window\n"));
    if (!screen->Vshow) {	/* not showing, turn on */
	set_vt_visibility(True);
    } else if (TEK4014_SHOWN(xw) || allowswitch) {	/* is showing, turn off */
	set_vt_visibility(False);
	if (!TEK4014_ACTIVE(xw) && tekRefreshList)
	    TekRefresh(tekWidget);
	end_vt_mode();		/* WARNING: this does a longjmp... */
    } else
	Bell(xw, XkbBI_MinorError, 0);
}

static void
do_vtshow(Widget gw,
	  XtPointer closure GCC_UNUSED,
	  XtPointer data GCC_UNUSED)
{
    handle_vtshow(gw, True);
}

static void
do_vtonoff(Widget gw,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    handle_vtshow(gw, False);
}

static void
do_vtmode(Widget gw GCC_UNUSED,
	  XtPointer closure GCC_UNUSED,
	  XtPointer data GCC_UNUSED)
{
    switch_modes(TEK4014_ACTIVE(term));		/* switch to vt, or from */
}

/* ARGSUSED */
static void
do_tekhide(Widget gw GCC_UNUSED,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    hide_tek_window();
}
#endif /* OPT_TEK4014 */

/*
 * public handler routines
 */
int
decodeToggle(XtermWidget xw, String *params, Cardinal nparams)
{
    int dir = toggleErr;

    switch (nparams) {
    case 0:
	dir = toggleAll;
	break;
    case 1:
	if (XmuCompareISOLatin1(params[0], "on") == 0)
	    dir = toggleOn;
	else if (XmuCompareISOLatin1(params[0], "off") == 0)
	    dir = toggleOff;
	else if (XmuCompareISOLatin1(params[0], "toggle") == 0)
	    dir = toggleAll;
	break;
    }

    if (dir == toggleErr) {
	Bell(xw, XkbBI_MinorError, 0);
    }

    return dir;
}

static void
handle_toggle(void (*proc) PROTO_XT_CALLBACK_ARGS,
	      int var,
	      String *params,
	      Cardinal nparams,
	      Widget w,
	      XtPointer closure,
	      XtPointer data)
{
    XtermWidget xw = term;

    switch (decodeToggle(xw, params, nparams)) {

    case toggleAll:
	(*proc) (w, closure, data);
	break;

    case toggleOff:
	if (var)
	    (*proc) (w, closure, data);
	else
	    Bell(xw, XkbBI_MinorError, 0);
	break;

    case toggleOn:
	if (!var)
	    (*proc) (w, closure, data);
	else
	    Bell(xw, XkbBI_MinorError, 0);
	break;
    }
    return;
}

#define handle_vt_toggle(proc, var, params, nparams, w) \
	handle_toggle(proc, (int) (var), params, nparams, w, (XtPointer)0, (XtPointer)0)

#define HANDLE_VT_TOGGLE(name) \
	handle_vt_toggle(do_##name, TScreenOf(term)->name, params, *param_count, w)

#define handle_tek_toggle(proc, var, params, nparams, w) \
	handle_toggle(proc, (int) (var), params, nparams, w, (XtPointer)0, (XtPointer)0)

void
HandleAllowSends(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    handle_vt_toggle(do_allowsends, TScreenOf(term)->allowSendEvents,
		     params, *param_count, w);
}

void
HandleSetVisualBell(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(visualbell);
}

void
HandleSetPopOnBell(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(poponbell);
}

#ifdef ALLOWLOGGING
void
HandleLogging(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params,
	      Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(logging);
}
#endif

#if OPT_PRINT_ON_EXIT
void
HandleWriteNow(Widget w,
	       XEvent *event GCC_UNUSED,
	       String *params GCC_UNUSED,
	       Cardinal *param_count GCC_UNUSED)
{
    do_write_now(w, 0, 0);
}

void
HandleWriteError(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(write_error);
}
#endif

/* ARGSUSED */
void
HandlePrintScreen(Widget w GCC_UNUSED,
		  XEvent *event GCC_UNUSED,
		  String *params GCC_UNUSED,
		  Cardinal *param_count GCC_UNUSED)
{
    xtermPrintScreen(term, True, getPrinterFlags(term, params, param_count));
}

/* ARGSUSED */
void
HandlePrintEverything(Widget w GCC_UNUSED,
		      XEvent *event GCC_UNUSED,
		      String *params,
		      Cardinal *param_count)
{
    xtermPrintEverything(term, getPrinterFlags(term, params, param_count));
}

/* ARGSUSED */
void
HandlePrintControlMode(Widget w,
		       XEvent *event GCC_UNUSED,
		       String *params GCC_UNUSED,
		       Cardinal *param_count GCC_UNUSED)
{
    do_print_redir(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
void
HandleRedraw(Widget w,
	     XEvent *event GCC_UNUSED,
	     String *params GCC_UNUSED,
	     Cardinal *param_count GCC_UNUSED)
{
    do_redraw(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
void
HandleSendSignal(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    /* *INDENT-OFF* */
    static const struct sigtab {
	const char *name;
	int sig;
    } signals[] = {
#ifdef SIGTSTP
	{ "suspend",	SIGTSTP },
	{ "tstp",	SIGTSTP },
#endif
#ifdef SIGCONT
	{ "cont",	SIGCONT },
#endif
	{ "int",	SIGINT },
	{ "hup",	SIGHUP },
	{ "quit",	SIGQUIT },
	{ "alrm",	SIGALRM },
	{ "alarm",	SIGALRM },
	{ "term",	SIGTERM },
	{ "kill",	SIGKILL },
	{ NULL, 0 },
    };
    /* *INDENT-ON* */

    if (*param_count == 1) {
	const struct sigtab *st;

	for (st = signals; st->name; st++) {
	    if (XmuCompareISOLatin1(st->name, params[0]) == 0) {
		handle_send_signal(w, st->sig);
		return;
	    }
	}
	/* one could allow numeric values, but that would be a security hole */
    }

    Bell(term, XkbBI_MinorError, 0);
}

/* ARGSUSED */
void
HandleQuit(Widget w,
	   XEvent *event GCC_UNUSED,
	   String *params GCC_UNUSED,
	   Cardinal *param_count GCC_UNUSED)
{
    do_quit(w, (XtPointer) 0, (XtPointer) 0);
}

void
Handle8BitControl(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    handle_vt_toggle(do_8bit_control, TScreenOf(term)->control_eight_bits,
		     params, *param_count, w);
}

void
HandleBackarrow(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    handle_vt_toggle(do_backarrow, term->keyboard.flags & MODE_DECBKM,
		     params, *param_count, w);
}

#if OPT_MAXIMIZE
#if OPT_TEK4014
#define WhichEWMH (TEK4014_ACTIVE(xw) != 0)
#else
#define WhichEWMH 0
#endif
static void
do_fullscreen(Widget gw GCC_UNUSED,
	      XtPointer closure GCC_UNUSED,
	      XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;

    if (resource.fullscreen != esNever)
	FullScreen(xw, !xw->work.ewmh[WhichEWMH].mode);
}

/* ARGSUSED */
void
HandleFullscreen(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params GCC_UNUSED,
		 Cardinal *param_count GCC_UNUSED)
{
    XtermWidget xw = term;

    if (resource.fullscreen != esNever) {
	handle_vt_toggle(do_fullscreen, xw->work.ewmh[WhichEWMH].mode,
			 params, *param_count, w);
    }
}

void
update_fullscreen(void)
{
    XtermWidget xw = term;

    if (resource.fullscreen <= 1) {
	UpdateCheckbox("update_fullscreen",
		       mainMenuEntries,
		       mainMenu_fullscreen,
		       xw->work.ewmh[WhichEWMH].mode);
    } else {
	SetItemSensitivity(mainMenuEntries[mainMenu_fullscreen].widget,
			   False);
    }
}

#endif /* OPT_MAXIMIZE */

#if OPT_SIXEL_GRAPHICS
static void
do_sixelscrolling(Widget gw GCC_UNUSED,
		  XtPointer closure GCC_UNUSED,
		  XtPointer data GCC_UNUSED)
{
    term->keyboard.flags ^= MODE_DECSDM;
    update_decsdm();
}

void
update_decsdm(void)
{
    UpdateCheckbox("update_decsdm",
		   vtMenuEntries,
		   vtMenu_sixelscrolling,
		   (term->keyboard.flags & MODE_DECSDM) != 0);
}

void
HandleSixelScrolling(Widget w,
		     XEvent *event GCC_UNUSED,
		     String *params,
		     Cardinal *param_count)
{
    handle_vt_toggle(do_sixelscrolling, term->keyboard.flags & MODE_DECSDM,
		     params, *param_count, w);
}
#endif

#if OPT_GRAPHICS
static void
do_privatecolorregisters(Widget gw GCC_UNUSED,
			 XtPointer closure GCC_UNUSED,
			 XtPointer data GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    ToggleFlag(screen->privatecolorregisters);
    update_privatecolorregisters();
}

void
update_privatecolorregisters(void)
{
    UpdateCheckbox("update_privatecolorregisters",
		   vtMenuEntries,
		   vtMenu_privatecolorregisters,
		   TScreenOf(term)->privatecolorregisters);
}

void
HandleSetPrivateColorRegisters(Widget w,
			       XEvent *event GCC_UNUSED,
			       String *params,
			       Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(privatecolorregisters);
}
#endif

#if OPT_SUN_FUNC_KEYS
void
HandleSunFunctionKeys(Widget w,
		      XEvent *event GCC_UNUSED,
		      String *params,
		      Cardinal *param_count)
{
    handle_vt_toggle(do_sun_fkeys, term->keyboard.type == keyboardIsSun,
		     params, *param_count, w);
}
#endif

#if OPT_NUM_LOCK
void
HandleNumLock(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params,
	      Cardinal *param_count)
{
    handle_vt_toggle(do_num_lock, term->misc.real_NumLock,
		     params, *param_count, w);
}

void
HandleAltEsc(Widget w,
	     XEvent *event GCC_UNUSED,
	     String *params,
	     Cardinal *param_count)
{
    handle_vt_toggle(do_alt_esc, !TScreenOf(term)->alt_sends_esc,
		     params, *param_count, w);
}

void
HandleMetaEsc(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params,
	      Cardinal *param_count)
{
    handle_vt_toggle(do_meta_esc, TScreenOf(term)->meta_sends_esc,
		     params, *param_count, w);
}
#endif

void
HandleDeleteIsDEL(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    handle_vt_toggle(do_delete_del, TScreenOf(term)->delete_is_del,
		     params, *param_count, w);
}

void
HandleOldFunctionKeys(Widget w,
		      XEvent *event GCC_UNUSED,
		      String *params,
		      Cardinal *param_count)
{
    handle_vt_toggle(do_old_fkeys, term->keyboard.type == keyboardIsLegacy,
		     params, *param_count, w);
}

#if OPT_SUNPC_KBD
void
HandleSunKeyboard(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    handle_vt_toggle(do_sun_kbd, term->keyboard.type == keyboardIsVT220,
		     params, *param_count, w);
}
#endif

#if OPT_HP_FUNC_KEYS
void
HandleHpFunctionKeys(Widget w,
		     XEvent *event GCC_UNUSED,
		     String *params,
		     Cardinal *param_count)
{
    handle_vt_toggle(do_hp_fkeys, term->keyboard.type == keyboardIsHP,
		     params, *param_count, w);
}
#endif

#if OPT_SCO_FUNC_KEYS
void
HandleScoFunctionKeys(Widget w,
		      XEvent *event GCC_UNUSED,
		      String *params,
		      Cardinal *param_count)
{
    handle_vt_toggle(do_sco_fkeys, term->keyboard.type == keyboardIsSCO,
		     params, *param_count, w);
}
#endif

void
HandleScrollbar(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    XtermWidget xw = term;

    if (IsIcon(TScreenOf(xw))) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	handle_vt_toggle(do_scrollbar, TScreenOf(xw)->fullVwin.sb_info.width,
			 params, *param_count, w);
    }
}

void
HandleJumpscroll(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(jumpscroll);
}

void
HandleKeepClipboard(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(keepClipboard);
}

void
HandleKeepSelection(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(keepSelection);
}

void
HandleSetSelect(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    handle_vt_toggle(do_selectClipboard, TScreenOf(term)->selectToClipboard,
		     params, *param_count, w);
}

void
HandleReverseVideo(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    handle_vt_toggle(do_reversevideo, (term->misc.re_verse0),
		     params, *param_count, w);
}

void
HandleAutoWrap(Widget w,
	       XEvent *event GCC_UNUSED,
	       String *params,
	       Cardinal *param_count)
{
    handle_vt_toggle(do_autowrap, (term->flags & WRAPAROUND),
		     params, *param_count, w);
}

void
HandleReverseWrap(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    handle_vt_toggle(do_reversewrap, (term->flags & REVERSEWRAP),
		     params, *param_count, w);
}

void
HandleAutoLineFeed(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    handle_vt_toggle(do_autolinefeed, (term->flags & LINEFEED),
		     params, *param_count, w);
}

void
HandleAppCursor(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    handle_vt_toggle(do_appcursor, (term->keyboard.flags & MODE_DECCKM),
		     params, *param_count, w);
}

void
HandleAppKeypad(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    handle_vt_toggle(do_appkeypad, (term->keyboard.flags & MODE_DECKPAM),
		     params, *param_count, w);
}

void
HandleScrollKey(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(scrollkey);
}

void
HandleScrollTtyOutput(Widget w,
		      XEvent *event GCC_UNUSED,
		      String *params,
		      Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(scrollttyoutput);
}

void
HandleAllow132(Widget w,
	       XEvent *event GCC_UNUSED,
	       String *params,
	       Cardinal *param_count)
{
    handle_vt_toggle(do_allow132, TScreenOf(term)->c132,
		     params, *param_count, w);
}

void
HandleCursesEmul(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    handle_vt_toggle(do_cursesemul, TScreenOf(term)->curses,
		     params, *param_count, w);
}

void
HandleBellIsUrgent(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(bellIsUrgent);
}

void
HandleMarginBell(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(marginbell);
}

#if OPT_BLINK_CURS
void
HandleCursorBlink(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    handle_vt_toggle(do_cursorblink, TScreenOf(term)->cursor_blink,
		     params, *param_count, w);
}
#endif

void
HandleAltScreen(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    /* eventually want to see if sensitive or not */
    handle_vt_toggle(do_altscreen, TScreenOf(term)->whichBuf,
		     params, *param_count, w);
}

void
HandleTiteInhibit(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    /* eventually want to see if sensitive or not */
    handle_vt_toggle(do_titeInhibit, !(term->misc.titeInhibit),
		     params, *param_count, w);
}

/* ARGSUSED */
void
HandleSoftReset(Widget w,
		XEvent *event GCC_UNUSED,
		String *params GCC_UNUSED,
		Cardinal *param_count GCC_UNUSED)
{
    do_softreset(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
void
HandleHardReset(Widget w,
		XEvent *event GCC_UNUSED,
		String *params GCC_UNUSED,
		Cardinal *param_count GCC_UNUSED)
{
    do_hardreset(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
void
HandleClearSavedLines(Widget w,
		      XEvent *event GCC_UNUSED,
		      String *params GCC_UNUSED,
		      Cardinal *param_count GCC_UNUSED)
{
    do_clearsavedlines(w, (XtPointer) 0, (XtPointer) 0);
}

void
HandleAllowBoldFonts(Widget w,
		     XEvent *event GCC_UNUSED,
		     String *params,
		     Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowBoldFonts);
}

#if OPT_LOAD_VTFONTS
void
update_font_escape(void)
{
    TScreen *screen = TScreenOf(term);

    SetItemSensitivity(fontMenuEntries[fontMenu_fontescape].widget,
		       ((screen->allowFontOps &&
			 screen->EscapeFontName())
			? True : False));
}
#endif

#if OPT_DEC_CHRSET
void
HandleFontDoublesize(Widget w,
		     XEvent *event GCC_UNUSED,
		     String *params,
		     Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(font_doublesize);
}
#endif

#if OPT_BOX_CHARS
void
HandleFontBoxChars(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    handle_vt_toggle(do_font_boxchars, TScreenOf(term)->force_box_chars,
		     params, *param_count, w);
}

void
HandleFontPacked(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    handle_vt_toggle(do_font_packed, TScreenOf(term)->force_packed,
		     params, *param_count, w);
}
#endif

#if OPT_DEC_SOFTFONT
void
HandleFontLoading(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *param_count)
{
    handle_vt_toggle(do_font_loadable, term->misc.font_loadable,
		     params, *param_count, w);
}
#endif

#if OPT_RENDERFONT
static void
update_fontmenu(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int n;

    for (n = 0; n <= fontMenu_lastBuiltin; ++n) {
	Boolean active = (Boolean) (xw->work.render_font ||
				    (screen->menu_font_sizes[n] >= 0));
	SetItemSensitivity(fontMenuEntries[n].widget, active);
    }
}

void
HandleRenderFont(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    XtermWidget xw = (XtermWidget) term;

    DefaultRenderFont(xw);

    handle_vt_toggle(do_font_renderfont, xw->work.render_font,
		     params, *param_count, w);

    update_fontmenu(xw);
}
#endif

#if OPT_WIDE_CHARS
void
HandleUTF8Mode(Widget w,
	       XEvent *event GCC_UNUSED,
	       String *params,
	       Cardinal *param_count)
{
    handle_vt_toggle(do_font_utf8_mode, TScreenOf(term)->utf8_mode,
		     params, *param_count, w);
}

void
HandleUTF8Fonts(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    handle_vt_toggle(do_font_utf8_fonts, TScreenOf(term)->utf8_fonts,
		     params, *param_count, w);
}

void
HandleUTF8Title(Widget w,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    handle_vt_toggle(do_font_utf8_title, TScreenOf(term)->utf8_title,
		     params, *param_count, w);
}
#endif

#if OPT_SCREEN_DUMPS
void
HandleDumpHtml(Widget w GCC_UNUSED,
	       XEvent *event GCC_UNUSED,
	       String *params GCC_UNUSED,
	       Cardinal *param_count GCC_UNUSED)
{
    xtermDumpHtml(term);
}

void
HandleDumpSvg(Widget w GCC_UNUSED,
	      XEvent *event GCC_UNUSED,
	      String *params GCC_UNUSED,
	      Cardinal *param_count GCC_UNUSED)
{
    xtermDumpSvg(term);
}
#endif

#if OPT_TEK4014
void
HandleSetTerminalType(Widget w,
		      XEvent *event GCC_UNUSED,
		      String *params,
		      Cardinal *param_count)
{
    XtermWidget xw = term;

    if (*param_count == 1) {
	switch (params[0][0]) {
	case 'v':
	case 'V':
	    if (TEK4014_ACTIVE(xw))
		do_vtmode(w, (XtPointer) 0, (XtPointer) 0);
	    break;
	case 't':
	case 'T':
	    if (!TEK4014_ACTIVE(xw))
		do_tekmode(w, (XtPointer) 0, (XtPointer) 0);
	    break;
	default:
	    Bell(xw, XkbBI_MinorError, 0);
	}
    } else {
	Bell(xw, XkbBI_MinorError, 0);
    }
}

void
HandleVisibility(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    XtermWidget xw = term;

    if (*param_count == 2) {
	switch (params[0][0]) {
	case 'v':
	case 'V':
	    handle_tek_toggle(do_vtonoff, (int) TScreenOf(xw)->Vshow,
			      params + 1, (*param_count) - 1, w);
	    break;
	case 't':
	case 'T':
	    handle_tek_toggle(do_tekonoff, (int) TEK4014_SHOWN(xw),
			      params + 1, (*param_count) - 1, w);
	    break;
	default:
	    Bell(xw, XkbBI_MinorError, 0);
	}
    } else {
	Bell(xw, XkbBI_MinorError, 0);
    }
}

/* ARGSUSED */
void
HandleSetTekText(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    XtermWidget xw = term;
    void (*proc) PROTO_XT_CALLBACK_ARGS = 0;

    switch (*param_count) {
    case 0:
	proc = do_tektextlarge;
	break;
    case 1:
	switch (TekGetFontSize(params[0])) {
	case TEK_FONT_LARGE:
	    proc = do_tektextlarge;
	    break;
	case TEK_FONT_2:
	    proc = do_tektext2;
	    break;
	case TEK_FONT_3:
	    proc = do_tektext3;
	    break;
	case TEK_FONT_SMALL:
	    proc = do_tektextsmall;
	    break;
	}
	break;
    }
    if (proc)
	(*proc) (w, (XtPointer) 0, (XtPointer) 0);
    else
	Bell(xw, XkbBI_MinorError, 0);
}

/* ARGSUSED */
void
HandleTekPage(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params GCC_UNUSED,
	      Cardinal *param_count GCC_UNUSED)
{
    do_tekpage(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
void
HandleTekReset(Widget w,
	       XEvent *event GCC_UNUSED,
	       String *params GCC_UNUSED,
	       Cardinal *param_count GCC_UNUSED)
{
    do_tekreset(w, (XtPointer) 0, (XtPointer) 0);
}

/* ARGSUSED */
void
HandleTekCopy(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params GCC_UNUSED,
	      Cardinal *param_count GCC_UNUSED)
{
    do_tekcopy(w, (XtPointer) 0, (XtPointer) 0);
}
#endif /* OPT_TEK4014 */

#if OPT_TOOLBAR
/*
 * The normal style of xterm popup menu delays initialization until the menu is
 * first requested.  When using a toolbar, we can use the same initialization,
 * though on the first popup there will be a little geometry layout jitter,
 * since the menu is already managed when this callback is invoked.
 */
static void
InitPopup(Widget gw,
	  XtPointer closure,
	  XtPointer data GCC_UNUSED)
{
    String params[2];
    Cardinal count = 1;

    params[0] = (char *) closure;
    params[1] = 0;
    TRACE(("InitPopup(%s)\n", params[0]));

    domenu(gw, (XEvent *) 0, params, &count);

    XtRemoveCallback(gw, XtNpopupCallback, InitPopup, closure);
}

static Dimension
SetupShell(Widget *menus, MenuList * shell, int n, int m)
{
    char temp[80];
    char *external_name = 0;
    Dimension button_height;
    Dimension button_border;
    String saveLocale = setMenuLocale(True, resource.menuLocale);

    shell[n].w = XtVaCreatePopupShell(menu_names[n].internal_name,
				      simpleMenuWidgetClass,
				      *menus,
				      XtNgeometry, NULL,
				      (XtPointer) 0);

    XtAddCallback(shell[n].w, XtNpopupCallback, InitPopup, menu_names[n].internal_name);
    XtVaGetValues(shell[n].w,
		  XtNlabel, &external_name,
		  (XtPointer) 0);

    TRACE(("...SetupShell(%s) -> %s -> %#lx\n",
	   menu_names[n].internal_name,
	   external_name,
	   (long) shell[n].w));

    sprintf(temp, "%sButton", menu_names[n].internal_name);
    shell[n].b = XtVaCreateManagedWidget(temp,
					 menuButtonWidgetClass,
					 *menus,
					 XtNfromHoriz, ((m >= 0)
							? shell[m].b
							: 0),
					 XtNmenuName, menu_names[n].internal_name,
					 XtNlabel, external_name,
					 (XtPointer) 0);
    XtVaGetValues(shell[n].b,
		  XtNheight, &button_height,
		  XtNborderWidth, &button_border,
		  (XtPointer) 0);

    (void) setMenuLocale(False, saveLocale);
    return (Dimension) (button_height + (button_border * 2));
}
#endif /* OPT_TOOLBAR */

void
SetupMenus(Widget shell, Widget *forms, Widget *menus, Dimension *menu_high)
{
#if OPT_TOOLBAR
    Dimension button_height = 0;
    Dimension toolbar_hSpace;
    Arg args[10];
#endif

    TRACE(("SetupMenus(%s)\n", shell == toplevel ? "vt100" : "tek4014"));

    *menu_high = 0;

    if (shell == toplevel) {
	XawSimpleMenuAddGlobalActions(app_con);
	XtRegisterGrabAction(HandlePopupMenu, True,
			     (unsigned) (ButtonPressMask | ButtonReleaseMask),
			     GrabModeAsync, GrabModeAsync);
    }
#if OPT_TOOLBAR
    *forms = XtVaCreateManagedWidget("form",
				     formWidgetClass, shell,
				     (XtPointer) 0);
    xtermAddInput(*forms);

    /*
     * Set a nominal value for the preferred pane size, which lets the
     * buttons determine the actual height of the menu bar.  We don't show
     * the grip, because it's too easy to make the toolbar look bad that
     * way.
     */
    XtSetArg(args[0], XtNorientation, XtorientHorizontal);
    XtSetArg(args[1], XtNtop, XawChainTop);
    XtSetArg(args[2], XtNbottom, XawChainTop);
    XtSetArg(args[3], XtNleft, XawChainLeft);
    XtSetArg(args[4], XtNright, XawChainLeft);

    if (resource.toolBar) {
	*menus = XtCreateManagedWidget("menubar", boxWidgetClass, *forms,
				       args, 5);
    } else {
	*menus = XtCreateWidget("menubar", boxWidgetClass, *forms, args, 5);
    }

    /*
     * The toolbar widget's height is not necessarily known yet.  If the
     * toolbar is not created as a managed widget, we can still make a good
     * guess about its height by collecting the widget's other resource values.
     */
    XtVaGetValues(*menus,
		  XtNhSpace, &toolbar_hSpace,
		  (XtPointer) 0);

    if (shell == toplevel) {	/* vt100 */
	int j;
	for (j = mainMenu; j <= fontMenu; j++) {
	    button_height = SetupShell(menus, vt_shell, j, j - 1);
	}
    }
#if OPT_TEK4014
    else {			/* tek4014 */
	(void) SetupShell(menus, tek_shell, mainMenu, -1);
	button_height = SetupShell(menus, tek_shell, tekMenu, mainMenu);
    }
#endif

    /*
     * Tell the main program how high the toolbar is, to help with the initial
     * layout.
     */
    *menu_high = (Dimension) (button_height + 2 * (toolbar_hSpace));
    TRACE(("...menuHeight:%d = (%d + 2 * %d)\n",
	   *menu_high, button_height, toolbar_hSpace));

#else /* !OPT_TOOLBAR */
    *forms = shell;
    *menus = shell;
#endif

    TRACE(("...shell=%#lx\n", (long) shell));
    TRACE(("...forms=%#lx\n", (long) *forms));
    TRACE(("...menus=%#lx\n", (long) *menus));
}

void
repairSizeHints(void)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    if (XtIsRealized((Widget) xw)) {
	getXtermSizeHints(xw);
	xtermSizeHints(xw, ScrollbarWidth(screen));

	XSetWMNormalHints(screen->display, VShellWindow(xw), &xw->hints);
    }
}

#if OPT_TOOLBAR
#define INIT_POPUP(s, n) InitPopup(s[n].w, menu_names[n].internal_name, 0)

static Bool
InitWidgetMenu(Widget shell)
{
    Bool result = False;

    TRACE(("InitWidgetMenu(%p)\n", (void *) shell));
    if (term != 0) {
	if (shell == toplevel) {	/* vt100 */
	    if (!term->init_menu) {
		INIT_POPUP(vt_shell, mainMenu);
		INIT_POPUP(vt_shell, vtMenu);
		INIT_POPUP(vt_shell, fontMenu);
		term->init_menu = True;
		TRACE(("...InitWidgetMenu(vt)\n"));
	    }
	    result = term->init_menu;
	}
#if OPT_TEK4014
	else if (tekWidget) {	/* tek4014 */
	    if (!tekWidget->init_menu) {
		INIT_POPUP(tek_shell, mainMenu);
		INIT_POPUP(tek_shell, tekMenu);
		tekWidget->init_menu = True;
		TRACE(("...InitWidgetMenu(tek)\n"));
	    }
	    result = tekWidget->init_menu;
	}
#endif
    }
    TRACE(("...InitWidgetMenu ->%d\n", result));
    return result;
}

static TbInfo *
toolbar_info(Widget w)
{
    TRACE(("...getting toolbar_info\n"));
#if OPT_TEK4014
    if (w != (Widget) term)
	return &(tekWidget->tek.tb_info);
#else
    (void) w;
#endif
    return &(WhichVWin(TScreenOf(term))->tb_info);
}

static void
hide_toolbar(Widget w)
{
    if (w != 0) {
	TbInfo *info = toolbar_info(w);

	TRACE(("hiding toolbar\n"));
	XtVaSetValues(w,
		      XtNfromVert, (Widget) 0,
		      (XtPointer) 0);

	if (info->menu_bar != 0) {
	    repairSizeHints();
	    XtUnmanageChild(info->menu_bar);
	    if (XtIsRealized(info->menu_bar)) {
		XtUnmapWidget(info->menu_bar);
	    }
	}
	TRACE(("...hiding toolbar (done)\n"));
    }
}

static void
show_toolbar(Widget w)
{
    if (w != 0) {
	TbInfo *info = toolbar_info(w);

	TRACE(("showing toolbar\n"));
	if (info->menu_bar != 0) {
	    XtVaSetValues(w,
			  XtNfromVert, info->menu_bar,
			  (XtPointer) 0);
	    if (XtIsRealized(info->menu_bar))
		repairSizeHints();
	    XtManageChild(info->menu_bar);
	    if (XtIsRealized(info->menu_bar)) {
		XtMapWidget(info->menu_bar);
	    }
	}
	/*
	 * This is needed to make the terminal widget move down below the
	 * toolbar.
	 */
	XawFormDoLayout(XtParent(w), True);
	TRACE(("...showing toolbar (done)\n"));
    }
}

/*
 * Make the toolbar visible or invisible in the current window(s).
 */
void
ShowToolbar(Bool enable)
{
    XtermWidget xw = term;

    TRACE(("ShowToolbar(%d)\n", enable));

    if (IsIcon(TScreenOf(xw))) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	if (enable) {
	    if (InitWidgetMenu(toplevel))
		show_toolbar((Widget) xw);
#if OPT_TEK4014
	    if (InitWidgetMenu(tekshellwidget))
		show_toolbar((Widget) tekWidget);
#endif
	} else {
	    hide_toolbar((Widget) xw);
#if OPT_TEK4014
	    hide_toolbar((Widget) tekWidget);
#endif
	}
	resource.toolBar = (Boolean) enable;
	update_toolbar();
    }
}

void
HandleToolbar(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params GCC_UNUSED,
	      Cardinal *param_count GCC_UNUSED)
{
    XtermWidget xw = term;

    if (IsIcon(TScreenOf(xw))) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	handle_vt_toggle(do_toolbar, resource.toolBar,
			 params, *param_count, w);
    }
}

/* ARGSUSED */
static void
do_toolbar(Widget gw GCC_UNUSED,
	   XtPointer closure GCC_UNUSED,
	   XtPointer data GCC_UNUSED)
{
    XtermWidget xw = term;

    /*
     * Toggle toolbars for both vt100 and tek windows, since they share the
     * menu which contains the checkbox indicating whether the toolbar is
     * active.
     */
    if (IsIcon(TScreenOf(xw))) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	ShowToolbar(ToggleFlag(resource.toolBar));
    }
}

void
update_toolbar(void)
{
    UpdateCheckbox("update_toolbar",
		   mainMenuEntries,
		   mainMenu_toolbar,
		   resource.toolBar);
}
#endif /* OPT_TOOLBAR */

void
update_securekbd(void)
{
    UpdateCheckbox("update_securekbd",
		   mainMenuEntries,
		   mainMenu_securekbd,
		   TScreenOf(term)->grabbedKbd);
}

void
update_allowsends(void)
{
    UpdateCheckbox("update_allowsends",
		   mainMenuEntries,
		   mainMenu_allowsends,
		   TScreenOf(term)->allowSendEvents);
}

#ifdef ALLOWLOGGING
void
update_logging(void)
{
    UpdateCheckbox("update_logging",
		   mainMenuEntries,
		   mainMenu_logging,
		   TScreenOf(term)->logging);
}
#endif

#if OPT_PRINT_ON_EXIT
void
update_write_error(void)
{
    UpdateCheckbox("update_write_error",
		   mainMenuEntries,
		   mainMenu_write_error,
		   TScreenOf(term)->write_error);
}
#endif

void
update_print_redir(void)
{
    UpdateCheckbox("update_print_redir",
		   mainMenuEntries,
		   mainMenu_print_redir,
		   PrinterOf(TScreenOf(term)).printer_controlmode);
}

void
update_8bit_control(void)
{
    UpdateCheckbox("update_8bit_control",
		   mainMenuEntries,
		   mainMenu_8bit_ctrl,
		   TScreenOf(term)->control_eight_bits);
}

void
update_decbkm(void)
{
    UpdateCheckbox("update_decbkm",
		   mainMenuEntries,
		   mainMenu_backarrow,
		   (term->keyboard.flags & MODE_DECBKM) != 0);
}

#if OPT_NUM_LOCK
void
update_num_lock(void)
{
    UpdateCheckbox("update_num_lock",
		   mainMenuEntries,
		   mainMenu_num_lock,
		   term->misc.real_NumLock);
}

void
update_alt_esc(void)
{
    UpdateCheckbox("update_alt_esc",
		   mainMenuEntries,
		   mainMenu_alt_esc,
		   TScreenOf(term)->alt_sends_esc);
}

void
update_meta_esc(void)
{
    UpdateCheckbox("update_meta_esc",
		   mainMenuEntries,
		   mainMenu_meta_esc,
		   TScreenOf(term)->meta_sends_esc);
}
#endif

#if OPT_SUN_FUNC_KEYS
void
update_sun_fkeys(void)
{
    UpdateCheckbox("update_sun_fkeys",
		   mainMenuEntries,
		   mainMenu_sun_fkeys,
		   term->keyboard.type == keyboardIsSun);
}
#endif

#if OPT_TCAP_FKEYS
void
update_tcap_fkeys(void)
{
    UpdateCheckbox("update_tcap_fkeys",
		   mainMenuEntries,
		   mainMenu_tcap_fkeys,
		   term->keyboard.type == keyboardIsTermcap);
}
#endif

void
update_old_fkeys(void)
{
    UpdateCheckbox("update_old_fkeys",
		   mainMenuEntries,
		   mainMenu_old_fkeys,
		   term->keyboard.type == keyboardIsLegacy);
}

void
update_delete_del(void)
{
    UpdateCheckbox("update_delete_del",
		   mainMenuEntries,
		   mainMenu_delete_del,
		   xtermDeleteIsDEL(term));
}

#if OPT_SUNPC_KBD
void
update_sun_kbd(void)
{
    UpdateCheckbox("update_sun_kbd",
		   mainMenuEntries,
		   mainMenu_sun_kbd,
		   term->keyboard.type == keyboardIsVT220);
}
#endif

#if OPT_HP_FUNC_KEYS
void
update_hp_fkeys(void)
{
    UpdateCheckbox("update_hp_fkeys",
		   mainMenuEntries,
		   mainMenu_hp_fkeys,
		   term->keyboard.type == keyboardIsHP);
}
#endif

#if OPT_SCO_FUNC_KEYS
void
update_sco_fkeys(void)
{
    UpdateCheckbox("update_sco_fkeys",
		   mainMenuEntries,
		   mainMenu_sco_fkeys,
		   term->keyboard.type == keyboardIsSCO);
}
#endif

void
update_scrollbar(void)
{
    UpdateCheckbox("update_scrollbar",
		   vtMenuEntries,
		   vtMenu_scrollbar,
		   ScrollbarWidth(TScreenOf(term)));
}

void
update_jumpscroll(void)
{
    UpdateCheckbox("update_jumpscroll",
		   vtMenuEntries,
		   vtMenu_jumpscroll,
		   TScreenOf(term)->jumpscroll);
}

void
update_reversevideo(void)
{
    UpdateCheckbox("update_reversevideo",
		   vtMenuEntries,
		   vtMenu_reversevideo,
		   (term->misc.re_verse));
}

void
update_autowrap(void)
{
    UpdateCheckbox("update_autowrap",
		   vtMenuEntries,
		   vtMenu_autowrap,
		   (term->flags & WRAPAROUND) != 0);
}

void
update_reversewrap(void)
{
    UpdateCheckbox("update_reversewrap",
		   vtMenuEntries,
		   vtMenu_reversewrap,
		   (term->flags & REVERSEWRAP) != 0);
}

void
update_autolinefeed(void)
{
    UpdateCheckbox("update_autolinefeed",
		   vtMenuEntries,
		   vtMenu_autolinefeed,
		   (term->flags & LINEFEED) != 0);
}

void
update_appcursor(void)
{
    UpdateCheckbox("update_appcursor",
		   vtMenuEntries,
		   vtMenu_appcursor,
		   (term->keyboard.flags & MODE_DECCKM) != 0);
}

void
update_appkeypad(void)
{
    UpdateCheckbox("update_appkeypad",
		   vtMenuEntries,
		   vtMenu_appkeypad,
		   (term->keyboard.flags & MODE_DECKPAM) != 0);
}

void
update_scrollkey(void)
{
    UpdateCheckbox("update_scrollkey",
		   vtMenuEntries,
		   vtMenu_scrollkey,
		   TScreenOf(term)->scrollkey);
}

void
update_scrollttyoutput(void)
{
    UpdateCheckbox("update_scrollttyoutput",
		   vtMenuEntries,
		   vtMenu_scrollttyoutput,
		   TScreenOf(term)->scrollttyoutput);
}

void
update_keepSelection(void)
{
    UpdateCheckbox("update_keepSelection",
		   vtMenuEntries,
		   vtMenu_keepSelection,
		   TScreenOf(term)->keepSelection);
}

void
update_selectToClipboard(void)
{
    UpdateCheckbox("update_selectToClipboard",
		   vtMenuEntries,
		   vtMenu_selectToClipboard,
		   TScreenOf(term)->selectToClipboard);
}

void
update_allow132(void)
{
    UpdateCheckbox("update_allow132",
		   vtMenuEntries,
		   vtMenu_allow132,
		   TScreenOf(term)->c132);
}

void
update_cursesemul(void)
{
#if 0				/* 2006-2-12: no longer menu entry */
    UpdateMenuItem("update_cursesemul", vtMenuEntries, vtMenu_cursesemul,
		   TScreenOf(term)->curses);
#endif
}

void
update_visualbell(void)
{
    UpdateCheckbox("update_visualbell",
		   vtMenuEntries,
		   vtMenu_visualbell,
		   TScreenOf(term)->visualbell);
}

void
update_bellIsUrgent(void)
{
    UpdateCheckbox("update_bellIsUrgent",
		   vtMenuEntries,
		   vtMenu_bellIsUrgent,
		   TScreenOf(term)->bellIsUrgent);
}

void
update_poponbell(void)
{
    UpdateCheckbox("update_poponbell",
		   vtMenuEntries,
		   vtMenu_poponbell,
		   TScreenOf(term)->poponbell);
}

#ifndef update_marginbell	/* 2007-3-7: no longer menu entry */
void
update_marginbell(void)
{
    UpdateCheckbox("update_marginbell",
		   vtMenuEntries,
		   vtMenu_marginbell,
		   TScreenOf(term)->marginbell);
}
#endif

#if OPT_BLINK_CURS
void
update_cursorblink(void)
{
    BlinkOps check = TScreenOf(term)->cursor_blink;

    if (check == cbAlways ||
	check == cbNever) {
	SetItemSensitivity(vtMenuEntries[vtMenu_cursorblink].widget, False);
    }
    UpdateCheckbox("update_cursorblink",
		   vtMenuEntries,
		   vtMenu_cursorblink,
		   (check == cbTrue ||
		    check == cbAlways));
}
#endif

void
update_altscreen(void)
{
    UpdateCheckbox("update_altscreen",
		   vtMenuEntries,
		   vtMenu_altscreen,
		   TScreenOf(term)->whichBuf);
}

void
update_titeInhibit(void)
{
    UpdateCheckbox("update_titeInhibit",
		   vtMenuEntries,
		   vtMenu_titeInhibit,
		   !(term->misc.titeInhibit));
}

#ifndef NO_ACTIVE_ICON
void
update_activeicon(void)
{
    UpdateCheckbox("update_activeicon",
		   vtMenuEntries,
		   vtMenu_activeicon,
		   term->work.active_icon);
}
#endif /* NO_ACTIVE_ICON */

static void
do_allowBoldFonts(Widget w,
		  XtPointer closure GCC_UNUSED,
		  XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowBoldFonts);
	update_menu_allowBoldFonts();
	Redraw();
    }
}

#if OPT_DEC_CHRSET
void
update_font_doublesize(void)
{
    UpdateCheckbox("update_font_doublesize",
		   fontMenuEntries,
		   fontMenu_font_doublesize,
		   TScreenOf(term)->font_doublesize);
}
#endif

#if OPT_BOX_CHARS
void
update_font_boxchars(void)
{
    SetItemSensitivity(fontMenuEntries[fontMenu_font_boxchars].widget,
		       !TScreenOf(term)->broken_box_chars);
    UpdateCheckbox("update_font_boxchars",
		   fontMenuEntries,
		   fontMenu_font_boxchars,
		   TScreenOf(term)->force_box_chars ||
		   TScreenOf(term)->broken_box_chars);
}

void
update_font_packed(void)
{
    UpdateCheckbox("update_font_packed",
		   fontMenuEntries,
		   fontMenu_font_packedfont,
		   TScreenOf(term)->force_packed);
}
#endif

#if OPT_DEC_SOFTFONT
void
update_font_loadable(void)
{
    UpdateCheckbox("update_font_loadable",
		   fontMenuEntries,
		   fontMenu_font_loadable,
		   term->misc.font_loadable);
}
#endif

#if OPT_RENDERFONT
void
update_font_renderfont(void)
{
    UpdateCheckbox("update_font_renderfont",
		   fontMenuEntries,
		   fontMenu_render_font,
		   (term->work.render_font == True));
    SetItemSensitivity(fontMenuEntries[fontMenu_render_font].widget,
		       !IsEmpty(CurrentXftFont(term)));

    if (term->work.render_font) {
	TScreenOf(term)->broken_box_chars = term->work.broken_box_chars;
    } else {
	TScreenOf(term)->broken_box_chars = False;
    }
    update_font_boxchars();

    update_fontmenu(term);
}
#endif

#if OPT_WIDE_CHARS
void
update_font_utf8_mode(void)
{
    Bool active = (TScreenOf(term)->utf8_mode != uAlways);
    Bool enable = (TScreenOf(term)->utf8_mode != uFalse);

    TRACE(("update_font_utf8_mode active %d, enable %d\n", active, enable));
    SetItemSensitivity(fontMenuEntries[fontMenu_utf8_mode].widget, active);
    UpdateCheckbox("update_font_utf8_mode",
		   fontMenuEntries,
		   fontMenu_utf8_mode,
		   enable);
}

void
update_font_utf8_fonts(void)
{
    Bool active = (TScreenOf(term)->utf8_fonts != uAlways);
    Bool enable = (TScreenOf(term)->utf8_fonts != uFalse);

    TRACE(("update_font_utf8_fonts active %d, enable %d\n", active, enable));
    SetItemSensitivity(fontMenuEntries[fontMenu_utf8_fonts].widget, active);
    UpdateCheckbox("update_font_utf8_fonts",
		   fontMenuEntries,
		   fontMenu_utf8_fonts,
		   enable);
}

void
update_font_utf8_title(void)
{
    Bool active = (TScreenOf(term)->utf8_mode != uFalse);
    Bool enable = (TScreenOf(term)->utf8_title);

    TRACE(("update_font_utf8_title active %d, enable %d\n", active, enable));
    SetItemSensitivity(fontMenuEntries[fontMenu_utf8_title].widget, active);
    UpdateCheckbox("update_font_utf8_title",
		   fontMenuEntries,
		   fontMenu_utf8_title,
		   enable);
}
#endif

#if OPT_DEC_CHRSET || OPT_BOX_CHARS || OPT_DEC_SOFTFONT
void
update_menu_allowBoldFonts(void)
{
    UpdateCheckbox("update_menu_allowBoldFonts",
		   fontMenuEntries,
		   fontMenu_allowBoldFonts,
		   TScreenOf(term)->allowBoldFonts);
}
#endif

#if OPT_ALLOW_XXX_OPS
static void
enable_allow_xxx_ops(Bool enable)
{
    SetItemSensitivity(fontMenuEntries[fontMenu_allowFontOps].widget, enable);
    SetItemSensitivity(fontMenuEntries[fontMenu_allowMouseOps].widget, enable);
    SetItemSensitivity(fontMenuEntries[fontMenu_allowTcapOps].widget, enable);
    SetItemSensitivity(fontMenuEntries[fontMenu_allowTitleOps].widget, enable);
    SetItemSensitivity(fontMenuEntries[fontMenu_allowWindowOps].widget, enable);
}

static void
do_allowColorOps(Widget w,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowColorOps);
	update_menu_allowColorOps();
    }
}

static void
do_allowFontOps(Widget w,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowFontOps);
	update_menu_allowFontOps();
    }
}

static void
do_allowMouseOps(Widget w,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowMouseOps);
	update_menu_allowMouseOps();
    }
}

static void
do_allowTcapOps(Widget w,
		XtPointer closure GCC_UNUSED,
		XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowTcapOps);
	update_menu_allowTcapOps();
    }
}

static void
do_allowTitleOps(Widget w,
		 XtPointer closure GCC_UNUSED,
		 XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowTitleOps);
	update_menu_allowTitleOps();
    }
}

static void
do_allowWindowOps(Widget w,
		  XtPointer closure GCC_UNUSED,
		  XtPointer data GCC_UNUSED)
{
    XtermWidget xw = getXtermWidget(w);
    if (xw != 0) {
	ToggleFlag(TScreenOf(xw)->allowWindowOps);
	update_menu_allowWindowOps();
    }
}

void
HandleAllowColorOps(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowColorOps);
}

void
HandleAllowFontOps(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowFontOps);
}

void
HandleAllowMouseOps(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowMouseOps);
}

void
HandleAllowTcapOps(Widget w,
		   XEvent *event GCC_UNUSED,
		   String *params,
		   Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowTcapOps);
}

void
HandleAllowTitleOps(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowTitleOps);
}

void
HandleAllowWindowOps(Widget w,
		     XEvent *event GCC_UNUSED,
		     String *params,
		     Cardinal *param_count)
{
    HANDLE_VT_TOGGLE(allowWindowOps);
}

void
update_menu_allowColorOps(void)
{
    UpdateCheckbox("update_menu_allowColorOps",
		   fontMenuEntries,
		   fontMenu_allowColorOps,
		   TScreenOf(term)->allowColorOps);
}

void
update_menu_allowFontOps(void)
{
    UpdateCheckbox("update_menu_allowFontOps",
		   fontMenuEntries,
		   fontMenu_allowFontOps,
		   TScreenOf(term)->allowFontOps);
}

void
update_menu_allowMouseOps(void)
{
    UpdateCheckbox("update_menu_allowMouseOps",
		   fontMenuEntries,
		   fontMenu_allowMouseOps,
		   TScreenOf(term)->allowMouseOps);
}

void
update_menu_allowTcapOps(void)
{
    UpdateCheckbox("update_menu_allowTcapOps",
		   fontMenuEntries,
		   fontMenu_allowTcapOps,
		   TScreenOf(term)->allowTcapOps);
}

void
update_menu_allowTitleOps(void)
{
    UpdateCheckbox("update_menu_allowTitleOps",
		   fontMenuEntries,
		   fontMenu_allowTitleOps,
		   TScreenOf(term)->allowTitleOps);
}

void
update_menu_allowWindowOps(void)
{
    UpdateCheckbox("update_menu_allowWindowOps",
		   fontMenuEntries,
		   fontMenu_allowWindowOps,
		   TScreenOf(term)->allowWindowOps);
}
#endif

#if OPT_TEK4014
void
update_tekshow(void)
{
    if (!(TScreenOf(term)->inhibit & I_TEK)) {
	UpdateCheckbox("update_tekshow",
		       vtMenuEntries,
		       vtMenu_tekshow,
		       TEK4014_SHOWN(term));
    }
}

void
update_vttekmode(void)
{
    XtermWidget xw = term;

    if (!(TScreenOf(xw)->inhibit & I_TEK)) {
	UpdateCheckbox("update_vtmode",
		       vtMenuEntries,
		       vtMenu_tekmode,
		       TEK4014_ACTIVE(xw));
	UpdateCheckbox("update_tekmode",
		       tekMenuEntries,
		       tekMenu_vtmode,
		       !TEK4014_ACTIVE(xw));
	update_fullscreen();
    }
}

void
update_vtshow(void)
{
    if (!(TScreenOf(term)->inhibit & I_TEK)) {
	UpdateCheckbox("update_vtshow",
		       tekMenuEntries,
		       tekMenu_vtshow,
		       TScreenOf(term)->Vshow);
    }
}

void
set_vthide_sensitivity(void)
{
    if (!(TScreenOf(term)->inhibit & I_TEK)) {
	SetItemSensitivity(
			      vtMenuEntries[vtMenu_vthide].widget,
			      TEK4014_SHOWN(term));
    }
}

void
set_tekhide_sensitivity(void)
{
    if (!(TScreenOf(term)->inhibit & I_TEK)) {
	SetItemSensitivity(
			      tekMenuEntries[tekMenu_tekhide].widget,
			      TScreenOf(term)->Vshow);
    }
}

void
set_tekfont_menu_item(int n, int val)
{
    if (!(TScreenOf(term)->inhibit & I_TEK)) {
	UpdateCheckbox("set_tekfont_menu_item", tekMenuEntries, FS2MI(n),
		       (val));
    }
}
#endif /* OPT_TEK4014 */

void
set_menu_font(int val)
{
    UpdateCheckbox("set_menu_font",
		   fontMenuEntries,
		   TScreenOf(term)->menu_font_number,
		   (val));
}
