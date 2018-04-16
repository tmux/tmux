/* $XTermId: html.c,v 1.11 2017/12/30 14:46:50 tom Exp $ */

/*
 * Copyright 2015,2017	Jens Schweikhardt
 * Copyright 2017	Thomas E. Dickey
 *
 * All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written
 * authorization.
 */

#include <xterm.h>
#include <version.h>

#define MakeDim(color) \
	color = (unsigned short) ((2 * (unsigned) color) / 3)

#define RGBPCT(c) \
 	((double)c.red   / 655.35), \
	((double)c.green / 655.35), \
	((double)c.blue  / 655.35)

#define DUMP_PREFIX "xterm"
#define DUMP_SUFFIX ".xhtml"
#define DEFAULTNAME DUMP_PREFIX DUMP_SUFFIX

#ifdef VMS
#define VMS_HTML_FILE "sys$scratch:" DEFAULTNAME
#endif

static void dumpHtmlHeader(XtermWidget xw, FILE *fp);
static void dumpHtmlScreen(XtermWidget xw, FILE *fp);
static void dumpHtmlLine(XtermWidget xw, int row, FILE *fp);
static void dumpHtmlFooter(XtermWidget, FILE *fp);
static void writeStyle(XtermWidget, FILE *fp);
char *PixelToCSSColor(XtermWidget xw, Pixel p);

void
xtermDumpHtml(XtermWidget xw)
{
    FILE *fp;

    TRACE(("xtermDumpHtml...\n"));
#ifdef VMS
    fp = fopen(VMS_HTML_FILE, "wb");
#elif defined(HAVE_STRFTIME)
    {
	char fname[sizeof(DEFAULTNAME) + LEN_TIMESTAMP];
	time_t now;
	struct tm *ltm;

	now = time((time_t *) 0);
	ltm = localtime(&now);

	if (strftime(fname, sizeof fname,
		     DUMP_PREFIX FMT_TIMESTAMP DUMP_SUFFIX, ltm) > 0) {
	    fp = fopen(fname, "wb");
	} else {
	    fp = fopen(DEFAULTNAME, "wb");
	}
    }
#else
    fp = fopen(DEFAULTNAME, "wb");
#endif

    if (fp != 0) {
	dumpHtmlHeader(xw, fp);
	dumpHtmlScreen(xw, fp);
	dumpHtmlFooter(xw, fp);
	fclose(fp);
    }
    TRACE(("...xtermDumpHtml done\n"));
}

static void
dumpHtmlHeader(XtermWidget xw, FILE *fp)
{
    fputs("<?xml version='1.0' encoding='UTF-8'?>\n", fp);
    fputs("<!DOCTYPE html PUBLIC '-//W3C//DTD XHTML 1.0 Strict//EN'\n", fp);
    fputs("  'http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd'\n", fp);
    fputs("  [<!ENTITY s \"&#160;\">]>\n", fp);
    fputs("<html xmlns='http://www.w3.org/1999/xhtml' lang='en' xml:lang='en'>\n", fp);
    fputs(" <head>\n", fp);
    fprintf(fp, "  <meta name='generator' content='%s'/>\n", xtermVersion());
    fputs("  <meta http-equiv='Content-Type' content='text/html; charset=UTF-8'/>\n", fp);
    fputs("  <link rel='Stylesheet' type='text/css' href='xterm.css'/>\n", fp);
    fputs("  <title>Xterm</title>\n", fp);
    writeStyle(xw, fp);
    fputs(" </head>\n", fp);
    fputs(" <body>\n", fp);
    fputs("  <div id='vt100'>\n", fp);
    fputs("   <pre>", fp);
}

static void
writeStyle(XtermWidget xw, FILE *fp)
{
    TScreen *s = TScreenOf(xw);

    fputs("  <style type='text/css'>\n", fp);
    fputs("  body, pre { margin: 0 }\n", fp);
    fputs("  #vt100 {\n", fp);
    fputs("    float: left;\n", fp);
    fprintf(fp, "    font-size: 12pt;\n");
    fprintf(fp, "    border: %upx solid %s;\n", BorderWidth(xw),
	    PixelToCSSColor(xw, BorderPixel(xw)));
    fprintf(fp, "    padding: %dpx;\n", s->border);
    fprintf(fp, "    background: %s\n", PixelToCSSColor(xw, xw->old_background));
    fprintf(fp, "  }\n");
    fputs("  .ul { text-decoration: underline }\n", fp);
    fputs("  .bd { font-weight: bold }\n", fp);
    fputs("  .it { font-style: italic }\n", fp);
    fputs("  .st { text-decoration: line-through }\n", fp);
    fputs("  .lu { text-decoration: line-through underline }\n", fp);
    fputs("  </style>\n", fp);
}

static void
dumpHtmlScreen(XtermWidget xw, FILE *fp)
{
    TScreen *s = TScreenOf(xw);
    int row;

    for (row = s->top_marg; row <= s->bot_marg; ++row) {
	dumpHtmlLine(xw, row, fp);
    }
}

/*
 * Note: initial and final space around values of class and style
 *       attribute are deliberate. They make it easier for XPath
 *       to test whether a particular name is among the attributes.
 *       It allows expressions such as
 *           [contains(@class, ' ul ')]
 *       instead of the unwieldy
 *           [contains(concat(' ', @class, ' '), ' ul ')]
 *       The ev and od (for even and odd rows) values
 *       avoid empty values when going back to old fg/bg.
 */
static void
dumpHtmlLine(XtermWidget xw, int row, FILE *fp)
{
    TScreen *s = TScreenOf(xw);
    char attrs[2][sizeof
		  "<span class=' ev ul bd it st du ' style='color: rgb(100.00%, 100.00%, 100.00%); background: rgb(100.00%, 100.00%, 100.00%)'>"];
    int attr_index = 0;
    char *attr = &attrs[attr_index][0];
    int inx = ROW2INX(s, row);
    LineData *ld = getLineData(s, inx);
    int col;

    if (ld == 0)
	return;

    for (col = 0; col < MaxCols(s); col++) {
	XColor fgcolor, bgcolor;
	IChar chr = ld->charData[col];
	int slen = 0;

	fgcolor.pixel = xw->old_foreground;
	bgcolor.pixel = xw->old_background;
#if OPT_ISO_COLORS
	if (ld->attribs[col] & FG_COLOR) {
	    Pixel fg = extract_fg(xw, ld->color[col], ld->attribs[col]);
#if OPT_DIRECT_COLOR
	    if (ld->attribs[col] & ATR_DIRECT_FG)
		fgcolor.pixel = fg;
	    else
#endif
		fgcolor.pixel = s->Acolors[fg].value;
	}
	if (ld->attribs[col] & BG_COLOR) {
	    Pixel bg = extract_bg(xw, ld->color[col], ld->attribs[col]);
#if OPT_DIRECT_COLOR
	    if (ld->attribs[col] & ATR_DIRECT_BG)
		bgcolor.pixel = bg;
	    else
#endif
		bgcolor.pixel = s->Acolors[bg].value;
	}
#endif

	XQueryColor(xw->screen.display, xw->core.colormap, &fgcolor);
	XQueryColor(xw->screen.display, xw->core.colormap, &bgcolor);
	if (ld->attribs[col] & BLINK) {
	    /* White on red. */
	    fgcolor.red = fgcolor.green = fgcolor.blue = 65535u;
	    bgcolor.red = 65535u;
	    bgcolor.green = bgcolor.blue = 0u;
	}
#if OPT_WIDE_ATTRS
	if (ld->attribs[col] & ATR_FAINT) {
	    MakeDim(fgcolor.red);
	    MakeDim(fgcolor.green);
	    MakeDim(fgcolor.blue);
	}
#endif
	if (ld->attribs[col] & INVERSE) {
	    XColor tmp = fgcolor;
	    fgcolor = bgcolor;
	    bgcolor = tmp;
	}

	slen = sprintf(attr + slen, "<span class=' %s",
		       ((row % 2) ? "ev" : "od"));
	if (ld->attribs[col] & BOLD)
	    slen += sprintf(attr + slen, " bd");
#if OPT_WIDE_ATTRS
	/*
	 * Handle multiple text-decoration properties.
	 * Treat ATR_DBL_UNDER the same as UNDERLINE since there is no
	 * official proper CSS 2.2 way to use double underlining. (E.g.
	 * using border-bottom does not work for successive lines and
	 * "text-decoration: underline double" is a browser extension).
	 */
	if ((ld->attribs[col] & (UNDERLINE | ATR_DBL_UNDER)) &&
	    (ld->attribs[col] & ATR_STRIKEOUT))
	    slen += sprintf(attr + slen, " lu");
	else if (ld->attribs[col] & (UNDERLINE | ATR_DBL_UNDER))
	    slen += sprintf(attr + slen, " ul");
	else if (ld->attribs[col] & ATR_STRIKEOUT)
	    slen += sprintf(attr + slen, " st");

	if (ld->attribs[col] & ATR_ITALIC)
	    slen += sprintf(attr + slen, " it");
#else
	if (ld->attribs[col] & UNDERLINE)
	    slen += sprintf(attr + slen, " ul");
#endif
	slen += sprintf(attr + slen,
			" ' style='color: rgb(%.2f%%, %.2f%%, %.2f%%);",
			RGBPCT(fgcolor));
	(void) sprintf(attr + slen,
		       " background: rgb(%.2f%%, %.2f%%, %.2f%%)'>", RGBPCT(bgcolor));
	if (col == 0) {
	    fputs(attr, fp);
	    attr = &attrs[attr_index ^= 1][0];
	} else {
	    if (strcmp(&attrs[0][0], &attrs[1][0])) {
		fputs("</span>", fp);
		fputs(attr, fp);
		attr = &attrs[attr_index ^= 1][0];
	    }
	}

#if OPT_WIDE_CHARS
	if (chr > 127) {
	    /* Ignore hidden characters. */
	    if (chr != HIDDEN_CHAR) {
		Char temp[10];
		*convertToUTF8(temp, chr) = 0;
		fputs((char *) temp, fp);
	    }
	} else
#endif
	    switch (chr) {
	    case 0:
		/* This sometimes happens when resizing... ignore. */
		break;
	    case '&':
		fputs("&amp;", fp);
		break;
	    case '<':
		fputs("&lt;", fp);
		break;
	    case '>':
		fputs("&gt;", fp);
		break;
	    case ' ':
		fputs("&s;", fp);
		break;
	    default:
		fputc((int) chr, fp);
	    }
    }
    fprintf(fp, "</span>\n");
}

static void
dumpHtmlFooter(XtermWidget xw GCC_UNUSED, FILE *fp)
{
    fputs("</pre>\n", fp);
    fputs("  </div>\n", fp);
    fputs(" </body>\n", fp);
    fputs("</html>\n", fp);
}

char *
PixelToCSSColor(XtermWidget xw, Pixel p)
{
    static char rgb[sizeof "rgb(100.00%, 100.00%, 100.00%)"];
    XColor c;

    c.pixel = p;
    XQueryColor(xw->screen.display, xw->core.colormap, &c);
    sprintf(rgb, "rgb(%.2f%%, %.2f%%, %.2f%%)", RGBPCT(c));
    return rgb;
}
/* vim: set ts=8 sw=4 et: */
