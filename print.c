/* $XTermId: print.c,v 1.166 2017/12/19 23:47:15 tom Exp $ */

/*
 * Copyright 1997-2016,2017 by Thomas E. Dickey
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

#include <xterm.h>
#include <data.h>
#include <menu.h>
#include <error.h>
#include <xstrings.h>

#include <stdio.h>
#include <sys/stat.h>

#undef  CTRL
#define	CTRL(c)	((c) & 0x1f)

#define SHIFT_IN  '\017'
#define SHIFT_OUT '\016'

#define CSET_IN   'A'
#define CSET_OUT  '0'

#define isForm(c)      ((c) == '\r' || (c) == '\n' || (c) == '\f')
#define Strlen(a)      strlen((const char *)a)
#define Strcmp(a,b)    strcmp((const char *)a,(const char *)b)
#define Strncmp(a,b,c) strncmp((const char *)a,(const char *)b,c)

#define SPS PrinterOf(screen)

#ifdef VMS
#define VMS_TEMP_PRINT_FILE "sys$scratch:xterm_print.txt"
#endif

static void charToPrinter(XtermWidget /* xw */ ,
			  unsigned /* chr */ );
static void printLine(XtermWidget /* xw */ ,
		      int /* row */ ,
		      unsigned /* chr */ ,
		      PrinterFlags * /* p */ );
static void send_CharSet(XtermWidget /* xw */ ,
			 LineData * /* ld */ );
static void send_SGR(XtermWidget /* xw */ ,
		     unsigned /* attr */ ,
		     unsigned /* fg */ ,
		     unsigned /* bg */ );
static void stringToPrinter(XtermWidget /* xw */ ,
			    const char * /*str */ );

static void
closePrinter(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    if (SPS.fp != 0) {
	if (SPS.toFile) {
	    fclose(SPS.fp);
	    SPS.fp = 0;
	} else if (xtermHasPrinter(xw) != 0) {
#ifdef VMS
	    char pcommand[256];
	    (void) sprintf(pcommand, "%s %s;",
			   SPS.printer_command,
			   VMS_TEMP_PRINT_FILE);
#endif

	    DEBUG_MSG("closePrinter\n");
	    pclose(SPS.fp);
	    TRACE(("closed printer, waiting...\n"));
#ifdef VMS			/* This is a quick hack, really should use
				   spawn and check status or system services
				   and go straight to the queue */
	    (void) system(pcommand);
#else /* VMS */
	    while (nonblocking_wait() > 0) {
		;
	    }
#endif /* VMS */
	    SPS.fp = 0;
	    SPS.isOpen = False;
	    TRACE(("closed printer\n"));
	    DEBUG_MSG("...closePrinter (done)\n");
	}
    }
}

static void
printCursorLine(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("printCursorLine\n"));
    printLine(xw, screen->cur_row, '\n', getPrinterFlags(xw, NULL, 0));
}

/*
 * DEC's manual doesn't document whether trailing blanks are removed, or what
 * happens with a line that is entirely blank.  This function prints the
 * characters that xterm would allow as a selection (which may include blanks).
 */
static void
printLine(XtermWidget xw, int row, unsigned chr, PrinterFlags *p)
{
    TScreen *screen = TScreenOf(xw);
    int inx = ROW2INX(screen, row);
    LineData *ld;
    int last = MaxCols(screen);
#if OPT_ISO_COLORS && OPT_PRINT_COLORS
#define ColorOf(ld,col) (ld->color[col])
#endif
    Pixel fg = NO_COLOR;
    Pixel bg = NO_COLOR;
#if OPT_PRINT_COLORS
    Pixel last_fg = NO_COLOR;
    Pixel last_bg = NO_COLOR;
#endif

    ld = getLineData(screen, inx);
    if (ld == 0)
	return;

    TRACE(("printLine(row=%d/%d, top=%d:%d, chr=%d):%s\n",
	   row, ROW2INX(screen, row), screen->topline, screen->max_row, chr,
	   visibleIChars(ld->charData, (unsigned) last)));

    while (last > 0) {
	if ((ld->attribs[last - 1] & CHARDRAWN) == 0)
	    last--;
	else
	    break;
    }

    if (last) {
	int col;
	int cs = CSET_IN;
	int last_cs = CSET_IN;

	if (p->print_attributes) {
	    send_CharSet(xw, ld);
	    send_SGR(xw, 0, NO_COLOR, NO_COLOR);
	}
	for (col = 0; col < last; col++) {
	    IAttr attr = 0;
	    unsigned ch = ld->charData[col];
#if OPT_PRINT_COLORS
	    if (screen->colorMode) {
		if (p->print_attributes > 1) {
		    fg = (ld->attribs[col] & FG_COLOR)
			? extract_fg(xw, ColorOf(ld, col), ld->attribs[col])
			: NO_COLOR;
		    bg = (ld->attribs[col] & BG_COLOR)
			? extract_bg(xw, ColorOf(ld, col), ld->attribs[col])
			: NO_COLOR;
		}
	    }
#endif
	    if ((((ld->attribs[col] & ATTRIBUTES) != attr)
#if OPT_PRINT_COLORS
		 || (last_fg != fg) || (last_bg != bg)
#endif
		)
		&& ch) {
		attr = (IAttr) (ld->attribs[col] & ATTRIBUTES);
#if OPT_PRINT_COLORS
		last_fg = fg;
		last_bg = bg;
#endif
		if (p->print_attributes)
		    send_SGR(xw, attr, (unsigned) fg, (unsigned) bg);
	    }

	    if (ch == 0)
		ch = ' ';

#if OPT_WIDE_CHARS
	    if (screen->utf8_mode)
		cs = CSET_IN;
	    else
#endif
		cs = (ch >= ' ' && ch != ANSI_DEL) ? CSET_IN : CSET_OUT;
	    if (last_cs != cs) {
		if (p->print_attributes) {
		    charToPrinter(xw,
				  (unsigned) ((cs == CSET_OUT)
					      ? SHIFT_OUT
					      : SHIFT_IN));
		}
		last_cs = cs;
	    }

	    /* FIXME:  we shouldn't have to map back from the
	     * alternate character set, except that the
	     * corresponding charset information is not encoded
	     * into the CSETS array.
	     */
	    charToPrinter(xw,
			  ((cs == CSET_OUT)
			   ? (ch == ANSI_DEL ? 0x5f : (ch + 0x5f))
			   : ch));
	    if_OPT_WIDE_CHARS(screen, {
		size_t off;
		for_each_combData(off, ld) {
		    ch = ld->combData[off][col];
		    if (ch == 0)
			break;
		    charToPrinter(xw, ch);
		}
	    });
	}
	if (p->print_attributes) {
	    send_SGR(xw, 0, NO_COLOR, NO_COLOR);
	    if (cs != CSET_IN)
		charToPrinter(xw, SHIFT_IN);
	}
    }

    /* finish line (protocol for attributes needs a CR */
    if (p->print_attributes)
	charToPrinter(xw, '\r');

    if (chr && !(p->printer_newline)) {
	if (LineTstWrapped(ld))
	    chr = '\0';
    }

    if (chr)
	charToPrinter(xw, chr);

    return;
}

#define PrintNewLine() (unsigned) (((top < bot) || p->printer_newline) ? '\n' : '\0')

static void
printLines(XtermWidget xw, int top, int bot, PrinterFlags *p)
{
    TRACE(("printLines, rows %d..%d\n", top, bot));
    while (top <= bot) {
	printLine(xw, top, PrintNewLine(), p);
	++top;
    }
}

void
xtermPrintScreen(XtermWidget xw, Bool use_DECPEX, PrinterFlags *p)
{
    if (XtIsRealized((Widget) xw)) {
	TScreen *screen = TScreenOf(xw);
	Bool extent = (use_DECPEX && p->printer_extent);
	Boolean was_open = SPS.isOpen;

	printLines(xw,
		   extent ? 0 : screen->top_marg,
		   extent ? screen->max_row : screen->bot_marg,
		   p);
	if (p->printer_formfeed)
	    charToPrinter(xw, '\f');

	if (!was_open || SPS.printer_autoclose) {
	    closePrinter(xw);
	}
    } else {
	Bell(xw, XkbBI_MinorError, 0);
    }
}

/*
 * If p->print_everything is zero, use this behavior:
 * If the alternate screen is active, we'll print only that.  Otherwise, print
 * the normal screen plus all scrolled-back lines.  The distinction is made
 * because the normal screen's buffer is part of the overall scrollback buffer.
 *
 * Otherwise, decode bits:
 *	1 = current screen
 *	2 = normal screen
 *	4 = alternate screen
 *	8 = saved lines
 */
void
xtermPrintEverything(XtermWidget xw, PrinterFlags *p)
{
    TScreen *screen = TScreenOf(xw);
    Boolean was_open = SPS.isOpen;
    int save_which = screen->whichBuf;

    DEBUG_MSG("xtermPrintEverything\n");

    if (p->print_everything) {
	int done_which = 0;

	if (p->print_everything & 8) {
	    printLines(xw, -screen->savedlines, -(screen->topline + 1), p);
	}
	if (p->print_everything & 4) {
	    screen->whichBuf = 1;
	    done_which |= 2;
	    printLines(xw, 0, screen->max_row, p);
	    screen->whichBuf = save_which;
	}
	if (p->print_everything & 2) {
	    screen->whichBuf = 0;
	    done_which |= 1;
	    printLines(xw, 0, screen->max_row, p);
	    screen->whichBuf = save_which;
	}
	if (p->print_everything & 1) {
	    if (!(done_which & (1 << screen->whichBuf))) {
		printLines(xw, 0, screen->max_row, p);
	    }
	}
    } else {
	int top = 0;
	int bot = screen->max_row;
	if (!screen->whichBuf) {
	    top = -screen->savedlines - screen->topline;
	    bot -= screen->topline;
	}
	printLines(xw, top, bot, p);
    }
    if (p->printer_formfeed)
	charToPrinter(xw, '\f');

    if (!was_open || SPS.printer_autoclose) {
	closePrinter(xw);
    }
}

static void
send_CharSet(XtermWidget xw, LineData *ld)
{
#if OPT_DEC_CHRSET
    const char *msg = 0;

    switch (GetLineDblCS(ld)) {
    case CSET_SWL:
	msg = "\033#5";
	break;
    case CSET_DHL_TOP:
	msg = "\033#3";
	break;
    case CSET_DHL_BOT:
	msg = "\033#4";
	break;
    case CSET_DWL:
	msg = "\033#6";
	break;
    }
    if (msg != 0)
	stringToPrinter(xw, msg);
#else
    (void) xw;
    (void) ld;
#endif /* OPT_DEC_CHRSET */
}

static void
send_SGR(XtermWidget xw, unsigned attr, unsigned fg, unsigned bg)
{
    char msg[80];

#if OPT_ISO_COLORS && OPT_PC_COLORS
    if ((attr & FG_COLOR) && (fg != NO_COLOR)) {
	if (TScreenOf(xw)->boldColors
	    && fg > 8
	    && (attr & BOLD) != 0)
	    fg -= 8;
    }
#endif
    strcpy(msg, "\033[");
    xtermFormatSGR(xw, msg + strlen(msg), attr, (int) fg, (int) bg);
    strcat(msg, "m");
    stringToPrinter(xw, msg);
}

/*
 * This implementation only knows how to write to a pipe.
 */
static void
charToPrinter(XtermWidget xw, unsigned chr)
{
    TScreen *screen = TScreenOf(xw);

    if (!SPS.isOpen && (SPS.toFile || xtermHasPrinter(xw))) {
	switch (SPS.toFile) {
	    /*
	     * write to a pipe.
	     */
	case False:
#ifdef VMS
	    /*
	     * This implementation only knows how to write to a file.  When the
	     * file is closed the print command executes.  Print command must
	     * be of the form:
	     *   print/que=name/delete [/otherflags].
	     */
	    SPS.fp = fopen(VMS_TEMP_PRINT_FILE, "w");
#else
	    {
		int my_pipe[2];
		pid_t my_pid;

		if (pipe(my_pipe))
		    SysError(ERROR_FORK);
		if ((my_pid = fork()) < 0)
		    SysError(ERROR_FORK);

		if (my_pid == 0) {
		    DEBUG_MSG("charToPrinter: subprocess for printer\n");
		    TRACE_CLOSE();
		    close(my_pipe[1]);	/* printer is silent */
		    close(screen->respond);

		    close(fileno(stdout));
		    dup2(fileno(stderr), 1);

		    if (fileno(stderr) != 2) {
			dup2(fileno(stderr), 2);
			close(fileno(stderr));
		    }

		    /* don't want privileges! */
		    if (xtermResetIds(screen) < 0)
			exit(1);

		    SPS.fp = popen(SPS.printer_command, "w");
		    if (SPS.fp != 0) {
			FILE *input;
			DEBUG_MSG("charToPrinter: opened pipe to printer\n");
			if ((input = fdopen(my_pipe[0], "r")) != 0) {
			    clearerr(input);

			    for (;;) {
				int c;

				if (ferror(input)) {
				    DEBUG_MSG("charToPrinter: break on ferror\n");
				    break;
				} else if (feof(input)) {
				    DEBUG_MSG("charToPrinter: break on feof\n");
				    break;
				} else if ((c = fgetc(input)) == EOF) {
				    DEBUG_MSG("charToPrinter: break on EOF\n");
				    break;
				}
				fputc(c, SPS.fp);
				if (isForm(c))
				    fflush(SPS.fp);
			    }
			}
			DEBUG_MSG("charToPrinter: calling pclose\n");
			pclose(SPS.fp);
			if (input)
			    fclose(input);
		    }
		    exit(0);
		} else {
		    close(my_pipe[0]);	/* won't read from printer */
		    if ((SPS.fp = fdopen(my_pipe[1], "w")) != 0) {
			DEBUG_MSG("charToPrinter: opened printer in parent\n");
			TRACE(("opened printer from pid %d/%d\n",
			       (int) getpid(), (int) my_pid));
		    } else {
			TRACE(("failed to open printer:%s\n", strerror(errno)));
			DEBUG_MSG("charToPrinter: could not open in parent\n");
		    }
		}
	    }
#endif
	    break;
	case True:
	    TRACE(("opening \"%s\" as printer output\n", SPS.printer_command));
	    SPS.fp = fopen(SPS.printer_command, "w");
	    break;
	}
	SPS.isOpen = True;
    }
    if (SPS.fp != 0) {
#if OPT_WIDE_CHARS
	if (chr > 127) {
	    Char temp[10];
	    *convertToUTF8(temp, chr) = 0;
	    fputs((char *) temp, SPS.fp);
	} else
#endif
	    fputc((int) chr, SPS.fp);
	if (isForm(chr))
	    fflush(SPS.fp);
    }
}

static void
stringToPrinter(XtermWidget xw, const char *str)
{
    while (*str)
	charToPrinter(xw, CharOf(*str++));
}

/*
 * This module implements the MC (Media Copy) and related printing control
 * sequences for VTxxx emulation.  This is based on the description in the
 * VT330/VT340 Programmer Reference Manual EK-VT3XX-TP-001 (Digital Equipment
 * Corp., March 1987).
 */
void
xtermMediaControl(XtermWidget xw, int param, int private_seq)
{
    TRACE(("MediaCopy param=%d, private=%d\n", param, private_seq));

    if (private_seq) {
	switch (param) {
	case 1:
	    printCursorLine(xw);
	    break;
	case 4:
	    setPrinterControlMode(xw, 0);
	    break;
	case 5:
	    setPrinterControlMode(xw, 1);
	    break;
	case 10:		/* VT320 */
	    xtermPrintScreen(xw, False, getPrinterFlags(xw, NULL, 0));
	    break;
	case 11:		/* VT320 */
	    xtermPrintEverything(xw, getPrinterFlags(xw, NULL, 0));
	    break;
	}
    } else {
	switch (param) {
	case -1:
	case 0:
	    xtermPrintScreen(xw, True, getPrinterFlags(xw, NULL, 0));
	    break;
	case 4:
	    setPrinterControlMode(xw, 0);
	    break;
	case 5:
	    setPrinterControlMode(xw, 2);
	    break;
#if OPT_SCREEN_DUMPS
	case 10:
	    xtermDumpHtml(xw);
	    break;
	case 11:
	    xtermDumpSvg(xw);
	    break;
#endif
	}
    }
}

/*
 * When in autoprint mode, the printer prints a line from the screen when you
 * move the cursor off that line with an LF, FF, or VT character, or an
 * autowrap occurs.  The printed line ends with a CR and the character (LF, FF
 * or VT) that moved the cursor off the previous line.
 */
void
xtermAutoPrint(XtermWidget xw, unsigned chr)
{
    TScreen *screen = TScreenOf(xw);

    if (SPS.printer_controlmode == 1) {
	TRACE(("AutoPrint %d\n", chr));
	printLine(xw, screen->cursorp.row, chr, getPrinterFlags(xw, NULL, 0));
	if (SPS.fp != 0)
	    fflush(SPS.fp);
    }
}

/*
 * When in printer controller mode, the terminal sends received characters to
 * the printer without displaying them on the screen. The terminal sends all
 * characters and control sequences to the printer, except NUL, XON, XOFF, and
 * the printer controller sequences.
 *
 * This function eats characters, returning 0 as long as it must buffer or
 * divert to the printer.  We're only invoked here when in printer controller
 * mode, and handle the exit from that mode.
 */
#define LB '['

int
xtermPrinterControl(XtermWidget xw, int chr)
{
    TScreen *screen = TScreenOf(xw);
    /* *INDENT-OFF* */
    static const struct {
	const Char seq[5];
	int active;
    } tbl[] = {
	{ { ANSI_CSI, '5', 'i'      }, 2 },
	{ { ANSI_CSI, '4', 'i'      }, 0 },
	{ { ANSI_ESC, LB,  '5', 'i' }, 2 },
	{ { ANSI_ESC, LB,  '4', 'i' }, 0 },
    };
    /* *INDENT-ON* */

    static Char bfr[10];
    static size_t length;
    size_t n;

    TRACE(("In printer:%04X\n", chr));

    switch (chr) {
    case 0:
    case CTRL('Q'):
    case CTRL('S'):
	return 0;		/* ignored by application */

    case ANSI_CSI:
    case ANSI_ESC:
    case '[':
    case '4':
    case '5':
    case 'i':
	bfr[length++] = CharOf(chr);
	for (n = 0; n < sizeof(tbl) / sizeof(tbl[0]); n++) {
	    size_t len = Strlen(tbl[n].seq);

	    if (length == len
		&& Strcmp(bfr, tbl[n].seq) == 0) {
		setPrinterControlMode(xw, tbl[n].active);
		if (SPS.printer_autoclose
		    && SPS.printer_controlmode == 0)
		    closePrinter(xw);
		length = 0;
		return 0;
	    } else if (len > length
		       && Strncmp(bfr, tbl[n].seq, length) == 0) {
		return 0;
	    }
	}
	length--;

	/* FALLTHRU */

    default:
	for (n = 0; n < length; n++)
	    charToPrinter(xw, bfr[n]);
	bfr[0] = CharOf(chr);
	length = 1;
	return 0;
    }
}

/*
 * If there is no printer command, we will ignore printer controls.
 *
 * If we do have a printer command, we still have to verify that it will
 * (perhaps) work if we pass it to popen().  At a minimum, the program
 * must exist and be executable.  If not, warn and disable the feature.
 */
Bool
xtermHasPrinter(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    Bool result = SPS.printer_checked;

    if (strlen(SPS.printer_command) != 0 && !result) {
	char **argv = x_splitargs(SPS.printer_command);
	if (argv) {
	    if (argv[0]) {
		char *myShell = xtermFindShell(argv[0], False);
		if (myShell == 0) {
		    xtermWarning("No program found for printerCommand: %s\n", SPS.printer_command);
		    SPS.printer_command = x_strdup("");
		} else {
		    free(myShell);
		    SPS.printer_checked = True;
		    result = True;
		}
	    }
	    x_freeargs(argv);
	}
	TRACE(("xtermHasPrinter:%d\n", result));
    }

    return result;
}

#define showPrinterControlMode(mode) \
		(((mode) == 0) \
		 ? "normal" \
		 : ((mode) == 1 \
		    ? "autoprint" \
		    : "printer controller"))

void
setPrinterControlMode(XtermWidget xw, int mode)
{
    TScreen *screen = TScreenOf(xw);

    if (xtermHasPrinter(xw)
	&& SPS.printer_controlmode != mode) {
	TRACE(("%s %s mode\n",
	       (mode
		? "set"
		: "reset"),
	       (mode
		? showPrinterControlMode(mode)
		: showPrinterControlMode(SPS.printer_controlmode))));
	SPS.printer_controlmode = mode;
	update_print_redir();
    }
}

PrinterFlags *
getPrinterFlags(XtermWidget xw, String *params, Cardinal *param_count)
{
    /* *INDENT-OFF* */
    static const struct {
	const char *name;
	unsigned    offset;
	int	    value;
    } table[] = {
	{ "noFormFeed", XtOffsetOf(PrinterFlags, printer_formfeed), 0 },
	{ "FormFeed",	XtOffsetOf(PrinterFlags, printer_formfeed), 1 },
	{ "noNewLine",	XtOffsetOf(PrinterFlags, printer_newline),  0 },
	{ "NewLine",	XtOffsetOf(PrinterFlags, printer_newline),  1 },
	{ "noAttrs",	XtOffsetOf(PrinterFlags, print_attributes), 0 },
	{ "monoAttrs",	XtOffsetOf(PrinterFlags, print_attributes), 1 },
	{ "colorAttrs", XtOffsetOf(PrinterFlags, print_attributes), 2 },
    };
    /* *INDENT-ON* */

    TScreen *screen = TScreenOf(xw);
    PrinterFlags *result = &(screen->printer_flags);

    TRACE(("getPrinterFlags %d params\n", param_count ? *param_count : 0));

    result->printer_extent = SPS.printer_extent;
    result->printer_formfeed = SPS.printer_formfeed;
    result->printer_newline = SPS.printer_newline;
    result->print_attributes = SPS.print_attributes;
    result->print_everything = SPS.print_everything;

    if (param_count != 0 && *param_count != 0) {
	Cardinal j;
	unsigned k;
	for (j = 0; j < *param_count; ++j) {
	    TRACE(("param%d:%s\n", j, params[j]));
	    for (k = 0; k < XtNumber(table); ++k) {
		if (!x_strcasecmp(params[j], table[k].name)) {
		    int *ptr = (int *) (void *) ((char *) result + table[k].offset);
		    TRACE(("...PrinterFlags(%s) %d->%d\n",
			   table[k].name,
			   *ptr,
			   table[k].value));
		    *ptr = table[k].value;
		    break;
		}
	    }
	}
    }

    return result;
}

/*
 * Print a timestamped copy of everything.
 */
void
xtermPrintImmediately(XtermWidget xw, String filename, int opts, int attrs)
{
    TScreen *screen = TScreenOf(xw);
    PrinterState save_state = screen->printer_state;
    char *my_filename = malloc(TIMESTAMP_LEN + strlen(filename));

    if (my_filename != 0) {
	mode_t save_umask = umask(0177);

	timestamp_filename(my_filename, filename);
	SPS.fp = 0;
	SPS.isOpen = False;
	SPS.toFile = True;
	SPS.printer_command = my_filename;
	SPS.printer_autoclose = True;
	SPS.printer_formfeed = False;
	SPS.printer_newline = True;
	SPS.print_attributes = attrs;
	SPS.print_everything = opts;
	xtermPrintEverything(xw, getPrinterFlags(xw, NULL, 0));

	umask(save_umask);
	screen->printer_state = save_state;
    }
}

void
xtermPrintOnXError(XtermWidget xw, int n)
{
#if OPT_PRINT_ON_EXIT
    /*
     * The user may have requested that the contents of the screen will be
     * written to a file if an X error occurs.
     */
    if (TScreenOf(xw)->write_error && !IsEmpty(resource.printFileOnXError)) {
	Boolean printIt = False;

	switch (n) {
	case ERROR_XERROR:
	    /* FALLTHRU */
	case ERROR_XIOERROR:
	    /* FALLTHRU */
	case ERROR_ICEERROR:
	    printIt = True;
	    break;
	}

	if (printIt) {
	    xtermPrintImmediately(xw,
				  resource.printFileOnXError,
				  resource.printOptsOnXError,
				  resource.printModeOnXError);
	}
    }
#else
    (void) xw;
    (void) n;
#endif
}
