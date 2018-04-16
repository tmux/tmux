/* $XTermId: version.c,v 1.4 2016/12/23 14:30:49 tom Exp $ */

/*
 * Copyright 2013-2015,2016 by Thomas E. Dickey
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

#include <ctype.h>
#include <xterm.h>
#include <version.h>

/*
 * Returns the version-string used in the "-v' message as well as a few other
 * places.  It is derived (when possible) from the __vendorversion__ symbol
 * that some newer imake configurations define.
 */
const char *
xtermVersion(void)
{
    static const char vendor_version[] = __vendorversion__;
    static char *buffer;
    const char *result;

    if (buffer == 0) {
	const char *vendor = vendor_version;

	buffer = TextAlloc(strlen(vendor) + 9);
	if (buffer == 0) {
	    result = vendor;
	} else {
	    char first[BUFSIZ];
	    char second[BUFSIZ];

	    /* some vendors leave trash in this string */
	    for (;;) {
		if (!strncmp(vendor, "Version ", (size_t) 8))
		    vendor += 8;
		else if (isspace(CharOf(*vendor)))
		    ++vendor;
		else
		    break;
	    }
	    if (strlen(vendor) < BUFSIZ &&
		sscanf(vendor, "%[0-9.] %[A-Za-z_0-9.]", first, second) == 2) {
		sprintf(buffer, "%s %s(%d)", second, first, XTERM_PATCH);
	    } else {
		sprintf(buffer, "%s(%d)", vendor, XTERM_PATCH);
	    }
	    result = buffer;
	}
    } else {
	result = buffer;
    }
    return result;
}
