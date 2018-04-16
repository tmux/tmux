/* $XTermId: wcwidth.h,v 1.14 2017/06/18 17:56:35 tom Exp $ */

/* $XFree86: xc/programs/xterm/wcwidth.h,v 1.5 2005/05/03 00:38:25 dickey Exp $ */

/*
 * Copyright 2000-2005,2017 by Thomas E. Dickey
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
#ifndef	included_wcwidth_h
#define	included_wcwidth_h 1

#include <stddef.h>

extern void mk_wcwidth_init(int mode);

extern int mk_wcswidth(const wchar_t * pwcs, size_t n);
extern int mk_wcswidth_cjk(const wchar_t * pwcs, size_t n);
extern int mk_wcwidth(wchar_t ucs);
extern int mk_wcwidth_cjk(wchar_t ucs);
extern int wcswidth_cjk(const wchar_t * pwcs, size_t n);

#endif /* included_wcwidth_h */
