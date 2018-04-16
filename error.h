/* $XTermId: error.h,v 1.26 2012/10/05 00:17:51 tom Exp $ */

/*
 * Copyright 1997-2011,2012 by Thomas E. Dickey
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

/* @(#)error.h	X10/6.6	11/6/86 */

/* main.c */
#define	ERROR_FIONBIO	11	/* main: ioctl() failed on FIONBIO */
#define	ERROR_F_GETFL	12	/* main: ioctl() failed on F_GETFL */
#define	ERROR_F_SETFL	13	/* main: ioctl() failed on F_SETFL */
#define	ERROR_OPDEVTTY	14	/* spawn: open() failed on /dev/tty */
#define	ERROR_TIOCGETP	15	/* spawn: ioctl() failed on TIOCGETP */
#define ERROR_PTSNAME   17	/* spawn: ptsname() failed */
#define ERROR_OPPTSNAME 18	/* spawn: open() failed on ptsname */
#define ERROR_PTEM      19	/* spawn: ioctl() failed on I_PUSH/"ptem" */
#define ERROR_CONSEM    20	/* spawn: ioctl() failed on I_PUSH/"consem" */
#define ERROR_LDTERM    21	/* spawn: ioctl() failed on I_PUSH/"ldterm" */
#define ERROR_TTCOMPAT  22	/* spawn: ioctl() failed on I_PUSH/"ttcompat" */
#define	ERROR_TIOCSETP	23	/* spawn: ioctl() failed on TIOCSETP */
#define	ERROR_TIOCSETC	24	/* spawn: ioctl() failed on TIOCSETC */
#define	ERROR_TIOCSETD	25	/* spawn: ioctl() failed on TIOCSETD */
#define	ERROR_TIOCSLTC	26	/* spawn: ioctl() failed on TIOCSLTC */
#define	ERROR_TIOCLSET	27	/* spawn: ioctl() failed on TIOCLSET */
#define	ERROR_INIGROUPS 28	/* spawn: initgroups() failed */
#define	ERROR_FORK	29	/* spawn: fork() failed */
#define	ERROR_EXEC	30	/* spawn: exec() failed */
#define	ERROR_PTYS	32	/* get_pty: not enough ptys */
#define ERROR_PTY_EXEC	34	/* waiting for initial map */
#define	ERROR_SETUID	35	/* spawn: setuid() failed */
#define	ERROR_INIT	36	/* spawn: can't initialize window */
#define	ERROR_TIOCKSET	46	/* spawn: ioctl() failed on TIOCKSET */
#define	ERROR_TIOCKSETC	47	/* spawn: ioctl() failed on TIOCKSETC */
#define	ERROR_LUMALLOC  49	/* luit: command-line malloc failed */

/* charproc.c */
#define	ERROR_SELECT	50	/* in_put: select() failed */
#define	ERROR_VINIT	54	/* VTInit: can't initialize window */
#define	ERROR_KMMALLOC1 57	/* HandleKeymapChange: malloc failed */

/* Tekproc.c */
#define	ERROR_TSELECT	60	/* Tinput: select() failed */
#define	ERROR_TINIT	64	/* TekInit: can't initialize window */

/* button.c */
#define	ERROR_BMALLOC2	71	/* SaltTextAway: malloc() failed */

/* misc.c */
#define	ERROR_LOGEXEC	80	/* StartLog: exec() failed */
#define	ERROR_XERROR	83	/* xerror: XError event */
#define	ERROR_XIOERROR	84	/* xioerror: X I/O error */
#define	ERROR_ICEERROR	85	/* ice_error: ICE I/O error */

/* screen.c */
#define	ERROR_SCALLOC	90	/* Alloc: calloc() failed on base */
#define	ERROR_SCALLOC2	91	/* Alloc: calloc() failed on rows */
#define	ERROR_SAVE_PTR	102	/* ScrnPointers: malloc/realloc() failed */
