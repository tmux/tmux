/* $XTermId: resize.c,v 1.139 2017/05/31 08:58:56 tom Exp $ */

/*
 * Copyright 2003-2015,2017 by Thomas E. Dickey
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

/* resize.c */

#include <stdio.h>
#include <ctype.h>

#include <xterm.h>
#include <version.h>
#include <xstrings.h>
#include <xtermcap.h>
#include <xterm_io.h>

#ifndef USE_TERMINFO		/* avoid conflict with configure script */
#if defined(__QNX__) || defined(__SCO__) || defined(linux) || defined(__OpenBSD__) || defined(__UNIXWARE__)
#define USE_TERMINFO
#endif
#endif

#if defined(__QNX__)
#include <unix.h>
#endif

/*
 * Some OS's may want to use both, like SCO for example.  We catch here anyone
 * who hasn't decided what they want.
 */
#if !defined(USE_TERMCAP) && !defined(USE_TERMINFO)
#define USE_TERMINFO
#endif

#include <signal.h>
#include <pwd.h>

#ifdef USE_IGNORE_RC
int ignore_unused;
#endif

#ifdef __MVS__
#define ESCAPE(string) "\047" string
#else
#define ESCAPE(string) "\033" string
#endif

#define	EMULATIONS	2
#define	SUN		1
#define	VT100		0

#define	TIMEOUT		10

#define	SHELL_UNKNOWN	0
#define	SHELL_C		1
#define	SHELL_BOURNE	2
/* *INDENT-OFF* */
static struct {
    const char *name;
    int type;
} shell_list[] = {
    { "csh",	SHELL_C },	/* vanilla cshell */
    { "jcsh",   SHELL_C },
    { "tcsh",   SHELL_C },
    { "sh",	SHELL_BOURNE }, /* vanilla Bourne shell */
    { "ash",    SHELL_BOURNE },
    { "bash",	SHELL_BOURNE }, /* GNU Bourne again shell */
    { "dash",	SHELL_BOURNE },
    { "jsh",    SHELL_BOURNE },
    { "ksh",	SHELL_BOURNE }, /* Korn shell (from AT&T toolchest) */
    { "ksh-i",	SHELL_BOURNE }, /* another name for Korn shell */
    { "ksh93",	SHELL_BOURNE }, /* Korn shell */
    { "mksh",   SHELL_BOURNE },
    { "pdksh",  SHELL_BOURNE },
    { "zsh",    SHELL_BOURNE },
    { NULL,	SHELL_BOURNE }	/* default (same as xterm's) */
};
/* *INDENT-ON* */

static const char *const emuname[EMULATIONS] =
{
    "VT100",
    "Sun",
};
static char *myname;
static int shell_type = SHELL_UNKNOWN;
static const char *const getsize[EMULATIONS] =
{
    ESCAPE("7") ESCAPE("[r") ESCAPE("[9999;9999H") ESCAPE("[6n"),
    ESCAPE("[18t"),
};
#if defined(USE_STRUCT_WINSIZE)
static const char *const getwsize[EMULATIONS] =
{				/* size in pixels */
    0,
    ESCAPE("[14t"),
};
#endif /* USE_STRUCT_WINSIZE */
static const char *const restore[EMULATIONS] =
{
    ESCAPE("8"),
    0,
};
static const char *const setsize[EMULATIONS] =
{
    0,
    ESCAPE("[8;%s;%st"),
};

#ifdef USE_ANY_SYSV_TERMIO
static struct termio tioorig;
#elif defined(USE_TERMIOS)
static struct termios tioorig;
#else
static struct sgttyb sgorig;
#endif /* USE_ANY_SYSV_TERMIO/USE_TERMIOS */

static const char *const size[EMULATIONS] =
{
    ESCAPE("[%d;%dR"),
    ESCAPE("[8;%d;%dt"),
};
static const char sunname[] = "sunsize";
static int tty;
static FILE *ttyfp;

#if defined(USE_STRUCT_WINSIZE)
static const char *wsize[EMULATIONS] =
{
    0,
    ESCAPE("[4;%hd;%hdt"),
};
#endif /* USE_STRUCT_WINSIZE */

static void
failed(const char *s)
{
    int save = errno;
    IGNORE_RC(write(2, myname, strlen(myname)));
    IGNORE_RC(write(2, ": ", (size_t) 2));
    errno = save;
    perror(s);
    exit(EXIT_FAILURE);
}

/* ARGSUSED */
static void
onintr(int sig GCC_UNUSED)
{
#ifdef USE_ANY_SYSV_TERMIO
    (void) ioctl(tty, TCSETAW, &tioorig);
#elif defined(USE_TERMIOS)
    (void) tcsetattr(tty, TCSADRAIN, &tioorig);
#else /* not USE_TERMIOS */
    (void) ioctl(tty, TIOCSETP, &sgorig);
#endif /* USE_ANY_SYSV_TERMIO/USE_TERMIOS */
    exit(EXIT_FAILURE);
}

static void
resize_timeout(int sig)
{
    fprintf(stderr, "\n%s: Time out occurred\r\n", myname);
    onintr(sig);
}

static void
Usage(void)
{
    fprintf(stderr, strcmp(myname, sunname) == 0 ?
	    "Usage: %s [rows cols]\n" :
	    "Usage: %s [-v] [-u] [-c] [-s [rows cols]]\n", myname);
    exit(EXIT_FAILURE);
}

#ifdef USE_TERMCAP
static void
print_termcap(const char *termcap)
{
    int ch;

    putchar('\'');
    while ((ch = *termcap++) != '\0') {
	switch (ch & 0xff) {
	case 127:		/* undo bug in GNU termcap */
	    printf("^?");
	    break;
	case '\'':		/* must escape anyway (unlikely) */
	    /* FALLTHRU */
	case '!':		/* must escape for SunOS csh */
	    putchar('\\');
	    /* FALLTHRU */
	default:
	    putchar(ch);
	    break;
	}
    }
    putchar('\'');
}
#endif /* USE_TERMCAP */

static int
checkdigits(char *str)
{
    while (*str) {
	if (!isdigit(CharOf(*str)))
	    return (0);
	str++;
    }
    return (1);
}

static void
readstring(FILE *fp, char *buf, const char *str)
{
    int last, c;
#if !defined(USG) && !defined(__minix)
    /* What is the advantage of setitimer() over alarm()? */
    struct itimerval it;
#endif

    signal(SIGALRM, resize_timeout);
#if defined(USG) || defined(__minix)
    alarm(TIMEOUT);
#else
    memset((char *) &it, 0, sizeof(struct itimerval));
    it.it_value.tv_sec = TIMEOUT;
    setitimer(ITIMER_REAL, &it, (struct itimerval *) NULL);
#endif
    if ((c = getc(fp)) == 0233) {	/* meta-escape, CSI */
	c = ESCAPE("")[0];
	*buf++ = (char) c;
	*buf++ = '[';
    } else {
	*buf++ = (char) c;
    }
    if (c != *str) {
	fprintf(stderr, "%s: unknown character, exiting.\r\n", myname);
	onintr(0);
    }
    last = str[strlen(str) - 1];
    while ((*buf++ = (char) getc(fp)) != last) {
	;
    }
#if defined(USG) || defined(__minix)
    alarm(0);
#else
    memset((char *) &it, 0, sizeof(struct itimerval));
    setitimer(ITIMER_REAL, &it, (struct itimerval *) NULL);
#endif
    *buf = 0;
}

/*
   resets termcap string to reflect current screen size
 */
int
main(int argc, char **argv ENVP_ARG)
{
#ifdef USE_TERMCAP
    char *env;
#endif
    char *ptr;
    int emu = VT100;
    char *shell;
    int i;
    int rc;
    int rows, cols;
#ifdef USE_ANY_SYSV_TERMIO
    struct termio tio;
#elif defined(USE_TERMIOS)
    struct termios tio;
#else
    struct sgttyb sg;
#endif /* USE_ANY_SYSV_TERMIO/USE_TERMIOS */
#ifdef USE_TERMCAP
    int ok_tcap = 1;
    char termcap[TERMCAP_SIZE];
    char newtc[TERMCAP_SIZE];
#endif /* USE_TERMCAP */
    char buf[BUFSIZ];
#ifdef TTYSIZE_STRUCT
    TTYSIZE_STRUCT ts;
#endif
    char *name_of_tty;
#ifdef CANT_OPEN_DEV_TTY
    extern char *ttyname();
#endif
    const char *setname = "";

    myname = x_basename(argv[0]);
    if (strcmp(myname, sunname) == 0)
	emu = SUN;
    for (argv++, argc--; argc > 0 && **argv == '-'; argv++, argc--) {
	switch ((*argv)[1]) {
	case 's':		/* Sun emulation */
	    if (emu == SUN)
		Usage();	/* Never returns */
	    emu = SUN;
	    break;
	case 'u':		/* Bourne (Unix) shell */
	    shell_type = SHELL_BOURNE;
	    break;
	case 'c':		/* C shell */
	    shell_type = SHELL_C;
	    break;
	case 'v':
	    printf("%s\n", xtermVersion());
	    exit(EXIT_SUCCESS);
	default:
	    Usage();		/* Never returns */
	}
    }

    if (SHELL_UNKNOWN == shell_type) {
	/* Find out what kind of shell this user is running.
	 * This is the same algorithm that xterm uses.
	 */
	if ((ptr = x_getenv("SHELL")) == NULL) {
	    uid_t uid = getuid();
	    struct passwd pw;

	    if (x_getpwuid(uid, &pw)) {
		(void) x_getlogin(uid, &pw);
	    }
	    if (!OkPasswd(&pw)
		|| *(ptr = pw.pw_shell) == 0) {
		/* this is the same default that xterm uses */
		ptr = x_strdup("/bin/sh");
	    }
	}

	shell = x_basename(ptr);

	/* now that we know, what kind is it? */
	for (i = 0; shell_list[i].name; i++) {
	    if (!strcmp(shell_list[i].name, shell)) {
		break;
	    }
	}
	shell_type = shell_list[i].type;
    }

    if (argc == 2) {
	if (!setsize[emu]) {
	    fprintf(stderr,
		    "%s: Can't set window size under %s emulation\n",
		    myname, emuname[emu]);
	    exit(EXIT_FAILURE);
	}
	if (!checkdigits(argv[0]) || !checkdigits(argv[1])) {
	    Usage();		/* Never returns */
	}
    } else if (argc != 0) {
	Usage();		/* Never returns */
    }
#ifdef CANT_OPEN_DEV_TTY
    if ((name_of_tty = ttyname(fileno(stderr))) == NULL)
#endif
	name_of_tty = x_strdup("/dev/tty");

    if ((ttyfp = fopen(name_of_tty, "r+")) == NULL) {
	fprintf(stderr, "%s:  can't open terminal %s\n",
		myname, name_of_tty);
	exit(EXIT_FAILURE);
    }
    tty = fileno(ttyfp);
#ifdef USE_TERMCAP
    if ((env = x_getenv("TERM")) == 0) {
	env = DFT_TERMTYPE;
	if (SHELL_BOURNE == shell_type) {
	    setname = "TERM=" DFT_TERMTYPE ";\nexport TERM;\n";
	} else {
	    setname = "setenv TERM " DFT_TERMTYPE ";\n";
	}
    }
    termcap[0] = 0;		/* ...just in case we've accidentally gotten terminfo */
    if (tgetent(termcap, env) <= 0 || termcap[0] == 0) {
	ok_tcap = 0;
    }
#endif /* USE_TERMCAP */
#ifdef USE_TERMINFO
    if (x_getenv("TERM") == 0) {
	if (SHELL_BOURNE == shell_type) {
	    setname = "TERM=" DFT_TERMTYPE ";\nexport TERM;\n";
	} else {
	    setname = "setenv TERM " DFT_TERMTYPE ";\n";
	}
    }
#endif /* USE_TERMINFO */

#ifdef USE_ANY_SYSV_TERMIO
    rc = ioctl(tty, TCGETA, &tioorig);
    tio = tioorig;
    UIntClr(tio.c_iflag, (ICRNL | IUCLC));
    UIntClr(tio.c_lflag, (ICANON | ECHO));
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 6;
    tio.c_cc[VTIME] = 1;
#elif defined(USE_TERMIOS)
    rc = tcgetattr(tty, &tioorig);
    tio = tioorig;
    UIntClr(tio.c_iflag, ICRNL);
    UIntClr(tio.c_lflag, (ICANON | ECHO));
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 6;
    tio.c_cc[VTIME] = 1;
#else /* not USE_TERMIOS */
    rc = ioctl(tty, TIOCGETP, &sgorig);
    sg = sgorig;
    sg.sg_flags |= RAW;
    UIntClr(sg.sg_flags, ECHO);
#endif /* USE_ANY_SYSV_TERMIO/USE_TERMIOS */
    if (rc != 0)
	failed("get tty settings");

    signal(SIGINT, onintr);
    signal(SIGQUIT, onintr);
    signal(SIGTERM, onintr);

#ifdef USE_ANY_SYSV_TERMIO
    rc = ioctl(tty, TCSETAW, &tio);
#elif defined(USE_TERMIOS)
    rc = tcsetattr(tty, TCSADRAIN, &tio);
#else /* not USE_TERMIOS */
    rc = ioctl(tty, TIOCSETP, &sg);
#endif /* USE_ANY_SYSV_TERMIO/USE_TERMIOS */
    if (rc != 0)
	failed("set tty settings");

    if (argc == 2) {		/* look for optional parameters of "-s" */
	char *tmpbuf = TypeMallocN(char,
				   strlen(setsize[emu]) +
				   strlen(argv[0]) +
				   strlen(argv[1]) +
				   1);
	if (tmpbuf == 0) {
	    fprintf(stderr, "%s: Cannot query size\n", myname);
	    onintr(0);
	} else {
	    sprintf(tmpbuf, setsize[emu], argv[0], argv[1]);
	    IGNORE_RC(write(tty, tmpbuf, strlen(tmpbuf)));
	    free(tmpbuf);
	}
    }
    IGNORE_RC(write(tty, getsize[emu], strlen(getsize[emu])));
    readstring(ttyfp, buf, size[emu]);
    if (sscanf(buf, size[emu], &rows, &cols) != 2) {
	fprintf(stderr, "%s: Can't get rows and columns\r\n", myname);
	onintr(0);
    }
    if (restore[emu])
	IGNORE_RC(write(tty, restore[emu], strlen(restore[emu])));
#if defined(USE_STRUCT_WINSIZE)
    /* finally, set the tty's window size */
    if (getwsize[emu]) {
	/* get the window size in pixels */
	IGNORE_RC(write(tty, getwsize[emu], strlen(getwsize[emu])));
	readstring(ttyfp, buf, wsize[emu]);
	if (sscanf(buf, wsize[emu], &ts.ws_xpixel, &ts.ws_ypixel) != 2) {
	    fprintf(stderr, "%s: Can't get window size\r\n", myname);
	    onintr(0);
	}
	setup_winsize(ts, rows, cols, 0, 0);
	SET_TTYSIZE(tty, ts);
    } else if (ioctl(tty, TIOCGWINSZ, &ts) != -1) {
	/* we don't have any way of directly finding out
	   the current height & width of the window in pixels.  We try
	   our best by computing the font height and width from the "old"
	   window-size values, and multiplying by these ratios... */
#define scaled(old,new,len) (old)?((unsigned)(new)*(len)/(old)):(len)
	setup_winsize(ts, rows, cols,
		      scaled(TTYSIZE_ROWS(ts), rows, ts.ws_ypixel),
		      scaled(TTYSIZE_COLS(ts), cols, ts.ws_xpixel));
	SET_TTYSIZE(tty, ts);
    }
#endif /* USE_STRUCT_WINSIZE */

#ifdef USE_ANY_SYSV_TERMIO
    rc = ioctl(tty, TCSETAW, &tioorig);
#elif defined(USE_TERMIOS)
    rc = tcsetattr(tty, TCSADRAIN, &tioorig);
#else /* not USE_TERMIOS */
    rc = ioctl(tty, TIOCSETP, &sgorig);
#endif /* USE_ANY_SYSV_TERMIO/USE_TERMIOS */
    if (rc != 0)
	failed("set tty settings");

    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);

#ifdef USE_TERMCAP
    if (ok_tcap) {
	/* update termcap string */
	/* first do columns */
	if ((ptr = x_strindex(termcap, "co#")) == NULL) {
	    fprintf(stderr, "%s: No `co#'\n", myname);
	    exit(EXIT_FAILURE);
	}

	i = (int) (ptr - termcap) + 3;
	strncpy(newtc, termcap, (size_t) i);
	sprintf(newtc + i, "%d", cols);
	if ((ptr = strchr(ptr, ':')) != 0)
	    strcat(newtc, ptr);

	/* now do lines */
	if ((ptr = x_strindex(newtc, "li#")) == NULL) {
	    fprintf(stderr, "%s: No `li#'\n", myname);
	    exit(EXIT_FAILURE);
	}

	i = (int) (ptr - newtc) + 3;
	strncpy(termcap, newtc, (size_t) i);
	sprintf(termcap + i, "%d", rows);
	if ((ptr = strchr(ptr, ':')) != 0)
	    strcat(termcap, ptr);
    }
#endif /* USE_TERMCAP */

    if (SHELL_BOURNE == shell_type) {

#ifdef USE_TERMCAP
	if (ok_tcap) {
	    printf("%sTERMCAP=", setname);
	    print_termcap(termcap);
	    printf(";\nexport TERMCAP;\n");
	}
#endif /* USE_TERMCAP */
#ifdef USE_TERMINFO
	printf("%sCOLUMNS=%d;\nLINES=%d;\nexport COLUMNS LINES;\n",
	       setname, cols, rows);
#endif /* USE_TERMINFO */

    } else {			/* not Bourne shell */

#ifdef USE_TERMCAP
	if (ok_tcap) {
	    printf("set noglob;\n%ssetenv TERMCAP ", setname);
	    print_termcap(termcap);
	    printf(";\nunset noglob;\n");
	}
#endif /* USE_TERMCAP */
#ifdef USE_TERMINFO
	printf("set noglob;\n%ssetenv COLUMNS '%d';\nsetenv LINES '%d';\nunset noglob;\n",
	       setname, cols, rows);
#endif /* USE_TERMINFO */
    }
    exit(EXIT_SUCCESS);
}
