/* $XTermId: ptydata.c,v 1.107 2017/06/16 23:40:20 tom Exp $ */

/*
 * Copyright 1999-2016,2017 by Thomas E. Dickey
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

#include <data.h>

#if OPT_WIDE_CHARS
#include <menu.h>
#include <wcwidth.h>
#endif

/*
 * Check for both EAGAIN and EWOULDBLOCK, because some supposedly POSIX
 * systems are broken and return EWOULDBLOCK when they should return EAGAIN.
 * Note that this macro may evaluate its argument more than once.
 */
#if defined(EAGAIN) && defined(EWOULDBLOCK)
#define E_TEST(err) ((err) == EAGAIN || (err) == EWOULDBLOCK)
#else
#ifdef EAGAIN
#define E_TEST(err) ((err) == EAGAIN)
#else
#define E_TEST(err) ((err) == EWOULDBLOCK)
#endif
#endif

#if OPT_WIDE_CHARS
/*
 * Convert the 8-bit codes in data->buffer[] into Unicode in data->utf_data.
 * The number of bytes converted will be nonzero iff there is data.
 */
Bool
decodeUtf8(TScreen *screen, PtyData *data)
{
    int i;
    int length = (int) (data->last - data->next);
    int utf_count = 0;
    unsigned utf_char = 0;

    data->utf_size = 0;
    for (i = 0; i < length; i++) {
	unsigned c = data->next[i];

	/* Combine UTF-8 into Unicode */
	if (c < 0x80) {
	    /* We received an ASCII character */
	    if (utf_count > 0) {
		data->utf_data = UCS_REPL;	/* prev. sequence incomplete */
		data->utf_size = i;
	    } else {
		data->utf_data = (IChar) c;
		data->utf_size = 1;
	    }
	    break;
	} else if (c < 0xc0) {
	    /* We received a continuation byte */
	    if (utf_count < 1) {
		/*
		 * We received a continuation byte before receiving a sequence
		 * state.  Or an attempt to use a C1 control string.  Either
		 * way, it is mapped to the replacement character, unless
		 * allowed by optional feature.
		 */
		data->utf_data = (IChar) (screen->c1_printable ? c : UCS_REPL);
		data->utf_size = (i + 1);
		break;
	    } else {
		/* Check for overlong UTF-8 sequences for which a shorter
		 * encoding would exist and replace them with UCS_REPL.
		 * An overlong UTF-8 sequence can have any of the following
		 * forms:
		 *   1100000x 10xxxxxx
		 *   11100000 100xxxxx 10xxxxxx
		 *   11110000 1000xxxx 10xxxxxx 10xxxxxx
		 *   11111000 10000xxx 10xxxxxx 10xxxxxx 10xxxxxx
		 *   11111100 100000xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
		 */
		if (!utf_char && !((c & 0x7f) >> (7 - utf_count))) {
		    utf_char = UCS_REPL;
		}
		utf_char <<= 6;
		utf_char |= (c & 0x3f);
		if ((utf_char >= 0xd800 &&
		     utf_char <= 0xdfff) ||
		    (utf_char == 0xfffe) ||
		    (utf_char == HIDDEN_CHAR)) {
		    utf_char = UCS_REPL;
		}
		utf_count--;
		if (utf_count == 0) {
#if !OPT_WIDER_ICHAR
		    /* characters outside UCS-2 become UCS_REPL */
		    if (utf_char > 0xffff) {
			TRACE(("using replacement for %#x\n", utf_char));
			utf_char = UCS_REPL;
		    }
#endif
		    data->utf_data = (IChar) utf_char;
		    data->utf_size = (i + 1);
		    break;
		}
	    }
	} else {
	    /* We received a sequence start byte */
	    if (utf_count > 0) {
		data->utf_data = UCS_REPL;	/* prev. sequence incomplete */
		data->utf_size = (i + 1);
		break;
	    }
	    if (c < 0xe0) {
		utf_count = 1;
		utf_char = (c & 0x1f);
		if (!(c & 0x1e)) {
		    utf_char = UCS_REPL;	/* overlong sequence */
		}
	    } else if (c < 0xf0) {
		utf_count = 2;
		utf_char = (c & 0x0f);
	    } else if (c < 0xf8) {
		utf_count = 3;
		utf_char = (c & 0x07);
	    } else if (c < 0xfc) {
		utf_count = 4;
		utf_char = (c & 0x03);
	    } else if (c < 0xfe) {
		utf_count = 5;
		utf_char = (c & 0x01);
	    } else {
		data->utf_data = UCS_REPL;
		data->utf_size = (i + 1);
		break;
	    }
	}
    }
#if OPT_TRACE > 1
    TRACE(("UTF-8 char %04X [%d..%d]\n",
	   data->utf_data,
	   (int) (data->next - data->buffer),
	   (int) (data->next - data->buffer + data->utf_size - 1)));
#endif

    return (data->utf_size != 0);
}
#endif

int
readPtyData(XtermWidget xw, PtySelect * select_mask, PtyData *data)
{
    TScreen *screen = TScreenOf(xw);
    int size = 0;

#ifdef VMS
    if (*select_mask & pty_mask) {
	trimPtyData(xw, data);
	if (read_queue.flink != 0) {
	    size = tt_read(data->next);
	    if (size == 0) {
		Panic("input: read returned zero\n", 0);
	    }
	} else {
	    sys$hiber();
	}
    }
#else /* !VMS */
    if (FD_ISSET(screen->respond, select_mask)) {
	int save_err;
	trimPtyData(xw, data);

	size = (int) read(screen->respond, (char *) data->last, (size_t) FRG_SIZE);
	save_err = errno;
#if (defined(i386) && defined(SVR4) && defined(sun)) || defined(__CYGWIN__)
	/*
	 * Yes, I know this is a majorly f*ugly hack, however it seems to
	 * be necessary for Solaris x86.  DWH 11/15/94
	 * Dunno why though..
	 * (and now CYGWIN, alanh@xfree86.org 08/15/01
	 */
	if (size <= 0) {
	    if (save_err == EIO || save_err == 0)
		NormalExit();
	    else if (!E_TEST(save_err))
		Panic("input: read returned unexpected error (%d)\n", save_err);
	    size = 0;
	}
#else /* !f*ugly */
	if (size < 0) {
	    if (save_err == EIO)
		NormalExit();
	    else if (!E_TEST(save_err))
		Panic("input: read returned unexpected error (%d)\n", save_err);
	    size = 0;
	} else if (size == 0) {
#if defined(__FreeBSD__)
	    NormalExit();
#else
	    Panic("input: read returned zero\n", 0);
#endif
	}
#endif /* f*ugly */
    }
#endif /* VMS */

    if (size) {
#if OPT_TRACE
	int i;

	TRACE(("read %d bytes from pty\n", size));
	for (i = 0; i < size; i++) {
	    if (!(i % 16))
		TRACE(("%s", i ? "\n    " : "READ"));
	    TRACE((" %02X", data->last[i]));
	}
	TRACE(("\n"));
#endif
	data->last += size;
#ifdef ALLOWLOGGING
	TScreenOf(term)->logstart = VTbuffer->next;
#endif
    }

    return (size);
}

/*
 * Return the next value from the input buffer.  Note that morePtyData() is
 * always called before this function, so we can do the UTF-8 input conversion
 * in that function and simply return the result here.
 */
#if OPT_WIDE_CHARS
IChar
nextPtyData(TScreen *screen, PtyData *data)
{
    IChar result;
    if (screen->utf8_inparse) {
	result = skipPtyData(data);
    } else {
	result = *((data)->next++);
	if (!screen->output_eight_bits) {
	    result = (IChar) (result & 0x7f);
	}
    }
    TRACE2(("nextPtyData returns %#x\n", result));
    return result;
}

/*
 * Simply return the data and skip past it.
 */
IChar
skipPtyData(PtyData *data)
{
    IChar result = data->utf_data;

    data->next += data->utf_size;
    data->utf_size = 0;

    return result;
}
#endif

#if OPT_WIDE_CHARS
/*
 * Called when UTF-8 mode has been turned on/off.
 */
void
switchPtyData(TScreen *screen, int flag)
{
    if (screen->utf8_mode != flag) {
	screen->utf8_mode = flag;
	screen->utf8_inparse = (Boolean) (flag != 0);
	mk_wcwidth_init(screen->utf8_mode);

	TRACE(("turning UTF-8 mode %s\n", BtoS(flag)));
	update_font_utf8_mode();
    }
}
#endif

/*
 * Allocate a buffer.
 */
void
initPtyData(PtyData **result)
{
    PtyData *data;

    TRACE(("initPtyData given minBufSize %d, maxBufSize %d\n",
	   FRG_SIZE, BUF_SIZE));

    if (FRG_SIZE < 64)
	FRG_SIZE = 64;
    if (BUF_SIZE < FRG_SIZE)
	BUF_SIZE = FRG_SIZE;
    if (BUF_SIZE % FRG_SIZE)
	BUF_SIZE = BUF_SIZE + FRG_SIZE - (BUF_SIZE % FRG_SIZE);

    TRACE(("initPtyData using minBufSize %d, maxBufSize %d\n",
	   FRG_SIZE, BUF_SIZE));

    data = TypeXtMallocX(PtyData, (BUF_SIZE + FRG_SIZE));

    memset(data, 0, sizeof(*data));
    data->next = data->buffer;
    data->last = data->buffer;
    *result = data;
}

/*
 * Initialize a buffer for the caller, using its data in 'next'.
 */
#if OPT_WIDE_CHARS
PtyData *
fakePtyData(PtyData *result, Char *next, Char *last)
{
    PtyData *data = result;

    memset(data, 0, sizeof(*data));
    data->next = next;
    data->last = last;

    return data;
}
#endif

/*
 * Remove used data by shifting the buffer down, to make room for more data,
 * e.g., a continuation-read.
 */
void
trimPtyData(XtermWidget xw GCC_UNUSED, PtyData *data)
{
    FlushLog(xw);

    if (data->next != data->buffer) {
	int i;
	int n = (int) (data->last - data->next);

	TRACE(("shifting buffer down by %d\n", n));
	for (i = 0; i < n; ++i) {
	    data->buffer[i] = data->next[i];
	}
	data->next = data->buffer;
	data->last = data->next + n;
    }

}

/*
 * Insert new data into the input buffer so the next calls to morePtyData()
 * and nextPtyData() will return that.
 */
void
fillPtyData(XtermWidget xw, PtyData *data, const char *value, int length)
{
    int size;
    int n;

    /* remove the used portion of the buffer */
    trimPtyData(xw, data);

    VTbuffer->last += length;
    size = (int) (VTbuffer->last - VTbuffer->next);

    /* shift the unused portion up to make room */
    for (n = size; n >= length; --n)
	VTbuffer->next[n] = VTbuffer->next[n - length];

    /* insert the new bytes to interpret */
    for (n = 0; n < length; n++)
	VTbuffer->next[n] = CharOf(value[n]);
}

#if OPT_WIDE_CHARS
Char *
convertToUTF8(Char *lp, unsigned c)
{
#define CH(n) (Char)((c) >> ((n) * 8))
    if (c < 0x80) {
	/*  0*******  */
	*lp++ = (Char) CH(0);
    } else if (c < 0x800) {
	/*  110***** 10******  */
	*lp++ = (Char) (0xc0 | (CH(0) >> 6) | ((CH(1) & 0x07) << 2));
	*lp++ = (Char) (0x80 | (CH(0) & 0x3f));
    } else if (c < 0x00010000) {
	/*  1110**** 10****** 10******  */
	*lp++ = (Char) (0xe0 | ((int) (CH(1) & 0xf0) >> 4));
	*lp++ = (Char) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	*lp++ = (Char) (0x80 | (CH(0) & 0x3f));
    } else if (c < 0x00200000) {
	*lp++ = (Char) (0xf0 | ((int) (CH(2) & 0x1f) >> 2));
	*lp++ = (Char) (0x80 |
			((int) (CH(1) & 0xf0) >> 4) |
			((int) (CH(2) & 0x03) << 4));
	*lp++ = (Char) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	*lp++ = (Char) (0x80 | (CH(0) & 0x3f));
    } else if (c < 0x04000000) {
	*lp++ = (Char) (0xf8 | (CH(3) & 0x03));
	*lp++ = (Char) (0x80 | (CH(2) >> 2));
	*lp++ = (Char) (0x80 |
			((int) (CH(1) & 0xf0) >> 4) |
			((int) (CH(2) & 0x03) << 4));
	*lp++ = (Char) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	*lp++ = (Char) (0x80 | (CH(0) & 0x3f));
    } else {
	*lp++ = (Char) (0xfc | ((int) (CH(3) & 0x40) >> 6));
	*lp++ = (Char) (0x80 | (CH(3) & 0x3f));
	*lp++ = (Char) (0x80 | (CH(2) >> 2));
	*lp++ = (Char) (0x80 | (CH(1) >> 4) | ((CH(2) & 0x03) << 4));
	*lp++ = (Char) (0x80 | (CH(0) >> 6) | ((CH(1) & 0x0f) << 2));
	*lp++ = (Char) (0x80 | (CH(0) & 0x3f));
    }
    return lp;
#undef CH
}

/*
 * Write data back to the PTY
 */
void
writePtyData(int f, IChar *d, unsigned len)
{
    unsigned n = (len << 1);

    if (VTbuffer->write_len <= len) {
	VTbuffer->write_len = n;
	VTbuffer->write_buf = (Char *) XtRealloc((char *)
						 VTbuffer->write_buf, VTbuffer->write_len);
    }

    for (n = 0; n < len; n++)
	VTbuffer->write_buf[n] = (Char) d[n];

    TRACE(("writePtyData %d:%s\n", n,
	   visibleChars(VTbuffer->write_buf, n)));
    v_write(f, VTbuffer->write_buf, n);
}
#endif /* OPT_WIDE_CHARS */

#ifdef NO_LEAKS
void
noleaks_ptydata(void)
{
    if (VTbuffer != 0) {
#if OPT_WIDE_CHARS
	if (VTbuffer->write_buf != 0)
	    free(VTbuffer->write_buf);
#endif
	free(VTbuffer);
	VTbuffer = 0;
    }
}
#endif
