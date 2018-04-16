/* $XTermId: xstrings.h,v 1.30 2016/12/22 23:48:38 tom Exp $ */

/*
 * Copyright 2000-2015,2016 by Thomas E. Dickey
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

#ifndef included_xstrings_h
#define included_xstrings_h 1
/* *INDENT-OFF* */

#include <X11/Intrinsic.h>
#include <pwd.h>

#define OkPasswd(p) ((p)->pw_name != 0 && (p)->pw_name[0] != 0)

extern Boolean x_getpwnam(const char * /* name */, struct passwd * /* result */);
extern Boolean x_getpwuid(uid_t /* uid */, struct passwd * /* result */);
extern String x_nonempty(String /* s */);
extern String x_skip_blanks(String /* s */);
extern String x_skip_nonblanks(String /* s */);
extern char **x_splitargs(const char * /* command */);
extern char *x_basename(char * /* name */);
extern char *x_decode_hex(const char * /* source */, const char ** /* next */);
extern char *x_encode_hex(const char * /* source */);
extern char *x_getenv(const char * /* name */);
extern char *x_getlogin(uid_t /* uid */, struct passwd * /* in_out */);
extern char *x_strdup(const char * /* s */);
extern char *x_strindex(char * /* s1 */, const char * /* s2 */);
extern char *x_strtrim(const char * /* s */);
extern char *x_strrtrim(const char * /* s */);
extern char x_toupper(int /* ch */);
extern int x_hex2int(int /* ch */);
extern int x_strcasecmp(const char * /* s1 */, const char * /* s2 */);
extern int x_strncasecmp(const char * /* s1 */, const char * /* s2 */, unsigned  /* n */);
extern int x_wildstrcmp(const char * /* pattern */, const char * /* actual */);
extern unsigned x_countargv(char ** /* argv */);
extern void x_appendargv(char ** /* target */, char ** /* source */);
extern void x_freeargs(char ** /* argv */);

/* *INDENT-ON* */

#endif /* included_xstrings_h */
