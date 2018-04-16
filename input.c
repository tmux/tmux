/* $XTermId: input.c,v 1.357 2017/05/29 20:11:03 tom Exp $ */

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

/* input.c */

#include <xterm.h>

#include <X11/keysym.h>

#ifdef VMS
#include <X11/keysymdef.h>
#endif

#if HAVE_X11_DECKEYSYM_H
#include <X11/DECkeysym.h>
#endif

#if HAVE_X11_SUNKEYSYM_H
#include <X11/Sunkeysym.h>
#endif

#if HAVE_X11_XF86KEYSYM_H
#include <X11/XF86keysym.h>
#endif

#ifdef HAVE_XKBKEYCODETOKEYSYM
#include <X11/XKBlib.h>
#endif

#include <X11/Xutil.h>
#include <stdio.h>
#include <ctype.h>

#include <xutf8.h>

#include <data.h>
#include <fontutils.h>
#include <xstrings.h>
#include <xtermcap.h>

/*
 * Xutil.h has no macro to check for the complete set of function- and
 * modifier-keys that might be returned.  Fake it.
 */
#ifdef XK_ISO_Lock
#define IsPredefinedKey(n) ((n) >= XK_ISO_Lock && (n) <= XK_Delete)
#else
#define IsPredefinedKey(n) ((n) >= XK_BackSpace && (n) <= XK_Delete)
#endif

#ifdef XK_ISO_Left_Tab
#define IsTabKey(n) ((n) == XK_Tab || (n) == XK_ISO_Left_Tab)
#else
#define IsTabKey(n) ((n) == XK_Tab)
#endif

#ifndef IsPrivateKeypadKey
#define IsPrivateKeypadKey(k) (0)
#endif

#define IsBackarrowToggle(keyboard, keysym, state) \
	((((keyboard->flags & MODE_DECBKM) == 0) \
	    ^ ((state & ControlMask) != 0)) \
	&& (keysym == XK_BackSpace))

#define MAP(from, to) case from: result = to; break
#define Masked(value,mask) ((value) & (unsigned) (~(mask)))

#define KEYSYM_FMT "0x%04lX"	/* simplify matching <X11/keysymdef.h> */

#define TEK4014_GIN(tw) (tw != 0 && TekScreenOf(tw)->TekGIN)

typedef struct {
    KeySym keysym;
    Bool is_fkey;
    int nbytes;
#define STRBUFSIZE 500
    char strbuf[STRBUFSIZE];
} KEY_DATA;

static
const char kypd_num[] = " XXXXXXXX\tXXX\rXXXxxxxXXXXXXXXXXXXXXXXXXXXX*+,-./0123456789XXX=";
/*                       0123456789 abc def0123456789abcdef0123456789abcdef0123456789abcd */
static
const char kypd_apl[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ??????abcdefghijklmnopqrstuvwxyzXXX";
/*                       0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcd */
static
const char curfinal[] = "HDACB  FE";

static int decfuncvalue(KEY_DATA *);
static void sunfuncvalue(ANSI *, KEY_DATA *);
static void hpfuncvalue(ANSI *, KEY_DATA *);
static void scofuncvalue(ANSI *, KEY_DATA *);

static void
AdjustAfterInput(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->scrollkey && screen->topline != 0)
	WindowScroll(xw, 0, False);
    if (screen->marginbell) {
	int col = screen->max_col - screen->nmarginbell;
	if (screen->bellArmed >= 0) {
	    if (screen->bellArmed == screen->cur_row) {
		if (screen->cur_col >= col) {
		    Bell(xw, XkbBI_MarginBell, 0);
		    screen->bellArmed = -1;
		}
	    } else {
		screen->bellArmed =
		    screen->cur_col < col ? screen->cur_row : -1;
	    }
	} else if (screen->cur_col < col)
	    screen->bellArmed = screen->cur_row;
    }
}

/*
 * Return true if the key is on the editing keypad.  This overlaps with
 * IsCursorKey() and IsKeypadKey() and must be tested before those macros to
 * distinguish it from them.
 *
 * VT220  emulation  uses  the  VT100  numeric  keypad as well as a 6-key
 * editing keypad. Here's a picture of the VT220 editing keypad:
 *      +--------+--------+--------+
 *      | Find   | Insert | Remove |
 *      +--------+--------+--------+
 *      | Select | Prev   | Next   |
 *      +--------+--------+--------+
 *
 * and the similar Sun and PC keypads:
 *      +--------+--------+--------+
 *      | Insert | Home   | PageUp |
 *      +--------+--------+--------+
 *      | Delete | End    | PageDn |
 *      +--------+--------+--------+
 */
static Bool
IsEditKeypad(XtermWidget xw, KeySym keysym)
{
    Bool result;

    switch (keysym) {
    case XK_Delete:
	result = !xtermDeleteIsDEL(xw);
	break;
    case XK_Prior:
    case XK_Next:
    case XK_Insert:
    case XK_Find:
    case XK_Select:
#ifdef DXK_Remove
    case DXK_Remove:
#endif
	result = True;
	break;
    default:
	result = False;
	break;
    }
    return result;
}

/*
 * Editing-keypad, plus other editing keys which are not included in the
 * other macros.
 */
static Bool
IsEditFunctionKey(XtermWidget xw, KeySym keysym)
{
    Bool result;

    switch (keysym) {
#ifdef XK_KP_Delete
    case XK_KP_Delete:		/* editing key on numeric keypad */
    case XK_KP_Insert:		/* editing key on numeric keypad */
#endif
#ifdef XK_ISO_Left_Tab
    case XK_ISO_Left_Tab:
#endif
	result = True;
	break;
    default:
	result = IsEditKeypad(xw, keysym);
	break;
    }
    return result;
}

#if OPT_MOD_FKEYS
#define IS_CTRL(n) ((n) < ANSI_SPA || ((n) >= 0x7f && (n) <= 0x9f))

/*
 * Return true if the keysym corresponds to one of the control characters,
 * or one of the common ASCII characters that is combined with control to
 * make a control character.
 */
static Bool
IsControlInput(KEY_DATA * kd)
{
    return ((kd->keysym) >= 0x40 && (kd->keysym) <= 0x7f);
}

static Bool
IsControlOutput(KEY_DATA * kd)
{
    return IS_CTRL(kd->keysym);
}

/*
 * X "normally" has some built-in translations, which the user may want to
 * suppress when processing the modifyOtherKeys resource.  In particular, the
 * control modifier applied to some of the keyboard digits gives results for
 * control characters.
 *
 * control 2   0    NUL
 * control SPC 0    NUL
 * control @   0    NUL
 * control `   0    NUL
 * control 3   0x1b ESC
 * control 4   0x1c FS
 * control \   0x1c FS
 * control 5   0x1d GS
 * control 6   0x1e RS
 * control ^   0x1e RS
 * control ~   0x1e RS
 * control 7   0x1f US
 * control /   0x1f US
 * control _   0x1f US
 * control 8   0x7f DEL
 *
 * It is possible that some other keyboards do not work for these combinations,
 * but they do work with modifyOtherKeys=2 for the US keyboard:
 *
 * control `   0    NUL
 * control [   0x1b ESC
 * control \   0x1c FS
 * control ]   0x1d GS
 * control ?   0x7f DEL
 */
static Bool
IsControlAlias(KEY_DATA * kd)
{
    Bool result = False;

    if (kd->nbytes == 1) {
	result = IS_CTRL(CharOf(kd->strbuf[0]));
    }
    return result;
}

/*
 * If we are in the non-VT220/VT52 keyboard state, allow modifiers to add a
 * parameter to the function-key control sequences.
 *
 * Note that we generally cannot capture the Shift-modifier for the numeric
 * keypad since this is commonly used to act as a type of NumLock, e.g.,
 * making the keypad send "7" (actually XK_KP_7) where the unshifted code
 * would be Home (XK_KP_Home).  The other modifiers work, subject to the
 * usual window-manager assignments.
 */
#if OPT_SUNPC_KBD
#define LegacyAllows(code) (!is_legacy || (code & xw->keyboard.modify_now.allow_keys) != 0)
#else
#define LegacyAllows(code) True
#endif

static Bool
allowModifierParm(XtermWidget xw, KEY_DATA * kd)
{
    TKeyboard *keyboard = &(xw->keyboard);
    int is_legacy = (keyboard->type == keyboardIsLegacy);
    Bool result = False;

#if OPT_SUNPC_KBD
    if (keyboard->type == keyboardIsVT220)
	is_legacy = True;
#endif

#if OPT_VT52_MODE
    if (TScreenOf(xw)->vtXX_level != 0)
#endif
    {
	if (IsCursorKey(kd->keysym) || IsEditFunctionKey(xw, kd->keysym)) {
	    result = LegacyAllows(2);
	} else if (IsKeypadKey(kd->keysym)) {
	    result = LegacyAllows(1);
	} else if (IsFunctionKey(kd->keysym)) {
	    result = LegacyAllows(4);
	} else if (IsMiscFunctionKey(kd->keysym)) {
	    result = LegacyAllows(8);
	}
    }
    if (xw->keyboard.modify_now.other_keys != 0) {
	result = True;
    }
    return result;
}

/*
* Modifier codes:
*       None                  1
*       Shift                 2 = 1(None)+1(Shift)
*       Alt                   3 = 1(None)+2(Alt)
*       Alt+Shift             4 = 1(None)+1(Shift)+2(Alt)
*       Ctrl                  5 = 1(None)+4(Ctrl)
*       Ctrl+Shift            6 = 1(None)+1(Shift)+4(Ctrl)
*       Ctrl+Alt              7 = 1(None)+2(Alt)+4(Ctrl)
*       Ctrl+Alt+Shift        8 = 1(None)+1(Shift)+2(Alt)+4(Ctrl)
*       Meta                  9 = 1(None)+8(Meta)
*       Meta+Shift           10 = 1(None)+8(Meta)+1(Shift)
*       Meta+Alt             11 = 1(None)+8(Meta)+2(Alt)
*       Meta+Alt+Shift       12 = 1(None)+8(Meta)+1(Shift)+2(Alt)
*       Meta+Ctrl            13 = 1(None)+8(Meta)+4(Ctrl)
*       Meta+Ctrl+Shift      14 = 1(None)+8(Meta)+1(Shift)+4(Ctrl)
*       Meta+Ctrl+Alt        15 = 1(None)+8(Meta)+2(Alt)+4(Ctrl)
*       Meta+Ctrl+Alt+Shift  16 = 1(None)+8(Meta)+1(Shift)+2(Alt)+4(Ctrl)
*/

unsigned
xtermParamToState(XtermWidget xw, unsigned param)
{
    unsigned result = 0;
#if OPT_NUM_LOCK
    if (param > MOD_NONE) {
	if ((param - MOD_NONE) & MOD_SHIFT)
	    UIntSet(result, ShiftMask);
	if ((param - MOD_NONE) & MOD_CTRL)
	    UIntSet(result, ControlMask);
	if ((param - MOD_NONE) & MOD_ALT)
	    UIntSet(result, xw->work.alt_mods);
	if ((param - MOD_NONE) & MOD_META)
	    UIntSet(result, xw->work.meta_mods);
    }
#else
    (void) xw;
    (void) param;
#endif
    TRACE(("xtermParamToState(%d) %s%s%s%s -> %#x\n", param,
	   MODIFIER_NAME(param, MOD_SHIFT),
	   MODIFIER_NAME(param, MOD_ALT),
	   MODIFIER_NAME(param, MOD_CTRL),
	   MODIFIER_NAME(param, MOD_META),
	   result));
    return result;
}

unsigned
xtermStateToParam(XtermWidget xw, unsigned state)
{
    unsigned modify_parm = MOD_NONE;

    TRACE(("xtermStateToParam %#x\n", state));
#if OPT_NUM_LOCK
    if (state & ShiftMask) {
	modify_parm += MOD_SHIFT;
	UIntClr(state, ShiftMask);
    }
    if (state & ControlMask) {
	modify_parm += MOD_CTRL;
	UIntClr(state, ControlMask);
    }
    if ((state & xw->work.alt_mods) != 0) {
	modify_parm += MOD_ALT;
	UIntClr(state, xw->work.alt_mods);
    }
    if ((state & xw->work.meta_mods) != 0) {
	modify_parm += MOD_META;
	/* UIntClr(state, xw->work.meta_mods); */
    }
    if (modify_parm == MOD_NONE)
	modify_parm = 0;
#else
    (void) xw;
    (void) state;
#endif
    TRACE(("...xtermStateToParam %d%s%s%s%s\n", modify_parm,
	   MODIFIER_NAME(modify_parm, MOD_SHIFT),
	   MODIFIER_NAME(modify_parm, MOD_ALT),
	   MODIFIER_NAME(modify_parm, MOD_CTRL),
	   MODIFIER_NAME(modify_parm, MOD_META)));
    return modify_parm;
}

#define computeMaskedModifier(xw, state, mask) \
	xtermStateToParam(xw, Masked(state, mask))

#if OPT_NUM_LOCK
static unsigned
filterAltMeta(unsigned result, unsigned mask, Bool enable, KEY_DATA * kd)
{
    if ((result & mask) != 0) {
	/*
	 * metaSendsEscape makes the meta key independent of
	 * modifyOtherKeys.
	 */
	if (enable) {
	    result &= ~mask;
	}
	/*
	 * A bare meta-modifier is independent of modifyOtherKeys.  If it
	 * is combined with other modifiers, make it depend.
	 */
	if ((result & ~(mask)) == 0) {
	    result &= ~mask;
	}
	/*
	 * Check for special cases of control+meta which are used by some
	 * applications, e.g., emacs.
	 */
	if ((IsControlInput(kd)
	     || IsControlOutput(kd))
	    && (result & ControlMask) != 0) {
	    result &= ~(mask | ControlMask);
	}
	if (kd->keysym == XK_Return || kd->keysym == XK_Tab) {
	    result &= ~(mask | ControlMask);
	}
    }
    return result;
}
#endif /* OPT_NUM_LOCK */

/*
 * Single characters (not function-keys) are allowed fewer modifiers when
 * interpreting modifyOtherKeys due to pre-existing associations with some
 * modifiers.
 */
static unsigned
allowedCharModifiers(XtermWidget xw, unsigned state, KEY_DATA * kd)
{
#if OPT_NUM_LOCK
    unsigned a_or_m = (state & (xw->work.meta_mods | xw->work.alt_mods));
#else
    unsigned a_or_m = 0;
#endif
    /*
     * Start by limiting the result to the modifiers we might want to use.
     */
    unsigned result = (state & (ControlMask
				| ShiftMask
				| a_or_m));

    /*
     * If modifyOtherKeys is off or medium (0 or 1), moderate its effects by
     * excluding the common cases for modifiers.
     */
    if (xw->keyboard.modify_now.other_keys <= 1) {
	if (IsControlInput(kd)
	    && Masked(result, ControlMask) == 0) {
	    /* These keys are already associated with the control-key */
	    if (xw->keyboard.modify_now.other_keys == 0) {
		UIntClr(result, ControlMask);
	    }
	} else if (kd->keysym == XK_Tab || kd->keysym == XK_Return) {
	    /* EMPTY */ ;
	} else if (IsControlAlias(kd)) {
	    /* Things like "^_" work here... */
	    if (Masked(result, (ControlMask | ShiftMask)) == 0) {
		result = 0;
	    }
	} else if (!IsControlOutput(kd) && !IsPredefinedKey(kd->keysym)) {
	    /* Printable keys are already associated with the shift-key */
	    if (!(result & ControlMask)) {
		UIntClr(result, ShiftMask);
	    }
	}
#if OPT_NUM_LOCK
	result = filterAltMeta(result,
			       xw->work.meta_mods,
			       TScreenOf(xw)->meta_sends_esc, kd);
	if (TScreenOf(xw)->alt_is_not_meta) {
	    result = filterAltMeta(result,
				   xw->work.alt_mods,
				   TScreenOf(xw)->alt_sends_esc, kd);
	}
#endif
    }
    TRACE(("...allowedCharModifiers(state=%u" FMT_MODIFIER_NAMES
	   ", ch=" KEYSYM_FMT ") ->"
	   "%u" FMT_MODIFIER_NAMES "\n",
	   state, ARG_MODIFIER_NAMES(state), kd->keysym,
	   result, ARG_MODIFIER_NAMES(result)));
    return result;
}

/*
 * Decide if we should generate a special escape sequence for "other" keys
 * than cursor-, function-keys, etc., as per the modifyOtherKeys resource.
 */
static Bool
ModifyOtherKeys(XtermWidget xw,
		unsigned state,
		KEY_DATA * kd,
		unsigned modify_parm)
{
    TKeyboard *keyboard = &(xw->keyboard);
    Bool result = False;

    /*
     * Exclude the keys already covered by a modifier.
     */
    if (kd->is_fkey
	|| IsEditFunctionKey(xw, kd->keysym)
	|| IsKeypadKey(kd->keysym)
	|| IsCursorKey(kd->keysym)
	|| IsPFKey(kd->keysym)
	|| IsMiscFunctionKey(kd->keysym)
	|| IsPrivateKeypadKey(kd->keysym)) {
	result = False;
    } else if (modify_parm != 0) {
	if (IsBackarrowToggle(keyboard, kd->keysym, state)) {
	    kd->keysym = XK_Delete;
	    UIntClr(state, ControlMask);
	}
	if (!IsPredefinedKey(kd->keysym)) {
	    state = allowedCharModifiers(xw, state, kd);
	}
	if (state != 0) {
	    switch (keyboard->modify_now.other_keys) {
	    default:
		break;
	    case 1:
		switch (kd->keysym) {
		case XK_BackSpace:
		case XK_Delete:
		    result = False;
		    break;
#ifdef XK_ISO_Left_Tab
		case XK_ISO_Left_Tab:
		    if (computeMaskedModifier(xw, state, ShiftMask))
			result = True;
		    break;
#endif
		case XK_Return:
		case XK_Tab:
		    result = (modify_parm != 0);
		    break;
		default:
		    if (IsControlInput(kd)) {
			if (state == ControlMask || state == ShiftMask) {
			    result = False;
			} else {
			    result = (modify_parm != 0);
			}
		    } else if (IsControlAlias(kd)) {
			if (state == ShiftMask)
			    result = False;
			else if (computeMaskedModifier(xw, state, ControlMask)) {
			    result = True;
			}
		    } else {
			result = True;
		    }
		    break;
		}
		break;
	    case 2:
		switch (kd->keysym) {
		case XK_BackSpace:
		    /* strip ControlMask as per IsBackarrowToggle() */
		    if (computeMaskedModifier(xw, state, ControlMask))
			result = True;
		    break;
		case XK_Delete:
		    result = (xtermStateToParam(xw, state) != 0);
		    break;
#ifdef XK_ISO_Left_Tab
		case XK_ISO_Left_Tab:
		    if (computeMaskedModifier(xw, state, ShiftMask))
			result = True;
		    break;
#endif
		case XK_Return:
		case XK_Tab:
		    result = (modify_parm != 0);
		    break;
		default:
		    if (IsControlInput(kd)) {
			result = True;
		    } else if (state == ShiftMask) {
			result = (kd->keysym == ' ' || kd->keysym == XK_Return);
		    } else if (computeMaskedModifier(xw, state, ShiftMask)) {
			result = True;
		    }
		    break;
		}
		break;
	    }
	}
    }
    TRACE(("...ModifyOtherKeys(%d,%d) %s\n",
	   keyboard->modify_now.other_keys,
	   modify_parm,
	   BtoS(result)));
    return result;
}

#define APPEND_PARM(number) \
	    reply->a_param[reply->a_nparam] = (ParmType) number; \
	    reply->a_nparam++

/*
 * Function-key code 27 happens to not be used in the vt220-style encoding.
 * xterm uses this to represent modified non-function-keys such as control/+ in
 * the Sun/PC keyboard layout.  See the modifyOtherKeys resource in the manpage
 * for more information.
 */
static Bool
modifyOtherKey(ANSI *reply, int input_char, unsigned modify_parm, int format_keys)
{
    Bool result = False;

    if (input_char >= 0) {
	reply->a_type = ANSI_CSI;
	if (format_keys) {
	    APPEND_PARM(input_char);
	    APPEND_PARM(modify_parm);
	    reply->a_final = 'u';
	} else {
	    APPEND_PARM(27);
	    APPEND_PARM(modify_parm);
	    APPEND_PARM(input_char);
	    reply->a_final = '~';
	}

	result = True;
    }
    return result;
}

static void
modifyCursorKey(ANSI *reply, int modify, unsigned *modify_parm)
{
    if (*modify_parm != 0) {
	if (modify < 0) {
	    *modify_parm = 0;
	}
	if (modify > 0) {
	    reply->a_type = ANSI_CSI;	/* SS3 should not have params */
	}
	if (modify > 1 && reply->a_nparam == 0) {
	    APPEND_PARM(1);	/* force modifier to 2nd param */
	}
	if (modify > 2) {
	    reply->a_pintro = '>';	/* mark this as "private" */
	}
    }
}
#else
#define modifyCursorKey(reply, modify, parm)	/* nothing */
#endif /* OPT_MOD_FKEYS */

#if OPT_SUNPC_KBD
/*
 * If we have told xterm that our keyboard is really a Sun/PC keyboard, this is
 * enough to make a reasonable approximation to DEC vt220 numeric and editing
 * keypads.
 */
static KeySym
TranslateFromSUNPC(KeySym keysym)
{
    /* *INDENT-OFF* */
    static struct {
	    KeySym before, after;
    } table[] = {
#ifdef DXK_Remove
	{ XK_Delete,       DXK_Remove },
#endif
	{ XK_Home,         XK_Find },
	{ XK_End,          XK_Select },
#ifdef XK_KP_Home
	{ XK_Delete,       XK_KP_Decimal },
	{ XK_KP_Delete,    XK_KP_Decimal },
	{ XK_KP_Insert,    XK_KP_0 },
	{ XK_KP_End,       XK_KP_1 },
	{ XK_KP_Down,      XK_KP_2 },
	{ XK_KP_Next,      XK_KP_3 },
	{ XK_KP_Left,      XK_KP_4 },
	{ XK_KP_Begin,     XK_KP_5 },
	{ XK_KP_Right,     XK_KP_6 },
	{ XK_KP_Home,      XK_KP_7 },
	{ XK_KP_Up,        XK_KP_8 },
	{ XK_KP_Prior,     XK_KP_9 },
#endif
    };
    /* *INDENT-ON* */

    unsigned n;

    for (n = 0; n < sizeof(table) / sizeof(table[0]); n++) {
	if (table[n].before == keysym) {
	    TRACE(("...Input keypad before was " KEYSYM_FMT "\n", keysym));
	    keysym = table[n].after;
	    TRACE(("...Input keypad changed to " KEYSYM_FMT "\n", keysym));
	    break;
	}
    }
    return keysym;
}
#endif /* OPT_SUNPC_KBD */

#define VT52_KEYPAD \
	if_OPT_VT52_MODE(screen,{ \
		reply.a_type = ANSI_ESC; \
		reply.a_pintro = '?'; \
		})

#define VT52_CURSOR_KEYS \
	if_OPT_VT52_MODE(screen,{ \
		reply.a_type = ANSI_ESC; \
		})

#undef  APPEND_PARM
#define APPEND_PARM(number) \
	    reply.a_param[reply.a_nparam] = (ParmType) number, \
	    reply.a_nparam++

#if OPT_MOD_FKEYS
#define MODIFIER_PARM \
	if (modify_parm != 0) APPEND_PARM(modify_parm)
#else
#define MODIFIER_PARM		/*nothing */
#endif

/*
 * Determine if we use the \E[3~ sequence for Delete, or the legacy ^?.  We
 * maintain the delete_is_del value as 3 states:  unspecified(2), true and
 * false.  If unspecified, it is handled differently according to whether the
 * legacy keyboard support is enabled, or if xterm emulates a VT220.
 *
 * Once the user (or application) has specified delete_is_del via resource
 * setting, popup menu or escape sequence, it overrides the keyboard type
 * rather than the reverse.
 */
Bool
xtermDeleteIsDEL(XtermWidget xw)
{
    Bool result = True;

    if (xw->keyboard.type == keyboardIsDefault
	|| xw->keyboard.type == keyboardIsVT220)
	result = (TScreenOf(xw)->delete_is_del == True);

    if (xw->keyboard.type == keyboardIsLegacy)
	result = (TScreenOf(xw)->delete_is_del != False);

    TRACE(("xtermDeleteIsDEL(%d/%d) = %d\n",
	   xw->keyboard.type,
	   TScreenOf(xw)->delete_is_del,
	   result));

    return result;
}

static Boolean
lookupKeyData(KEY_DATA * kd, XtermWidget xw, XKeyEvent *event)
{
    TScreen *screen = TScreenOf(xw);
    Boolean result = True;
#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
#if OPT_MOD_FKEYS
    TKeyboard *keyboard = &(xw->keyboard);
#endif
#endif

    (void) screen;

    TRACE(("%s %#x\n", visibleEventType(event->type), event->keycode));

    kd->keysym = 0;
    kd->is_fkey = False;
#if OPT_TCAP_QUERY
    if (screen->tc_query_code >= 0) {
	kd->keysym = (KeySym) screen->tc_query_code;
	kd->is_fkey = screen->tc_query_fkey;
	if (kd->keysym != XK_BackSpace) {
	    kd->nbytes = 0;
	    kd->strbuf[0] = 0;
	} else {
	    kd->nbytes = 1;
	    kd->strbuf[0] = 8;
	}
    } else
#endif
    {
#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
	TInput *input = lookupTInput(xw, (Widget) xw);
	if (input && input->xic) {
	    Status status_return;
#if OPT_WIDE_CHARS
	    if (screen->utf8_mode) {
		kd->nbytes = Xutf8LookupString(input->xic, event,
					       kd->strbuf, (int) sizeof(kd->strbuf),
					       &(kd->keysym), &status_return);
	    } else
#endif
	    {
		kd->nbytes = XmbLookupString(input->xic, event,
					     kd->strbuf, (int) sizeof(kd->strbuf),
					     &(kd->keysym), &status_return);
	    }
#if OPT_MOD_FKEYS
	    /*
	     * Fill-in some code useful with IsControlAlias():
	     */
	    if (status_return == XLookupBoth
		&& kd->nbytes <= 1
		&& !IsPredefinedKey(kd->keysym)
		&& (keyboard->modify_now.other_keys > 1)
		&& !IsControlInput(kd)) {
		kd->nbytes = 1;
		kd->strbuf[0] = (char) kd->keysym;
	    }
#endif /* OPT_MOD_FKEYS */
	} else
#endif /* OPT_I18N_SUPPORT */
	{
	    static XComposeStatus compose_status =
	    {NULL, 0};
	    kd->nbytes = XLookupString(event,
				       kd->strbuf, (int) sizeof(kd->strbuf),
				       &(kd->keysym), &compose_status);
	}
	kd->is_fkey = IsFunctionKey(kd->keysym);
    }
    return result;
}

void
Input(XtermWidget xw,
      XKeyEvent *event,
      Bool eightbit)
{
    Char *string;

    TKeyboard *keyboard = &(xw->keyboard);
    TScreen *screen = TScreenOf(xw);

    int j;
    int key = False;
    ANSI reply;
    int dec_code;
    unsigned modify_parm = 0;
    int keypad_mode = ((keyboard->flags & MODE_DECKPAM) != 0);
    unsigned evt_state = event->state;
    unsigned mod_state;
    KEY_DATA kd;

    /* Ignore characters typed at the keyboard */
    if (keyboard->flags & MODE_KAM)
	return;

    lookupKeyData(&kd, xw, event);

    memset(&reply, 0, sizeof(reply));

    TRACE(("Input keysym "
	   KEYSYM_FMT
	   ", %d:'%s'%s" FMT_MODIFIER_NAMES "%s%s%s%s%s%s\n",
	   kd.keysym,
	   kd.nbytes,
	   visibleChars((Char *) kd.strbuf,
			((kd.nbytes > 0)
			 ? (unsigned) kd.nbytes
			 : 0)),
	   ARG_MODIFIER_NAMES(evt_state),
	   eightbit ? " 8bit" : " 7bit",
	   IsKeypadKey(kd.keysym) ? " KeypadKey" : "",
	   IsCursorKey(kd.keysym) ? " CursorKey" : "",
	   IsPFKey(kd.keysym) ? " PFKey" : "",
	   kd.is_fkey ? " FKey" : "",
	   IsMiscFunctionKey(kd.keysym) ? " MiscFKey" : "",
	   IsEditFunctionKey(xw, kd.keysym) ? " EditFkey" : ""));

#if OPT_SUNPC_KBD
    /*
     * DEC keyboards don't have keypad(+), but do have keypad(,) instead.
     * Other (Sun, PC) keyboards commonly have keypad(+), but no keypad(,)
     * - it's a pain for users to work around.
     */
    if (keyboard->type == keyboardIsVT220
	&& (evt_state & ShiftMask) == 0) {
	if (kd.keysym == XK_KP_Add) {
	    kd.keysym = XK_KP_Separator;
	    UIntClr(evt_state, ShiftMask);
	    TRACE(("...Input keypad(+), change keysym to "
		   KEYSYM_FMT
		   "\n",
		   kd.keysym));
	}
	if ((evt_state & ControlMask) != 0
	    && kd.keysym == XK_KP_Separator) {
	    kd.keysym = XK_KP_Subtract;
	    UIntClr(evt_state, ControlMask);
	    TRACE(("...Input control/keypad(,), change keysym to "
		   KEYSYM_FMT
		   "\n",
		   kd.keysym));
	}
    }
#endif

    /*
     * The keyboard tables may give us different keypad codes according to
     * whether NumLock is pressed.  Use this check to simplify the process
     * of determining whether we generate an escape sequence for a keypad
     * key, or force it to the value kypd_num[].  There is no fixed
     * modifier for this feature, so we assume that it is the one assigned
     * to the NumLock key.
     *
     * This check used to try to return the contents of strbuf, but that
     * does not work properly when a control modifier is given (trash is
     * returned in the buffer in some cases -- perhaps an X bug).
     */
#if OPT_NUM_LOCK
    if (kd.nbytes == 1
	&& IsKeypadKey(kd.keysym)
	&& xw->misc.real_NumLock
	&& (xw->work.num_lock & evt_state) != 0) {
	keypad_mode = 0;
	TRACE(("...Input num_lock, force keypad_mode off\n"));
    }
#endif

#if OPT_MOD_FKEYS
    if (evt_state != 0
	&& allowModifierParm(xw, &kd)) {
	modify_parm = xtermStateToParam(xw, evt_state);
    }

    /*
     * Shift-tab is often mapped to XK_ISO_Left_Tab which is classified as
     * IsEditFunctionKey(), and the conversion does not produce any bytes.
     * Check for this special case so we have data when handling the
     * modifyOtherKeys resource.
     */
    if (keyboard->modify_now.other_keys > 1) {
	if (IsTabKey(kd.keysym) && kd.nbytes == 0) {
	    kd.nbytes = 1;
	    kd.strbuf[0] = '\t';
	}
    }
#ifdef XK_ISO_Left_Tab
    else if (IsTabKey(kd.keysym)
	     && kd.nbytes <= 1
	     && modify_parm == (MOD_NONE + MOD_SHIFT)) {
	kd.keysym = XK_ISO_Left_Tab;
    }
#endif
#endif /* OPT_MOD_FKEYS */

    /* VT300 & up: backarrow toggle */
    if ((kd.nbytes == 1)
	&& IsBackarrowToggle(keyboard, kd.keysym, evt_state)) {
	kd.strbuf[0] = ANSI_DEL;
	TRACE(("...Input backarrow changed to %d\n", kd.strbuf[0]));
    }
#if OPT_SUNPC_KBD
    /* make an DEC editing-keypad from a Sun or PC editing-keypad */
    if (keyboard->type == keyboardIsVT220
	&& (kd.keysym != XK_Delete || !xtermDeleteIsDEL(xw)))
	kd.keysym = TranslateFromSUNPC(kd.keysym);
    else
#endif
    {
#ifdef XK_KP_Home
	if (kd.keysym >= XK_KP_Home && kd.keysym <= XK_KP_Begin) {
	    TRACE(("...Input keypad before was " KEYSYM_FMT "\n", kd.keysym));
	    kd.keysym += (KeySym) (XK_Home - XK_KP_Home);
	    TRACE(("...Input keypad changed to " KEYSYM_FMT "\n", kd.keysym));
	}
#endif
    }

    /*
     * Map the Sun afterthought-keys in as F36 and F37.
     */
#ifdef SunXK_F36
    if (!kd.is_fkey) {
	if (kd.keysym == SunXK_F36) {
	    kd.keysym = XK_Fn(36);
	    kd.is_fkey = True;
	}
	if (kd.keysym == SunXK_F37) {
	    kd.keysym = XK_Fn(37);
	    kd.is_fkey = True;
	}
    }
#endif

    /*
     * Use the control- and shift-modifiers to obtain more function keys than
     * the keyboard provides.  We can do this if there is no conflicting use of
     * those modifiers:
     *
     * a) for VT220 keyboard, we use only the control-modifier.  The keyboard
     * uses shift-modifier for UDK's.
     *
     * b) for non-VT220 keyboards, we only have to check if the
     * modifyFunctionKeys resource is inactive.
     *
     * Thereafter, we note when we have a function-key and keep that
     * distinction when testing for "function-key" values.
     */
    if ((evt_state & (ControlMask | ShiftMask)) != 0
	&& kd.is_fkey) {

	/* VT220 keyboard uses shift for UDK */
	if (keyboard->type == keyboardIsVT220
	    || keyboard->type == keyboardIsLegacy) {

	    TRACE(("...map XK_F%ld", kd.keysym - XK_Fn(1) + 1));
	    if (evt_state & ControlMask) {
		kd.keysym += (KeySym) xw->misc.ctrl_fkeys;
		UIntClr(evt_state, ControlMask);
	    }
	    TRACE((" to XK_F%ld\n", kd.keysym - XK_Fn(1) + 1));

	}
#if OPT_MOD_FKEYS
	else if (keyboard->modify_now.function_keys < 0) {

	    TRACE(("...map XK_F%ld", kd.keysym - XK_Fn(1) + 1));
	    if (evt_state & ShiftMask) {
		kd.keysym += (KeySym) (xw->misc.ctrl_fkeys * 1);
		UIntClr(evt_state, ShiftMask);
	    }
	    if (evt_state & ControlMask) {
		kd.keysym += (KeySym) (xw->misc.ctrl_fkeys * 2);
		UIntClr(evt_state, ControlMask);
	    }
	    TRACE((" to XK_F%ld\n", kd.keysym - XK_Fn(1) + 1));

	}
	/*
	 * Reevaluate the modifier parameter, stripping off the modifiers
	 * that we just used.
	 */
	if (modify_parm) {
	    modify_parm = xtermStateToParam(xw, evt_state);
	}
#endif /* OPT_MOD_FKEYS */
    }

    /*
     * Test for one of the keyboard variants.
     */
    switch (keyboard->type) {
    case keyboardIsHP:
	hpfuncvalue(&reply, &kd);
	break;
    case keyboardIsSCO:
	scofuncvalue(&reply, &kd);
	break;
    case keyboardIsSun:
	sunfuncvalue(&reply, &kd);
	break;
    case keyboardIsTermcap:
#if OPT_TCAP_FKEYS
	if (xtermcapString(xw, (int) kd.keysym, evt_state))
	    return;
#endif
	break;
    case keyboardIsDefault:
    case keyboardIsLegacy:
    case keyboardIsVT220:
	break;
    }

    if (reply.a_final) {
	/*
	 * The key symbol matches one of the variants.  Most of those are
	 * function-keys, though some cursor- and editing-keys are mixed in.
	 */
	modifyCursorKey(&reply,
			((kd.is_fkey
			  || IsMiscFunctionKey(kd.keysym)
			  || IsEditFunctionKey(xw, kd.keysym))
			 ? keyboard->modify_now.function_keys
			 : keyboard->modify_now.cursor_keys),
			&modify_parm);
	MODIFIER_PARM;
	unparseseq(xw, &reply);
    } else if (((kd.is_fkey
		 || IsMiscFunctionKey(kd.keysym)
		 || IsEditFunctionKey(xw, kd.keysym))
#if OPT_MOD_FKEYS
		&& !ModifyOtherKeys(xw, evt_state, &kd, modify_parm)
#endif
	       ) || (kd.keysym == XK_Delete
		     && ((modify_parm != 0)
			 || !xtermDeleteIsDEL(xw)))) {
	dec_code = decfuncvalue(&kd);
	if ((evt_state & ShiftMask)
#if OPT_SUNPC_KBD
	    && keyboard->type == keyboardIsVT220
#endif
	    && ((string = (Char *) udk_lookup(xw, dec_code, &kd.nbytes)) != 0)) {
	    UIntClr(evt_state, ShiftMask);
	    while (kd.nbytes-- > 0)
		unparseputc(xw, CharOf(*string++));
	}
	/*
	 * Interpret F1-F4 as PF1-PF4 for VT52, VT100
	 */
	else if (keyboard->type != keyboardIsLegacy
		 && (dec_code >= 11 && dec_code <= 14)) {
	    reply.a_type = ANSI_SS3;
	    VT52_CURSOR_KEYS;
	    reply.a_final = (Char) A2E(dec_code - 11 + E2A('P'));
	    modifyCursorKey(&reply,
			    keyboard->modify_now.function_keys,
			    &modify_parm);
	    MODIFIER_PARM;
	    unparseseq(xw, &reply);
	} else {
	    reply.a_type = ANSI_CSI;
	    reply.a_final = 0;

#ifdef XK_ISO_Left_Tab
	    if (kd.keysym == XK_ISO_Left_Tab) {
		reply.a_nparam = 0;
		reply.a_final = 'Z';
#if OPT_MOD_FKEYS
		if (keyboard->modify_now.other_keys > 1
		    && computeMaskedModifier(xw, evt_state, ShiftMask))
		    modifyOtherKey(&reply, '\t', modify_parm, keyboard->format_keys);
#endif
	    } else
#endif /* XK_ISO_Left_Tab */
	    {
		reply.a_nparam = 1;
#if OPT_MOD_FKEYS
		if (kd.is_fkey) {
		    modifyCursorKey(&reply,
				    keyboard->modify_now.function_keys,
				    &modify_parm);
		}
		MODIFIER_PARM;
#endif
		reply.a_param[0] = (ParmType) dec_code;
		reply.a_final = '~';
	    }
	    if (reply.a_final != 0
		&& (reply.a_nparam == 0 || reply.a_param[0] >= 0))
		unparseseq(xw, &reply);
	}
	key = True;
    } else if (IsPFKey(kd.keysym)) {
	reply.a_type = ANSI_SS3;
	reply.a_final = (Char) ((kd.keysym - XK_KP_F1) + 'P');
	VT52_CURSOR_KEYS;
	MODIFIER_PARM;
	unparseseq(xw, &reply);
	key = True;
    } else if (IsKeypadKey(kd.keysym)) {
	if (keypad_mode) {
	    reply.a_type = ANSI_SS3;
	    reply.a_final = (Char) (kypd_apl[kd.keysym - XK_KP_Space]);
	    VT52_KEYPAD;
	    MODIFIER_PARM;
	    unparseseq(xw, &reply);
	} else {
	    unparseputc(xw, kypd_num[kd.keysym - XK_KP_Space]);
	}
	key = True;
    } else if (IsCursorKey(kd.keysym)) {
	if (keyboard->flags & MODE_DECCKM) {
	    reply.a_type = ANSI_SS3;
	} else {
	    reply.a_type = ANSI_CSI;
	}
	modifyCursorKey(&reply, keyboard->modify_now.cursor_keys, &modify_parm);
	reply.a_final = (Char) (curfinal[kd.keysym - XK_Home]);
	VT52_CURSOR_KEYS;
	MODIFIER_PARM;
	unparseseq(xw, &reply);
	key = True;
    } else if (kd.nbytes > 0) {

#if OPT_TEK4014
	if (TEK4014_GIN(tekWidget)) {
	    TekEnqMouse(tekWidget, kd.strbuf[0]);
	    TekGINoff(tekWidget);
	    kd.nbytes--;
	    for (j = 0; j < kd.nbytes; ++j) {
		kd.strbuf[j] = kd.strbuf[j + 1];
	    }
	}
#endif
#if OPT_MOD_FKEYS
	if ((keyboard->modify_now.other_keys > 0)
	    && ModifyOtherKeys(xw, evt_state, &kd, modify_parm)
	    && (mod_state = allowedCharModifiers(xw, evt_state, &kd)) != 0) {
	    int input_char;

	    evt_state = mod_state;

	    modify_parm = xtermStateToParam(xw, evt_state);

	    /*
	     * We want to show a keycode that corresponds to the 8-bit value
	     * of the key.  If the keysym is less than 256, that is good
	     * enough.  Special keys such as Tab may result in a value that
	     * is usable as well.  For the latter (special cases), try to use
	     * the result from the X library lookup.
	     */
	    input_char = ((kd.keysym < 256)
			  ? (int) kd.keysym
			  : ((kd.nbytes == 1)
			     ? CharOf(kd.strbuf[0])
			     : -1));

	    TRACE(("...modifyOtherKeys %d;%d\n", modify_parm, input_char));
	    if (modifyOtherKey(&reply, input_char, modify_parm, keyboard->format_keys)) {
		unparseseq(xw, &reply);
	    } else {
		Bell(xw, XkbBI_MinorError, 0);
	    }
	} else
#endif /* OPT_MOD_FKEYS */
	{
	    int prefix = 0;

#if OPT_NUM_LOCK
	    /*
	     * Send ESC if we have a META modifier and metaSendsEcape is true.
	     * Like eightBitInput, except that it is not associated with
	     * terminal settings.
	     */
	    if (kd.nbytes != 0) {
		if (screen->meta_sends_esc
		    && (evt_state & xw->work.meta_mods) != 0) {
		    TRACE(("...input-char is modified by META\n"));
		    UIntClr(evt_state, xw->work.meta_mods);
		    eightbit = False;
		    prefix = ANSI_ESC;
		} else if (eightbit) {
		    /* it might be overridden, but this helps for debugging */
		    TRACE(("...input-char is shifted by META\n"));
		}
		if (screen->alt_is_not_meta
		    && (evt_state & xw->work.alt_mods) != 0) {
		    UIntClr(evt_state, xw->work.alt_mods);
		    if (screen->alt_sends_esc) {
			TRACE(("...input-char is modified by ALT\n"));
			eightbit = False;
			prefix = ANSI_ESC;
		    } else if (!eightbit) {
			TRACE(("...input-char is shifted by ALT\n"));
			eightbit = True;
		    }
		}
	    }
#endif
	    /*
	     * If metaSendsEscape is false, fall through to this chunk, which
	     * implements the eightBitInput resource.
	     *
	     * It is normally executed when the user presses Meta plus a
	     * printable key, e.g., Meta+space.  The presence of the Meta
	     * modifier is not guaranteed since what really happens is the
	     * "insert-eight-bit" or "insert-seven-bit" action, which we
	     * distinguish by the eightbit parameter to this function.  So the
	     * eightBitInput resource really means that we use this shifting
	     * logic in the "insert-eight-bit" action.
	     */
	    if (eightbit && (kd.nbytes == 1) && screen->input_eight_bits) {
		IChar ch = CharOf(kd.strbuf[0]);
		if ((ch < 128) && (screen->eight_bit_meta == ebTrue)) {
		    kd.strbuf[0] |= (char) 0x80;
		    TRACE(("...input shift from %d to %d (%#x to %#x)\n",
			   ch, CharOf(kd.strbuf[0]),
			   ch, CharOf(kd.strbuf[0])));
#if OPT_WIDE_CHARS
		    if (screen->utf8_mode) {
			/*
			 * We could interpret the incoming code as "in the
			 * current locale", but it's simpler to treat it as
			 * a Unicode value to translate to UTF-8.
			 */
			ch = CharOf(kd.strbuf[0]);
			kd.nbytes = 2;
			kd.strbuf[0] = (char) (0xc0 | ((ch >> 6) & 0x3));
			kd.strbuf[1] = (char) (0x80 | (ch & 0x3f));
			TRACE(("...encoded %#x in UTF-8 as %#x,%#x\n",
			       ch, CharOf(kd.strbuf[0]), CharOf(kd.strbuf[1])));
		    }
#endif
		}
		eightbit = False;
	    }
#if OPT_WIDE_CHARS
	    if (kd.nbytes == 1)	/* cannot do NRC on UTF-8, for instance */
#endif
	    {
		/* VT220 & up: National Replacement Characters */
		if ((xw->flags & NATIONAL) != 0) {
		    unsigned cmp = xtermCharSetIn(screen,
						  CharOf(kd.strbuf[0]),
						  screen->keyboard_dialect[0]);
		    TRACE(("...input NRC %d, %s %d\n",
			   CharOf(kd.strbuf[0]),
			   (CharOf(kd.strbuf[0]) == cmp)
			   ? "unchanged"
			   : "changed to",
			   CharOf(cmp)));
		    kd.strbuf[0] = (char) cmp;
		} else if (eightbit) {
		    prefix = ANSI_ESC;
		} else if (kd.strbuf[0] == '?'
			   && (evt_state & ControlMask) != 0) {
		    kd.strbuf[0] = ANSI_DEL;
		    /* UIntClr(evt_state, ControlMask); */
		}
	    }
	    if (prefix != 0)
		unparseputc(xw, prefix);	/* escape */
	    for (j = 0; j < kd.nbytes; ++j)
		unparseputc(xw, CharOf(kd.strbuf[j]));
	}
	key = ((kd.keysym != ANSI_XOFF) && (kd.keysym != ANSI_XON));
    }
    unparse_end(xw);

    if (key && !TEK4014_ACTIVE(xw))
	AdjustAfterInput(xw);

    xtermShowPointer(xw, False);
    return;
}

void
StringInput(XtermWidget xw, const Char *string, size_t nbytes)
{
    TRACE(("InputString (%s,%lu)\n",
	   visibleChars(string, (unsigned) nbytes),
	   (unsigned long) nbytes));
#if OPT_TEK4014
    if (nbytes && TEK4014_GIN(tekWidget)) {
	TekEnqMouse(tekWidget, *string++);
	TekGINoff(tekWidget);
	nbytes--;
    }
#endif
    while (nbytes-- != 0)
	unparseputc(xw, *string++);
    if (!TEK4014_ACTIVE(xw))
	AdjustAfterInput(xw);
    unparse_end(xw);
}

/* These definitions are DEC-style (e.g., vt320) */
static int
decfuncvalue(KEY_DATA * kd)
{
    int result;

    if (kd->is_fkey) {
	switch (kd->keysym) {
	    MAP(XK_Fn(1), 11);
	    MAP(XK_Fn(2), 12);
	    MAP(XK_Fn(3), 13);
	    MAP(XK_Fn(4), 14);
	    MAP(XK_Fn(5), 15);
	    MAP(XK_Fn(6), 17);
	    MAP(XK_Fn(7), 18);
	    MAP(XK_Fn(8), 19);
	    MAP(XK_Fn(9), 20);
	    MAP(XK_Fn(10), 21);
	    MAP(XK_Fn(11), 23);
	    MAP(XK_Fn(12), 24);
	    MAP(XK_Fn(13), 25);
	    MAP(XK_Fn(14), 26);
	    MAP(XK_Fn(15), 28);
	    MAP(XK_Fn(16), 29);
	    MAP(XK_Fn(17), 31);
	    MAP(XK_Fn(18), 32);
	    MAP(XK_Fn(19), 33);
	    MAP(XK_Fn(20), 34);
	default:
	    /* after F20 the codes are made up and do not correspond to any
	     * real terminal.  So they are simply numbered sequentially.
	     */
	    result = 42 + (int) (kd->keysym - XK_Fn(21));
	    break;
	}
    } else {
	switch (kd->keysym) {
	    MAP(XK_Find, 1);
	    MAP(XK_Insert, 2);
	    MAP(XK_Delete, 3);
#ifdef XK_KP_Insert
	    MAP(XK_KP_Insert, 2);
	    MAP(XK_KP_Delete, 3);
#endif
#ifdef DXK_Remove
	    MAP(DXK_Remove, 3);
#endif
	    MAP(XK_Select, 4);
	    MAP(XK_Prior, 5);
	    MAP(XK_Next, 6);
#ifdef XK_ISO_Left_Tab
	    MAP(XK_ISO_Left_Tab, 'Z');
#endif
	    MAP(XK_Help, 28);
	    MAP(XK_Menu, 29);
	default:
	    result = -1;
	    break;
	}
    }
    return result;
}

static void
hpfuncvalue(ANSI *reply, KEY_DATA * kd)
{
#if OPT_HP_FUNC_KEYS
    int result;

    if (kd->is_fkey) {
	switch (kd->keysym) {
	    MAP(XK_Fn(1), 'p');
	    MAP(XK_Fn(2), 'q');
	    MAP(XK_Fn(3), 'r');
	    MAP(XK_Fn(4), 's');
	    MAP(XK_Fn(5), 't');
	    MAP(XK_Fn(6), 'u');
	    MAP(XK_Fn(7), 'v');
	    MAP(XK_Fn(8), 'w');
	default:
	    result = -1;
	    break;
	}
    } else {
	switch (kd->keysym) {
	    MAP(XK_Up, 'A');
	    MAP(XK_Down, 'B');
	    MAP(XK_Right, 'C');
	    MAP(XK_Left, 'D');
	    MAP(XK_End, 'F');
	    MAP(XK_Clear, 'J');
	    MAP(XK_Delete, 'P');
	    MAP(XK_Insert, 'Q');
	    MAP(XK_Next, 'S');
	    MAP(XK_Prior, 'T');
	    MAP(XK_Home, 'h');
#ifdef XK_KP_Insert
	    MAP(XK_KP_Delete, 'P');
	    MAP(XK_KP_Insert, 'Q');
#endif
#ifdef DXK_Remove
	    MAP(DXK_Remove, 'P');
#endif
	    MAP(XK_Select, 'F');
	    MAP(XK_Find, 'h');
	default:
	    result = -1;
	    break;
	}
    }
    if (result > 0) {
	reply->a_type = ANSI_ESC;
	reply->a_final = (Char) result;
    }
#else
    (void) reply;
    (void) kd;
#endif /* OPT_HP_FUNC_KEYS */
}

static void
scofuncvalue(ANSI *reply, KEY_DATA * kd)
{
#if OPT_SCO_FUNC_KEYS
    int result;

    if (kd->is_fkey) {
	switch (kd->keysym) {
	    MAP(XK_Fn(1), 'M');
	    MAP(XK_Fn(2), 'N');
	    MAP(XK_Fn(3), 'O');
	    MAP(XK_Fn(4), 'P');
	    MAP(XK_Fn(5), 'Q');
	    MAP(XK_Fn(6), 'R');
	    MAP(XK_Fn(7), 'S');
	    MAP(XK_Fn(8), 'T');
	    MAP(XK_Fn(9), 'U');
	    MAP(XK_Fn(10), 'V');
	    MAP(XK_Fn(11), 'W');
	    MAP(XK_Fn(12), 'X');
	    MAP(XK_Fn(13), 'Y');
	    MAP(XK_Fn(14), 'Z');
	    MAP(XK_Fn(15), 'a');
	    MAP(XK_Fn(16), 'b');
	    MAP(XK_Fn(17), 'c');
	    MAP(XK_Fn(18), 'd');
	    MAP(XK_Fn(19), 'e');
	    MAP(XK_Fn(20), 'f');
	    MAP(XK_Fn(21), 'g');
	    MAP(XK_Fn(22), 'h');
	    MAP(XK_Fn(23), 'i');
	    MAP(XK_Fn(24), 'j');
	    MAP(XK_Fn(25), 'k');
	    MAP(XK_Fn(26), 'l');
	    MAP(XK_Fn(27), 'm');
	    MAP(XK_Fn(28), 'n');
	    MAP(XK_Fn(29), 'o');
	    MAP(XK_Fn(30), 'p');
	    MAP(XK_Fn(31), 'q');
	    MAP(XK_Fn(32), 'r');
	    MAP(XK_Fn(33), 's');
	    MAP(XK_Fn(34), 't');
	    MAP(XK_Fn(35), 'u');
	    MAP(XK_Fn(36), 'v');
	    MAP(XK_Fn(37), 'w');
	    MAP(XK_Fn(38), 'x');
	    MAP(XK_Fn(39), 'y');
	    MAP(XK_Fn(40), 'z');
	    MAP(XK_Fn(41), '@');
	    MAP(XK_Fn(42), '[');
	    MAP(XK_Fn(43), '\\');
	    MAP(XK_Fn(44), ']');
	    MAP(XK_Fn(45), '^');
	    MAP(XK_Fn(46), '_');
	    MAP(XK_Fn(47), '`');
	    MAP(XK_Fn(48), L_CURL);
	default:
	    result = -1;
	    break;
	}
    } else {
	switch (kd->keysym) {
	    MAP(XK_Up, 'A');
	    MAP(XK_Down, 'B');
	    MAP(XK_Right, 'C');
	    MAP(XK_Left, 'D');
	    MAP(XK_Begin, 'E');
	    MAP(XK_End, 'F');
	    MAP(XK_Insert, 'L');
	    MAP(XK_Next, 'G');
	    MAP(XK_Prior, 'I');
	    MAP(XK_Home, 'H');
#ifdef XK_KP_Insert
	    MAP(XK_KP_Insert, 'L');
#endif
	default:
	    result = -1;
	    break;
	}
    }
    if (result > 0) {
	reply->a_type = ANSI_CSI;
	reply->a_final = (Char) result;
    }
#else
    (void) reply;
    (void) kd;
#endif /* OPT_SCO_FUNC_KEYS */
}

static void
sunfuncvalue(ANSI *reply, KEY_DATA * kd)
{
#if OPT_SUN_FUNC_KEYS
    ParmType result;

    if (kd->is_fkey) {
	switch (kd->keysym) {
	    /* kf1-kf20 are numbered sequentially */
	    MAP(XK_Fn(1), 224);
	    MAP(XK_Fn(2), 225);
	    MAP(XK_Fn(3), 226);
	    MAP(XK_Fn(4), 227);
	    MAP(XK_Fn(5), 228);
	    MAP(XK_Fn(6), 229);
	    MAP(XK_Fn(7), 230);
	    MAP(XK_Fn(8), 231);
	    MAP(XK_Fn(9), 232);
	    MAP(XK_Fn(10), 233);
	    MAP(XK_Fn(11), 192);
	    MAP(XK_Fn(12), 193);
	    MAP(XK_Fn(13), 194);
	    MAP(XK_Fn(14), 195);	/* kund */
	    MAP(XK_Fn(15), 196);
	    MAP(XK_Fn(16), 197);	/* kcpy */
	    MAP(XK_Fn(17), 198);
	    MAP(XK_Fn(18), 199);
	    MAP(XK_Fn(19), 200);	/* kfnd */
	    MAP(XK_Fn(20), 201);

	    /* kf31-kf36 are numbered sequentially */
	    MAP(XK_Fn(21), 208);	/* kf31 */
	    MAP(XK_Fn(22), 209);
	    MAP(XK_Fn(23), 210);
	    MAP(XK_Fn(24), 211);
	    MAP(XK_Fn(25), 212);
	    MAP(XK_Fn(26), 213);	/* kf36 */

	    /* kf37-kf47 are interspersed with keypad keys */
	    MAP(XK_Fn(27), 214);	/* khome */
	    MAP(XK_Fn(28), 215);	/* kf38 */
	    MAP(XK_Fn(29), 216);	/* kpp */
	    MAP(XK_Fn(30), 217);	/* kf40 */
	    MAP(XK_Fn(31), 218);	/* kb2 */
	    MAP(XK_Fn(32), 219);	/* kf42 */
	    MAP(XK_Fn(33), 220);	/* kend */
	    MAP(XK_Fn(34), 221);	/* kf44 */
	    MAP(XK_Fn(35), 222);	/* knp */
	    MAP(XK_Fn(36), 234);	/* kf46 */
	    MAP(XK_Fn(37), 235);	/* kf47 */
	default:
	    result = -1;
	    break;
	}
    } else {
	switch (kd->keysym) {
	    MAP(XK_Help, 196);	/* khlp */
	    MAP(XK_Menu, 197);

	    MAP(XK_Find, 1);
	    MAP(XK_Insert, 2);	/* kich1 */
	    MAP(XK_Delete, 3);
#ifdef XK_KP_Insert
	    MAP(XK_KP_Insert, 2);
	    MAP(XK_KP_Delete, 3);
#endif
#ifdef DXK_Remove
	    MAP(DXK_Remove, 3);
#endif
	    MAP(XK_Select, 4);

	    MAP(XK_Prior, 216);
	    MAP(XK_Next, 222);
	    MAP(XK_Home, 214);
	    MAP(XK_End, 220);
	    MAP(XK_Begin, 218);	/* kf41=kb2 */

	default:
	    result = -1;
	    break;
	}
    }
    if (result > 0) {
	reply->a_type = ANSI_CSI;
	reply->a_nparam = 1;
	reply->a_param[0] = result;
	reply->a_final = 'z';
    } else if (IsCursorKey(kd->keysym)) {
	reply->a_type = ANSI_SS3;
	reply->a_final = (Char) curfinal[kd->keysym - XK_Home];
    }
#else
    (void) reply;
    (void) kd;
#endif /* OPT_SUN_FUNC_KEYS */
}

#if OPT_NUM_LOCK
#define isName(c) ((c) == '_' || (c) == '-' || isalnum(CharOf(c)))

static const char *
skipName(const char *s)
{
    while (*s != '\0' && isName(CharOf(*s)))
	++s;
    return s;
}

/*
 * Found a ":" in a translation, check what is past it to see if it contains
 * any of the insert-text action names.
 */
static Boolean
keyCanInsert(const char *parse)
{
    Boolean result = False;
    Boolean escape = False;
    Boolean quoted = False;

    static const char *const table[] =
    {
	"insert",
	"insert-seven-bit",
	"insert-eight-bit",
	"string",
    };
    Cardinal n;

    while (*parse != '\0' && *parse != '\n') {
	int ch = CharOf(*parse++);
	if (escape) {
	    escape = False;
	} else if (ch == '\\') {
	    escape = True;
	} else if (ch == '"') {
	    quoted = (Boolean) !quoted;
	} else if (!quoted && isName(ch)) {
	    const char *next = skipName(--parse);
	    size_t need = (size_t) (next - parse);

	    for (n = 0; n < XtNumber(table); ++n) {
		if (need == strlen(table[n])
		    && !strncmp(parse, table[n], need)) {
		    result = True;
		    break;
		}
	    }
	    parse = next;
	}

    }
    return result;
}

/*
 * Strip the entire action, to avoid matching it.
 */
static char *
stripAction(char *base, char *last)
{
    while (last != base) {
	if (*--last == '\n') {
	    break;
	}
    }
    return last;
}

static char *
stripBlanks(char *base, char *last)
{
    while (last != base) {
	int ch = CharOf(last[-1]);
	if (ch != ' ' && ch != '\t')
	    break;
	--last;
    }
    return last;
}

/*
 * Strip unneeded whitespace from a translations resource, mono-casing and
 * returning a malloc'd copy of the result.
 */
static char *
stripTranslations(const char *s, Bool onlyInsert)
{
    char *dst = 0;

    if (s != 0) {
	dst = TypeMallocN(char, strlen(s) + 1);

	if (dst != 0) {
	    int state = 0;
	    int prv = 0;
	    char *d = dst;

	    TRACE(("stripping:\n%s\n", s));
	    while (*s != '\0') {
		int ch = *s++;
		if (ch == '\n') {
		    if (d != dst)
			*d++ = (char) ch;
		    state = 0;
		} else if (strchr(":!#", ch) != 0) {
		    d = stripBlanks(dst, d);
		    if (onlyInsert && (ch == ':') && !keyCanInsert(s)) {
			d = stripAction(dst, d);
		    }
		    state = -1;
		} else if (state >= 0) {
		    if (isspace(CharOf(ch))) {
			if (state == 0 || strchr("<>~ \t", prv))
			    continue;
		    } else if (strchr("<>~", ch)) {
			d = stripBlanks(dst, d);
		    }
		    *d++ = x_toupper(ch);
		    ++state;
		}
		prv = ch;
	    }
	    *d = '\0';
	    TRACE(("...result:\n%s\n", dst));
	}
    }
    return dst;
}

/*
 * Make a simple check to see if a given translations keyword appears in
 * xterm's translations resource.  It does not attempt to parse the strings,
 * just makes a case-independent check and ensures that the ends of the match
 * are on token-boundaries.
 *
 * That this can only retrieve translations that are given as resource values;
 * the default translations in charproc.c for example are not retrievable by
 * any interface to X.
 *
 * Also:  We can retrieve only the most-specified translation resource.  For
 * example, if the resource file specifies both "*translations" and
 * "XTerm*translations", we see only the latter.
 */
static Bool
TranslationsUseKeyword(Widget w, char **cache, const char *keyword, Bool onlyInsert)
{
    Bool result = False;
    char *copy;
    char *test;

    if ((test = stripTranslations(keyword, onlyInsert)) != 0) {
	if (*cache == 0) {
	    String data = 0;
	    getKeymapResources(w, "vt100", "VT100", XtRString, &data, sizeof(data));
	    if (data != 0 && (copy = stripTranslations(data, onlyInsert)) != 0) {
		*cache = copy;
	    }
	}

	if (*cache != 0) {
	    char *p = *cache;
	    int state = 0;
	    int now = ' ';

	    while (*p != 0) {
		int prv = now;
		now = *p++;
		if (now == ':'
		    || now == '!') {
		    state = -1;
		} else if (now == '\n') {
		    state = 0;
		} else if (state >= 0) {
		    if (now == test[state]) {
			if ((state != 0
			     || !isName(prv))
			    && ((test[++state] == 0)
				&& !isName(*p))) {
			    result = True;
			    break;
			}
		    } else {
			state = 0;
		    }
		}
	    }
	}
	free(test);
    }
    TRACE(("TranslationsUseKeyword(%p, %s) = %d\n",
	   (void *) w, keyword, result));
    return result;
}

static Bool
xtermHasTranslation(XtermWidget xw, const char *keyword, Bool onlyInsert)
{
    return (TranslationsUseKeyword(SHELL_OF(xw),
				   &(xw->keyboard.shell_translations),
				   keyword,
				   onlyInsert)
	    || TranslationsUseKeyword((Widget) xw,
				      &(xw->keyboard.xterm_translations),
				      keyword,
				      onlyInsert));
}

#if OPT_EXTRA_PASTE
static void
addTranslation(XtermWidget xw, const char *fromString, const char *toString)
{
    size_t have = (xw->keyboard.extra_translations
		   ? strlen(xw->keyboard.extra_translations)
		   : 0);
    size_t need = (((have != 0) ? (have + 4) : 0)
		   + strlen(fromString)
		   + strlen(toString)
		   + 6);

    if (!xtermHasTranslation(xw, fromString, False)) {
	xw->keyboard.extra_translations
	    = TypeRealloc(char, need, xw->keyboard.extra_translations);
	if ((xw->keyboard.extra_translations) != 0) {
	    TRACE(("adding %s: %s\n", fromString, toString));
	    if (have)
		strcat(xw->keyboard.extra_translations, " \\n\\");
	    sprintf(xw->keyboard.extra_translations, "%s: %s",
		    fromString, toString);
	    TRACE(("...{%s}\n", xw->keyboard.extra_translations));
	}
    }
}
#endif

#define SaveMask(name)	xw->work.name |= (unsigned) mask;\
			TRACE(("SaveMask(%#x -> %s) %#x (%#x is%s modifier)\n", \
				(unsigned) keysym, #name, \
				xw->work.name, (unsigned) mask, \
				ModifierName((unsigned) mask)));
/*
 * Determine which modifier mask (if any) applies to the Num_Lock keysym.
 *
 * Also, determine which modifiers are associated with the ALT keys, so we can
 * send that information as a parameter for special keys in Sun/PC keyboard
 * mode.  However, if the ALT modifier is used in translations, we do not want
 * to confuse things by sending the parameter.
 */
void
VTInitModifiers(XtermWidget xw)
{
    Display *dpy = XtDisplay(xw);
    XModifierKeymap *keymap = XGetModifierMapping(dpy);
    KeySym keysym;
    int min_keycode, max_keycode, keysyms_per_keycode = 0;

    if (keymap != 0) {
	KeySym *theMap;
	int keycode_count;

	TRACE(("VTInitModifiers\n"));

	XDisplayKeycodes(dpy, &min_keycode, &max_keycode);
	keycode_count = (max_keycode - min_keycode + 1);
	theMap = XGetKeyboardMapping(dpy,
				     (KeyCode) min_keycode,
				     keycode_count,
				     &keysyms_per_keycode);

	if (theMap != 0) {
	    int i, j, k, l;
	    unsigned long mask;

#if OPT_EXTRA_PASTE
	    /*
	     * Assume that if we can find the paste keysym in the X keyboard
	     * mapping that the server allows the corresponding translations
	     * resource.
	     */
	    int limit = (max_keycode - min_keycode) * keysyms_per_keycode;
	    for (i = 0; i < limit; ++i) {
#ifdef XF86XK_Paste
		if (theMap[i] == XF86XK_Paste) {
		    TRACE(("keyboard has XF86XK_Paste\n"));
		    addTranslation(xw,
				   ":<KeyPress> XF86Paste",
				   "insert-selection(SELECT, CUT_BUFFER0)");
		}
#endif
#ifdef SunXK_Paste
		if (theMap[i] == SunXK_Paste) {
		    TRACE(("keyboard has SunXK_Paste\n"));
		    addTranslation(xw,
				   ":<KeyPress> SunPaste",
				   "insert-selection(SELECT, CUT_BUFFER0)");
		}
#endif
	    }
#endif /* OPT_EXTRA_PASTE */

	    for (i = k = 0, mask = 1; i < 8; i++, mask <<= 1) {
		for (j = 0; j < keymap->max_keypermod; j++) {
		    KeyCode code = keymap->modifiermap[k++];
		    if (code == 0)
			continue;

		    for (l = 0; l < keysyms_per_keycode; ++l) {
#ifdef HAVE_XKBKEYCODETOKEYSYM
			keysym = XkbKeycodeToKeysym(dpy, code, 0, l);
#else
			keysym = XKeycodeToKeysym(dpy, code, l);
#endif
			if (keysym == NoSymbol) {
			    /* EMPTY */ ;
			} else if (keysym == XK_Num_Lock) {
			    SaveMask(num_lock);
			} else if (keysym == XK_Alt_L || keysym == XK_Alt_R) {
			    SaveMask(alt_mods);
			} else if (keysym == XK_Meta_L || keysym == XK_Meta_R) {
			    SaveMask(meta_mods);
			}
		    }
		}
	    }
	    XFree(theMap);
	}

	/* Don't disable any mods if "alwaysUseMods" is true. */
	if (!xw->misc.alwaysUseMods) {

	    /*
	     * Force TranslationsUseKeyword() to reload.
	     */
	    if (xw->keyboard.shell_translations) {
		free(xw->keyboard.shell_translations);
		xw->keyboard.shell_translations = 0;
	    }
	    if (xw->keyboard.xterm_translations) {
		free(xw->keyboard.xterm_translations);
		xw->keyboard.xterm_translations = 0;
	    }

	    /*
	     * If the Alt modifier is used in translations, we would rather not
	     * use it to modify function-keys when NumLock is active.
	     */
	    if ((xw->work.alt_mods != 0)
		&& xtermHasTranslation(xw, "alt", True)) {
		TRACE(("ALT is used as a modifier in translations (ignore mask)\n"));
		xw->work.alt_mods = 0;
	    }

	    /*
	     * If the Meta modifier is used in translations, we would rather not
	     * use it to modify function-keys.
	     */
	    if ((xw->work.meta_mods != 0)
		&& xtermHasTranslation(xw, "meta", True)) {
		TRACE(("META is used as a modifier in translations\n"));
		xw->work.meta_mods = 0;
	    }
	}

	XFreeModifiermap(keymap);
    }
}
#endif /* OPT_NUM_LOCK */
