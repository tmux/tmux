/* $XTermId: VTparse.h,v 1.66 2016/10/06 00:37:11 tom Exp $ */

/*
 * Copyright 1996-2015,2016 by Thomas E. Dickey
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

#ifndef included_VTparse_h
#define included_VTparse_h 1

#include <xterm.h>

#ifndef Const
# if defined(__STDC__) && !defined(__cplusplus)
#  define Const const
# else
#  define Const	/**/
# endif
#endif

/*
 * PARSE_T has to be large enough to handle the number of cases enumerated here.
 */
typedef unsigned char PARSE_T;

extern Const PARSE_T ansi_table[];
extern Const PARSE_T cigtable[];
extern Const PARSE_T csi2_table[];
extern Const PARSE_T csi_ex_table[];
extern Const PARSE_T csi_quo_table[];
extern Const PARSE_T csi_sp_table[];
extern Const PARSE_T csi_table[];
extern Const PARSE_T dec2_table[];
extern Const PARSE_T dec3_table[];
extern Const PARSE_T dec_table[];
extern Const PARSE_T eigtable[];
extern Const PARSE_T esc_sp_table[];
extern Const PARSE_T esc_table[];
extern Const PARSE_T scrtable[];
extern Const PARSE_T scs96table[];
extern Const PARSE_T scstable[];
extern Const PARSE_T sos_table[];
extern Const PARSE_T csi_dec_dollar_table[];
extern Const PARSE_T csi_tick_table[];

#if OPT_DEC_RECTOPS
extern Const PARSE_T csi_dollar_table[];
extern Const PARSE_T csi_star_table[];
#endif /* OPT_DEC_LOCATOR */

#if OPT_VT52_MODE
extern Const PARSE_T vt52_table[];
extern Const PARSE_T vt52_esc_table[];
extern Const PARSE_T vt52_ignore_table[];
#endif

#if OPT_WIDE_CHARS
extern Const PARSE_T esc_pct_table[];
extern Const PARSE_T scs_pct_table[];
#endif

#include <VTparse.hin>

#endif /* included_VTparse_h */
