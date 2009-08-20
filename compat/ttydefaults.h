/* $Id: ttydefaults.h,v 1.2 2009-08-20 12:27:58 nicm Exp $ */
/*	$OpenBSD: ttydefaults.h,v 1.6 2003/06/02 23:28:22 millert Exp $	*/
/*	$NetBSD: ttydefaults.h,v 1.8 1996/04/09 20:55:45 cgd Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ttydefaults.h	8.4 (Berkeley) 1/21/94
 */

/*
 * System wide defaults for terminal state.
 */
#ifndef _SYS_TTYDEFAULTS_H_
#define	_SYS_TTYDEFAULTS_H_

/*
 * Defaults on "first" open.
 */
#define	TTYDEF_IFLAG	(BRKINT | ICRNL | IMAXBEL | IXON | IXANY)
#define TTYDEF_OFLAG	(OPOST | ONLCR | OXTABS)
#define TTYDEF_LFLAG	(ECHO | ICANON | ISIG | IEXTEN | ECHOE|ECHOKE|ECHOCTL)
#define TTYDEF_CFLAG	(CREAD | CS8 | HUPCL)
#define TTYDEF_SPEED	(B9600)

/*
 * Control Character Defaults
 */
#define CTRL(x)	(x&037)
#ifndef CEOF
#define	CEOF		CTRL('d')
#endif
#ifndef CEOL
#define	CEOL		((unsigned char)'\377')	/* XXX avoid _POSIX_VDISABLE */
#endif
#ifndef CERASE
#define	CERASE		0177
#endif
#ifndef CINTR
#define	CINTR		CTRL('c')
#endif
#ifndef CSTATUS
#define	CSTATUS		((unsigned char)'\377')	/* XXX avoid _POSIX_VDISABLE */
#endif
#ifndef CKILL
#define	CKILL		CTRL('u')
#endif
#ifndef CMIN
#define	CMIN		1
#endif
#ifndef CQUIT
#define	CQUIT		034		/* FS, ^\ */
#endif
#ifndef CSUSP
#define	CSUSP		CTRL('z')
#endif
#ifndef CTIME
#define	CTIME		0
#endif
#ifndef CDSUSP
#define	CDSUSP		CTRL('y')
#endif
#ifndef CSTART
#define	CSTART		CTRL('q')
#endif
#ifndef CSTOP
#define	CSTOP		CTRL('s')
#endif
#ifndef CLNEXT
#define	CLNEXT		CTRL('v')
#endif
#ifndef CDISCARD
#define	CDISCARD 	CTRL('o')
#endif
#ifndef CWERASE
#define	CWERASE 	CTRL('w')
#endif
#ifndef CREPRINT
#define	CREPRINT 	CTRL('r')
#endif
#ifndef CEOT
#define	CEOT		CEOF
#endif
/* compat */
#ifndef CBRK
#define	CBRK		CEOL
#endif
#ifndef CRPRNT
#define CRPRNT		CREPRINT
#endif
#ifndef CFLUSH
#define	CFLUSH		CDISCARD
#endif

/* PROTECTED INCLUSION ENDS HERE */
#endif /* !_SYS_TTYDEFAULTS_H_ */

/*
 * #define TTYDEFCHARS to include an array of default control characters.
 */
#ifdef TTYDEFCHARS
cc_t	ttydefchars[NCCS] = {
	CEOF,	CEOL,	CEOL,	CERASE, CWERASE, CKILL, CREPRINT,
	_POSIX_VDISABLE, CINTR,	CQUIT,	CSUSP,	CDSUSP,	CSTART,	CSTOP,	CLNEXT,
	CDISCARD, CMIN,	CTIME,  CSTATUS, _POSIX_VDISABLE
};
#undef TTYDEFCHARS
#endif
