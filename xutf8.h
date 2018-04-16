/*
 * $XTermId: xutf8.h,v 1.4 2010/10/10 14:10:12 Jeremy.Huddleston Exp $
 */
/*
Copyright (c) 2001 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <X11/Xlib.h>

#ifndef X_HAVE_UTF8_STRING

#undef XA_UTF8_STRING
Atom _xa_utf8_string(Display*);
#define XA_UTF8_STRING(dpy) _xa_utf8_string(dpy)

#undef XUTF8StringStyle
#define XUTF8StringStyle 4

int Xutf8TextPropertyToTextList(
    Display *,
    const XTextProperty *,
    char ***,
    int *
);
int
Xutf8TextListToTextProperty(
    Display *,
    char **,
    int,
    XICCEncodingStyle,
    XTextProperty *
);
int Xutf8LookupString(
    XIC,
    XKeyPressedEvent *,
    char *,
    int,
    KeySym *,
    Status *
);
#else
void xutf8_dummy(void);
#endif
