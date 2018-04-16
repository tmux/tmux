/* $XTermId: xtermcap.c,v 1.49 2016/05/22 18:31:20 tom Exp $ */

/*
 * Copyright 2007-2014,2016 by Thomas E. Dickey
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
 */

#include <xtermcap.h>
#include <data.h>

#include <X11/keysym.h>
#include <ctype.h>

#ifdef VMS
#include <X11/keysymdef.h>
#endif

#include <xstrings.h>

#if USE_TERMINFO && defined(NCURSES_VERSION) && defined(HAVE_USE_EXTENDED_NAMES)
#define USE_EXTENDED_NAMES 1
#else
#define USE_EXTENDED_NAMES 0
#endif

#if USE_TERMINFO
#define TcapInit(buffer, name) (setupterm(name, fileno(stdout), &ignored) == OK)
#else
#define TcapInit(buffer, name) (tgetent(buffer, name) == 1)
#endif

#define NO_STRING (char *)(-1)

#if OPT_TCAP_QUERY || OPT_TCAP_FKEYS

#define SHIFT (MOD_NONE + MOD_SHIFT)

typedef struct {
    const char *tc;
    const char *ti;
    int code;
    unsigned param;		/* see xtermStateToParam() */
} TCAPINFO;
/* *INDENT-OFF* */
#define DATA(tc,ti,x,y) { tc, ti, x, y }
static const TCAPINFO table[] = {
	/*	tcap	terminfo	code		state */
	DATA(	"%1",	"khlp",		XK_Help,	0	),
	DATA(	"#1",	"kHLP",		XK_Help,	SHIFT	),
	DATA(	"@0",	"kfnd",		XK_Find,	0	),
	DATA(	"*0",	"kFND",		XK_Find,	SHIFT	),
	DATA(	"*6",	"kslt",		XK_Select,	0	),
	DATA(	"#6",	"kSLT",		XK_Select,	SHIFT	),

	DATA(	"kh",	"khome",	XK_Home,	0	),
	DATA(	"#2",	"kHOM",		XK_Home,	SHIFT	),
	DATA(	"@7",	"kend",		XK_End,		0	),
	DATA(	"*7",	"kEND",		XK_End,		SHIFT	),

	DATA(	"kl",	"kcub1",	XK_Left,	0	),
	DATA(	"kr",	"kcuf1",	XK_Right,	0	),
	DATA(	"ku",	"kcuu1",	XK_Up,		0	),
	DATA(	"kd",	"kcud1",	XK_Down,	0	),

	DATA(	"#4",	"kLFT",		XK_Left,	SHIFT	),
	DATA(	"%i",	"kRIT",		XK_Right,	SHIFT	),
	DATA(	"kF",	"kind",		XK_Down,	SHIFT	),
	DATA(	"kR",	"kri",		XK_Up,		SHIFT	),

	DATA(	"k1",	"kf1",		XK_Fn(1),	0	),
	DATA(	"k2",	"kf2",		XK_Fn(2),	0	),
	DATA(	"k3",	"kf3",		XK_Fn(3),	0	),
	DATA(	"k4",	"kf4",		XK_Fn(4),	0	),
	DATA(	"k5",	"kf5",		XK_Fn(5),	0	),
	DATA(	"k6",	"kf6",		XK_Fn(6),	0	),
	DATA(	"k7",	"kf7",		XK_Fn(7),	0	),
	DATA(	"k8",	"kf8",		XK_Fn(8),	0	),
	DATA(	"k9",	"kf9",		XK_Fn(9),	0	),
	DATA(	"k;",	"kf10",		XK_Fn(10),	0	),

	DATA(	"F1",	"kf11",		XK_Fn(11),	0	),
	DATA(	"F2",	"kf12",		XK_Fn(12),	0	),
	DATA(	"F3",	"kf13",		XK_Fn(13),	0	),
	DATA(	"F4",	"kf14",		XK_Fn(14),	0	),
	DATA(	"F5",	"kf15",		XK_Fn(15),	0	),
	DATA(	"F6",	"kf16",		XK_Fn(16),	0	),
	DATA(	"F7",	"kf17",		XK_Fn(17),	0	),
	DATA(	"F8",	"kf18",		XK_Fn(18),	0	),
	DATA(	"F9",	"kf19",		XK_Fn(19),	0	),
	DATA(	"FA",	"kf20",		XK_Fn(20),	0	),
	DATA(	"FB",	"kf21",		XK_Fn(21),	0	),
	DATA(	"FC",	"kf22",		XK_Fn(22),	0	),
	DATA(	"FD",	"kf23",		XK_Fn(23),	0	),
	DATA(	"FE",	"kf24",		XK_Fn(24),	0	),
	DATA(	"FF",	"kf25",		XK_Fn(25),	0	),
	DATA(	"FG",	"kf26",		XK_Fn(26),	0	),
	DATA(	"FH",	"kf27",		XK_Fn(27),	0	),
	DATA(	"FI",	"kf28",		XK_Fn(28),	0	),
	DATA(	"FJ",	"kf29",		XK_Fn(29),	0	),
	DATA(	"FK",	"kf30",		XK_Fn(30),	0	),
	DATA(	"FL",	"kf31",		XK_Fn(31),	0	),
	DATA(	"FM",	"kf32",		XK_Fn(32),	0	),
	DATA(	"FN",	"kf33",		XK_Fn(33),	0	),
	DATA(	"FO",	"kf34",		XK_Fn(34),	0	),
	DATA(	"FP",	"kf35",		XK_Fn(35),	0	),

	DATA(	"FQ",	"kf36",		-36,		0	),
	DATA(	"FR",	"kf37",		-37,		0	),
	DATA(	"FS",	"kf38",		-38,		0	),
	DATA(	"FT",	"kf39",		-39,		0	),
	DATA(	"FU",	"kf40",		-40,		0	),
	DATA(	"FV",	"kf41",		-41,		0	),
	DATA(	"FW",	"kf42",		-42,		0	),
	DATA(	"FX",	"kf43",		-43,		0	),
	DATA(	"FY",	"kf44",		-44,		0	),
	DATA(	"FZ",	"kf45",		-45,		0	),
	DATA(	"Fa",	"kf46",		-46,		0	),
	DATA(	"Fb",	"kf47",		-47,		0	),
	DATA(	"Fc",	"kf48",		-48,		0	),
	DATA(	"Fd",	"kf49",		-49,		0	),
	DATA(	"Fe",	"kf50",		-50,		0	),
	DATA(	"Ff",	"kf51",		-51,		0	),
	DATA(	"Fg",	"kf52",		-52,		0	),
	DATA(	"Fh",	"kf53",		-53,		0	),
	DATA(	"Fi",	"kf54",		-54,		0	),
	DATA(	"Fj",	"kf55",		-55,		0	),
	DATA(	"Fk",	"kf56",		-56,		0	),
	DATA(	"Fl",	"kf57",		-57,		0	),
	DATA(	"Fm",	"kf58",		-58,		0	),
	DATA(	"Fn",	"kf59",		-59,		0	),
	DATA(	"Fo",	"kf60",		-60,		0	),
	DATA(	"Fp",	"kf61",		-61,		0	),
	DATA(	"Fq",	"kf62",		-62,		0	),
	DATA(	"Fr",	"kf63",		-63,		0	),

	DATA(	"K1",	"ka1",		XK_KP_Home,	0	),
	DATA(	"K4",	"kc1",		XK_KP_End,	0	),
	DATA(	"K3",	"ka3",		XK_KP_Prior,	0	),
	DATA(	"K5",	"kc3",		XK_KP_Next,	0	),

#ifdef XK_ISO_Left_Tab
	DATA(	"kB",	"kcbt",		XK_ISO_Left_Tab, 0	),
#endif
	DATA(	"kC",	"kclr",		XK_Clear,	0	),
	DATA(	"kD",	"kdch1",	XK_Delete,	0	),
	DATA(	"kI",	"kich1",	XK_Insert,	0	),

	DATA(	"kN",	"knp",		XK_Next,	0	),
	DATA(	"kP",	"kpp",		XK_Prior,	0	),
	DATA(	"%c",	"kNXT",		XK_Next,	SHIFT	),
	DATA(	"%e",	"kPRV",		XK_Prior,	SHIFT	),

	DATA(	"&8",	"kund",		XK_Undo,	0	),
	DATA(	"kb",	"kbs",		XK_BackSpace,	0	),
# if OPT_TCAP_QUERY && OPT_ISO_COLORS
	/* XK_COLORS is a fake code. */
	DATA(	"Co",	"colors",	XK_COLORS,	0	),
# endif
	DATA(	"TN",	"name",		XK_TCAPNAME,	0	),
#if USE_EXTENDED_NAMES
#define DEXT(name, parm, code) DATA("", name, code, parm)
#define D1ST(name, parm, code) DEXT("k" #name, parm, code)
#define DMOD(name, parm, code) DEXT("k" #name #parm, parm, code)

#define DGRP(name, code) \
	D1ST(name, 2, code), \
	DMOD(name, 3, code), \
	DMOD(name, 4, code), \
	DMOD(name, 5, code), \
	DMOD(name, 6, code), \
	DMOD(name, 7, code), \
	DMOD(name, 8, code)

	/* the terminfo codes here are ncurses extensions */
	/* ignore the termcap names, which are empty */
	DATA(	"",	"kUP",		XK_Up,		SHIFT	),
	DATA(	"",	"kDN",		XK_Up,		SHIFT	),

	DGRP(DN,   XK_Down),
	DGRP(LFT,  XK_Left),
	DGRP(RIT,  XK_Right),
	DGRP(UP,   XK_Up),
	DGRP(DC,   XK_Delete),
	DGRP(END,  XK_End),
	DGRP(HOM,  XK_Home),
	DGRP(IC,   XK_Insert),
	DGRP(NXT,  XK_Next),
	DGRP(PRV,  XK_Prior),
#endif
};
#undef DATA
/* *INDENT-ON* */

#if OPT_TCAP_FKEYS
static Boolean
loadTermcapStrings(TScreen *screen)
{
    Boolean result = True;

    if (screen->tcap_fkeys == 0) {
	Cardinal want = XtNumber(table);
	Cardinal have;
#ifdef USE_TERMCAP
	char *area = screen->tcap_area;
#endif

	TRACE(("loadTermcapStrings\n"));
	if ((screen->tcap_fkeys = TypeCallocN(char *, want)) != 0) {

	    for (have = 0; have < want; ++have) {
		char name[80];
		char *fkey;

#ifndef USE_TERMCAP
		fkey = tigetstr(strcpy(name, table[have].ti));
#else
		fkey = tgetstr(strcpy(name, table[have].tc), &area);
#endif
		if (fkey != 0 && fkey != NO_STRING) {
		    screen->tcap_fkeys[have] = x_strdup(fkey);
		} else {
		    screen->tcap_fkeys[have] = NO_STRING;
		}
	    }
	} else {
	    result = False;
	}
    }
    return result;
}
#endif

#if OPT_TCAP_QUERY
static Boolean
keyIsDistinct(XtermWidget xw, int which)
{
    Boolean result = True;

    switch (xw->keyboard.type) {
    case keyboardIsTermcap:
#if OPT_TCAP_FKEYS
	if (table[which].param == SHIFT) {
	    TScreen *screen = TScreenOf(xw);
	    Cardinal k;

	    if (loadTermcapStrings(screen)
		&& screen->tcap_fkeys[which] != NO_STRING) {

		for (k = 0; k < XtNumber(table); k++) {

		    if (table[k].code == table[which].code
			&& table[k].param == 0) {
			char *fkey;

			if ((fkey = screen->tcap_fkeys[k]) != NO_STRING
			    && !strcmp(fkey, screen->tcap_fkeys[which])) {
			    TRACE(("shifted/unshifted keys do not differ\n"));
			    result = False;
			}
			break;
		    }
		}
	    } else {
		/* there is no data for the shifted key */
		result = -1;
	    }
	}
#endif
	break;
	/*
	 * The vt220-keyboard will not return distinct key sequences for
	 * shifted cursor-keys.  Just pretend they do not exist, since some
	 * programs may be confused if we return the same data for
	 * shifted/unshifted keys.
	 */
    case keyboardIsVT220:
	if (table[which].param == SHIFT) {
	    TRACE(("shifted/unshifted keys do not differ\n"));
	    result = False;
	}
	break;
    case keyboardIsLegacy:
    case keyboardIsDefault:
    case keyboardIsHP:
    case keyboardIsSCO:
    case keyboardIsSun:
	break;
    }

    return result;
}

static int
lookupTcapByName(const char *name)
{
    int result = -2;
    Cardinal j;

    if (!IsEmpty(name)) {
	for (j = 0; j < XtNumber(table); j++) {
	    if (!strcmp(table[j].ti, name) || !strcmp(table[j].tc, name)) {
		result = (int) j;
		break;
	    }
	}
    }

    if (result >= 0) {
	TRACE(("lookupTcapByName(%s) tc=%s, ti=%s code %#x, param %#x\n",
	       name,
	       table[result].tc,
	       table[result].ti,
	       table[result].code,
	       table[result].param));
    } else {
	TRACE(("lookupTcapByName(%s) FAIL\n", name));
    }
    return result;
}

/*
 * Parse the termcap/terminfo name from the string, returning a positive number
 * (the keysym) if found, otherwise -1.  Update the string pointer.
 * Returns the (shift, control) state in *state.
 *
 * This does not attempt to construct control/shift modifiers to construct
 * function-key values.  Instead, it sets the *fkey flag to pass to Input()
 * and bypass the lookup of keysym altogether.
 */
int
xtermcapKeycode(XtermWidget xw, const char **params, unsigned *state, Bool *fkey)
{
    const TCAPINFO *data;
    int code = -1;
    char *name;
    const char *p;

    TRACE(("xtermcapKeycode(%s)\n", *params));

    /* Convert hex encoded name to ascii */
    name = x_decode_hex(*params, &p);
    *params = p;

    *state = 0;
    *fkey = False;

    if (!IsEmpty(name) && (*p == 0 || *p == ';')) {
	int which;

	if ((which = lookupTcapByName(name)) >= 0) {
	    if (keyIsDistinct(xw, which)) {
		data = table + which;
		code = data->code;
		*state = xtermParamToState(xw, data->param);
		if (IsFunctionKey(code)) {
		    *fkey = True;
		} else if (code < 0) {
		    *fkey = True;
		    code = XK_Fn((-code));
		}
#if OPT_SUN_FUNC_KEYS
		if (*fkey && xw->keyboard.type == keyboardIsSun) {
		    int num = code - XK_Fn(0);

		    /* match function-key case in sunfuncvalue() */
		    if (num > 20) {
			if (num <= 30 || num > 47) {
			    code = -1;
			} else {
			    code -= 10;
			    switch (num) {
			    case 37:	/* khome */
			    case 39:	/* kpp */
			    case 41:	/* kb2 */
			    case 43:	/* kend */
			    case 45:	/* knp */
				code = -1;
				break;
			    }
			}
		    }
		}
#endif
	    } else {
		TRACE(("... name ok, data not ok\n"));
		code = -1;
	    }
	} else {
	    TRACE(("... name not ok\n"));
	    code = -2;
	}
    } else {
	TRACE(("... name not ok\n"));
	code = -2;
    }

    TRACE(("... xtermcapKeycode(%s, %u, %d) -> %#06x\n",
	   name, *state, *fkey, code));
    free(name);
    return code;
}
#endif /* OPT_TCAP_QUERY */

#if OPT_TCAP_FKEYS
static int
nextTcapByCode(int code, unsigned param, int last)
{
    int result = -1;
    int n;

    TRACE(("lookupTcapByCode %#x:%#x\n", code, param));
    for (n = last + 1; n < (int) XtNumber(table); n++) {
	if (table[n].code == code &&
	    table[n].param == param) {
	    TRACE(("->lookupTcapByCode %d:%s\n", n, table[n].ti));
	    result = n;
	    break;
	}
    }
    return result;
}

static int
firstTcapByCode(int code, unsigned param)
{
    return nextTcapByCode(code, param, -1);
}

int
xtermcapString(XtermWidget xw, int keycode, unsigned mask)
{
    int result = 0;
    unsigned param = xtermStateToParam(xw, mask);
    int which;

    if ((which = firstTcapByCode(keycode, param)) >= 0) {
	TScreen *screen = TScreenOf(xw);

	if (loadTermcapStrings(screen)) {
	    do {
		char *fkey;

		if ((fkey = screen->tcap_fkeys[which]) != NO_STRING) {
		    StringInput(xw, (Char *) fkey, strlen(fkey));
		    result = 1;
		    break;
		}
	    } while ((which = nextTcapByCode(keycode, param, which)) >= 0);
	}
    }

    TRACE(("xtermcapString(keycode=%#x, mask=%#x) ->%d\n",
	   keycode, mask, result));

    return result;
}
#endif /* OPT_TCAP_FKEYS */

#endif /* OPT_TCAP_QUERY || OPT_TCAP_FKEYS */

/*
 * If we're linked to terminfo, tgetent() will return an empty buffer.  We
 * cannot use that to adjust the $TERMCAP variable.
 */
Bool
get_termcap(XtermWidget xw, char *name)
{
#if USE_TERMINFO
    int ignored = 0;
#endif
    char *buffer = get_tcap_buffer(xw);

    *buffer = 0;		/* initialize, in case we're using terminfo's tgetent */

#if USE_EXTENDED_NAMES
    use_extended_names(TRUE);
#endif
    if (!IsEmpty(name)) {
	if (TcapInit(buffer, name)) {
	    TRACE(("get_termcap(%s) succeeded (%s)\n", name,
		   (*buffer
		    ? "ok:termcap, we can update $TERMCAP"
		    : "assuming this is terminfo")));
	    return True;
	} else {
	    *buffer = 0;	/* just in case */
	}
    }
    return False;
}

/*
 * Retrieve the termcap-buffer.
 */
char *
get_tcap_buffer(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    char *buffer;

#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	buffer = TekScreenOf(tekWidget)->tcapbuf;
    } else
#endif
    {
	buffer = screen->tcapbuf;
    }
    return buffer;
}

/*
 * Retrieve the erase-key, for initialization in main program.
 */
char *
get_tcap_erase(XtermWidget xw GCC_UNUSED)
{
#ifdef USE_TERMCAP
    char *area = TScreenOf(xw)->tcap_area;
#endif
    char *fkey;

#ifndef USE_TERMCAP
    fkey = tigetstr("kbs");
#else
    fkey = tgetstr("kb", &area);
#endif

    if (fkey == NO_STRING)
	fkey = 0;
    if (fkey != 0)
	fkey = x_strdup(fkey);
    return fkey;
}

/*
 * A legal termcap (or terminfo) name consists solely of graphic characters,
 * excluding the punctuation used to delimit fields of the source description.
 */
static Bool
isLegalTcapName(const char *name)
{
    Bool result = False;

    if (*name != '\0') {
	result = True;
	while (*name != '\0') {
	    if (isgraph(CharOf(*name))) {
		if (strchr("\\|,:'\"", *name) != 0) {
		    result = False;
		    break;
		}
	    } else {
		result = False;
		break;
	    }
	    ++name;
	}
    }

    return result;
}

void
set_termcap(XtermWidget xw, const char *name)
{
    Boolean success = False;
#if USE_TERMINFO
    int ignored = 0;
#else
    TScreen *screen = TScreenOf(xw);
    char buffer[sizeof(screen->tcapbuf)];
#endif

    TRACE(("set_termcap(%s)\n", NonNull(name)));
    if (IsEmpty(name)) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	const char *temp;
	char *value;

	if ((value = x_decode_hex(name, &temp)) != 0) {
	    if (*temp == '\0' && isLegalTcapName(value)) {
		if (TcapInit(buffer, value)) {
#if !USE_TERMINFO
		    memcpy(screen->tcapbuf, buffer, sizeof(buffer));
#endif
		    free_termcap(xw);
		    success = True;
		}
	    }
	    free(value);
	}
    }
    if (!success)
	Bell(xw, XkbBI_MinorError, 0);
}

void
free_termcap(XtermWidget xw)
{
#if OPT_TCAP_FKEYS
    TScreen *screen = TScreenOf(xw);

    if (screen->tcap_fkeys != 0) {
	Cardinal want = XtNumber(table);
	Cardinal have;

	for (have = 0; have < want; ++have) {
	    char *fkey = screen->tcap_fkeys[have];
	    if (fkey != 0 && fkey != NO_STRING) {
		free(fkey);
	    }
	}
	free(screen->tcap_fkeys);
	screen->tcap_fkeys = 0;
    }
#endif
}
