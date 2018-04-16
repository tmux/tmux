/* $XTermId: xutf8.c,v 1.16 2017/05/31 09:05:00 tom Exp $ */

/*
 * Copyright 2002-2016,2017 by Thomas E. Dickey
 * Copyright (c) 2001 by Juliusz Chroboczek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <xterm.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xmu/Xmu.h>

#include <xutf8.h>

#ifndef X_HAVE_UTF8_STRING

#undef XA_UTF8_STRING
#define KEYSYM2UCS_INCLUDED

#include "keysym2ucs.c"

Atom
_xa_utf8_string(Display *dpy)
{
    static AtomPtr p = NULL;

    if (p == NULL)
	p = XmuMakeAtom("UTF8_STRING");

    return XmuInternAtom(dpy, p);
}
#define XA_UTF8_STRING(dpy) _xa_utf8_string(dpy)

static int
utf8countBytes(int c)
{
    if (c < 0)
	return 0;

    if (c <= 0x7F) {
	return 1;
    } else if (c <= 0x7FF) {
	return 2;
    } else if (c <= 0xFFFF) {
	return 3;
    } else
	return 4;
}

static void
utf8insert(char *dest, int c, size_t *len_return)
{
    if (c < 0)
	return;

    if (c <= 0x7F) {
	dest[0] = (char) c;
	*len_return = 1;
    } else if (c <= 0x7FF) {
	dest[0] = (char) (0xC0 | ((c >> 6) & 0x1F));
	dest[1] = (char) (0x80 | (c & 0x3F));
	*len_return = 2;
    } else if (c <= 0xFFFF) {
	dest[0] = (char) (0xE0 | ((c >> 12) & 0x0F));
	dest[1] = (char) (0x80 | ((c >> 6) & 0x3F));
	dest[2] = (char) (0x80 | (c & 0x3F));
	*len_return = 3;
    } else {
	dest[0] = (char) (0xF0 | ((c >> 18) & 0x07));
	dest[1] = (char) (0x80 | ((c >> 12) & 0x3f));
	dest[2] = (char) (0x80 | ((c >> 6) & 0x3f));
	dest[3] = (char) (0x80 | (c & 0x3f));
	*len_return = 4;
    }
}

static size_t
l1countUtf8Bytes(char *s, size_t len)
{
    size_t l = 0;
    while (len != 0) {
	if ((*s & 0x80) == 0)
	    l++;
	else
	    l += 2;
	s++;
	len--;
    }
    return l;
}

static void
l1utf8copy(char *d, char *s, size_t len)
{
    size_t l;
    while (len != 0) {
	utf8insert(d, (*s) & 0xFF, &l);
	d += (int) l;
	s++;
	len--;
    }
}

static void
utf8l1strcpy(char *d, char *s)
{
#define SKIP do { s++; } while(((*s & 0x80) != 0) && (*s & 0xC0) != 0xC0)
    while (*s) {
	if ((*s & 0x80) == 0)
	    *d++ = *s++;
	else if ((*s & 0x7C) == 0x40) {
	    if ((s[1] & 0x80) == 0) {
		s++;		/* incorrect UTF-8 */
		continue;
	    } else if ((*s & 0x7C) == 0x40) {
		*d++ = (char) (((*s & 0x03) << 6) | (s[1] & 0x3F));
		s += 2;
	    } else {
		*d++ = '?';
		SKIP;
	    }
	} else {
	    *d++ = '?';
	    SKIP;
	}
    }
    *d = 0;
#undef SKIP
}

/* Keep this in sync with utf8l1strcpy! */
static int
utf8l1strlen(char *s)
{
#define SKIP do { s++; } while(((*s & 0x80) != 0) && (*s & 0xC0) != 0xC0)
    int len = 0;
    while (*s) {
	if ((*s & 0x80) == 0) {
	    s++;
	    len++;
	} else if ((*s & 0x7C) == 0x40) {
	    if ((s[1] & 0x80) == 0) {
		s++;
		continue;
	    } else if ((*s & 0x7C) == 0x40) {
		len++;
		s += 2;
	    } else {
		len++;
		SKIP;
	    }
	} else {
	    len++;
	    SKIP;
	}
    }
#undef SKIP
    return len;
}

int
Xutf8TextPropertyToTextList(Display *dpy,
			    const XTextProperty * tp,
			    char ***list_return,
			    int *count_return)
{
    int utf8;
    char **list;
    int nelements;
    char *cp;
    char *start;
    size_t i;
    int j;
    size_t datalen = tp->nitems;
    size_t len;

    if (tp->format != 8)
	return XConverterNotFound;

    if (tp->encoding == XA_STRING)
	utf8 = 0;
    else if (tp->encoding == XA_UTF8_STRING(dpy))
	utf8 = 1;
    else
	return XConverterNotFound;

    if (datalen == 0) {
	*list_return = NULL;
	*count_return = 0;
	return 0;
    }

    nelements = 1;
    for (cp = (char *) tp->value, i = datalen; i != 0; cp++, i--) {
	if (*cp == '\0')
	    nelements++;
    }

    list = TypeMallocN(char *, (unsigned) nelements);
    if (!list)
	return XNoMemory;

    if (utf8)
	len = datalen;
    else
	len = l1countUtf8Bytes((char *) tp->value, datalen);

    start = TextAlloc(len);
    if (!start) {
	free(list);
	return XNoMemory;
    }

    if (utf8)
	memcpy(start, (char *) tp->value, datalen);
    else
	l1utf8copy(start, (char *) tp->value, datalen);
    start[len] = '\0';

    for (cp = start, i = len + 1, j = 0; i != 0; cp++, i--) {
	if (*cp == '\0') {
	    list[j] = start;
	    start = (cp + 1);
	    j++;
	}
    }

    list[j] = NULL;
    *list_return = list;
    *count_return = nelements;
    return 0;
}

int
Xutf8TextListToTextProperty(Display *dpy,
			    char **list,
			    int count,
			    XICCEncodingStyle style,
			    XTextProperty * text_prop)
{
    XTextProperty proto;
    unsigned int nbytes;
    int i;

    if (style != XStringStyle &&
	style != XCompoundTextStyle &&
	style != XStdICCTextStyle &&
	style != XUTF8StringStyle)
	return XConverterNotFound;

    if (style == XUTF8StringStyle) {
	for (i = 0, nbytes = 0; i < count; i++) {
	    nbytes += (unsigned) ((list[i] ? strlen(list[i]) : 0) + 1);
	}
    } else {
	for (i = 0, nbytes = 0; i < count; i++) {
	    nbytes += (unsigned) ((list[i] ? utf8l1strlen(list[i]) : 0) + 1);
	}
    }

    if (style == XCompoundTextStyle)
	proto.encoding = XA_COMPOUND_TEXT(dpy);
    else if (style == XUTF8StringStyle)
	proto.encoding = XA_UTF8_STRING(dpy);
    else
	proto.encoding = XA_STRING;
    proto.format = 8;
    if (nbytes)
	proto.nitems = nbytes - 1;
    else
	proto.nitems = 0;
    proto.value = NULL;

    if (nbytes > 0) {
	char *buf = TypeMallocN(char, nbytes);
	if (!buf)
	    return XNoMemory;

	proto.value = (unsigned char *) buf;
	for (i = 0; i < count; i++) {
	    char *arg = list[i];

	    if (arg) {
		if (style == XUTF8StringStyle) {
		    strcpy(buf, arg);
		} else {
		    utf8l1strcpy(buf, arg);
		}
		buf += (strlen(buf) + 1);
	    } else {
		*buf++ = '\0';
	    }
	}
    } else {
	proto.value = CastMalloc(unsigned char);	/* easier for client */
	if (!proto.value)
	    return XNoMemory;

	proto.value[0] = '\0';
    }

    *text_prop = proto;
    return 0;
}

int
Xutf8LookupString(XIC ic GCC_UNUSED,
		  XKeyEvent *ev,
		  char *buffer,
		  int nbytes,
		  KeySym * keysym_return,
		  Status * status_return)
{
    int rc;
    KeySym keysym;
    int codepoint;
    size_t len;

    rc = XLookupString(ev, buffer, nbytes, &keysym, NULL);

    if (rc > 0) {
	codepoint = buffer[0] & 0xFF;
    } else {
	codepoint = keysym2ucs(keysym);
    }

    if (codepoint < 0) {
	if (keysym == None) {
	    *status_return = XLookupNone;
	} else {
	    *status_return = XLookupKeySym;
	    *keysym_return = keysym;
	}
	return 0;
    }

    if (nbytes < utf8countBytes(codepoint)) {
	*status_return = XBufferOverflow;
	return utf8countBytes(codepoint);
    }

    utf8insert(buffer, codepoint, &len);

    if (keysym != None) {
	*keysym_return = keysym;
	*status_return = XLookupBoth;
    } else {
	*status_return = XLookupChars;
    }
    return (int) len;
}

#else /* X_HAVE_UTF8_STRING */
/* Silence the compiler */
void
xutf8_dummy(void)
{
    return;
}
#endif
