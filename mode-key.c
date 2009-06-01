/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include "tmux.h"

enum mode_key_cmd mode_key_lookup_vi(struct mode_key_data *, int);
enum mode_key_cmd mode_key_lookup_emacs(struct mode_key_data *, int);

void
mode_key_init(struct mode_key_data *mdata, int type, int flags)
{
	mdata->type = type;

	if (flags & MODEKEY_CANEDIT)
		flags |= MODEKEY_EDITMODE;
	mdata->flags = flags;
}

void
mode_key_free(unused struct mode_key_data *mdata)
{
}

enum mode_key_cmd
mode_key_lookup(struct mode_key_data *mdata, int key)
{
	switch (mdata->type) {
	case MODEKEY_VI:
		return (mode_key_lookup_vi(mdata, key));
	case MODEKEY_EMACS:
		return (mode_key_lookup_emacs(mdata, key));
	default:
		fatalx("unknown mode key type");
	}
}

enum mode_key_cmd
mode_key_lookup_vi(struct mode_key_data *mdata, int key)
{
	if (KEYC_ISESC(key)) {
		key = KEYC_REMOVEESC(key);
		if (mdata->flags & MODEKEY_CANEDIT)
			mdata->flags ^= MODEKEY_EDITMODE;
	}


	if (mdata->flags & MODEKEY_EDITMODE) {
		switch (key) {
		case '\003':
			return (MODEKEYCMD_QUIT);
		case '\033':
			if (mdata->flags & MODEKEY_CANEDIT)
				mdata->flags &= ~MODEKEY_EDITMODE;
			return (MODEKEYCMD_NONE);
		case '\010':
		case '\177':
			return (MODEKEYCMD_BACKSPACE);
		case '\011':
			return (MODEKEYCMD_COMPLETE);
		case KEYC_DC:
			return (MODEKEYCMD_DELETE);
		case '\r':
			return (MODEKEYCMD_CHOOSE);
		}
		return (MODEKEYCMD_OTHERKEY);
	}

	switch (key) {
	case '\010':
	case '\177':
		return (MODEKEYCMD_LEFT);
	case KEYC_DC:
		return (MODEKEYCMD_DELETE);
	case '\011':
		return (MODEKEYCMD_COMPLETE);
	case 'i':
		if (mdata->flags & MODEKEY_CANEDIT)
			mdata->flags |= MODEKEY_EDITMODE;
		break;
	case 'a':
		if (mdata->flags & MODEKEY_CANEDIT) {
			mdata->flags |= MODEKEY_EDITMODE;
			return (MODEKEYCMD_RIGHT);
		}
		break;
	case '\r':
		if (mdata->flags & (MODEKEY_CANEDIT|MODEKEY_CHOOSEMODE))
			return (MODEKEYCMD_CHOOSE);
		return (MODEKEYCMD_COPYSELECTION);
	case '0':
	case '^':
		return (MODEKEYCMD_STARTOFLINE);
	case '\033':
		return (MODEKEYCMD_CLEARSELECTION);
	case 'j':
	case KEYC_DOWN:
		return (MODEKEYCMD_DOWN);
	case '$':
		return (MODEKEYCMD_ENDOFLINE);
	case 'h':
	case KEYC_LEFT:
		return (MODEKEYCMD_LEFT);
	case '\006':
	case KEYC_NPAGE:
		return (MODEKEYCMD_NEXTPAGE);
	case 'w':
		return (MODEKEYCMD_NEXTWORD);
	case '\025':
	case KEYC_PPAGE:
		return (MODEKEYCMD_PREVIOUSPAGE);
	case 'b':
		return (MODEKEYCMD_PREVIOUSWORD);
	case 'q':
	case '\003':
		return (MODEKEYCMD_QUIT);
	case 'l':
	case KEYC_RIGHT:
		return (MODEKEYCMD_RIGHT);
	case ' ':
		return (MODEKEYCMD_STARTSELECTION);
	case 'k':
	case KEYC_UP:
		return (MODEKEYCMD_UP);
	case 'p':
		return (MODEKEYCMD_PASTE);
	}

	return (MODEKEYCMD_NONE);
}

enum mode_key_cmd
mode_key_lookup_emacs(struct mode_key_data *mdata, int key)
{
	switch (key) {
	case '\010':
	case '\177':
		return (MODEKEYCMD_BACKSPACE);
	case KEYC_DC:
		return (MODEKEYCMD_DELETE);
	case '\011':
		return (MODEKEYCMD_COMPLETE);
	case '\r':
		return (MODEKEYCMD_CHOOSE);
	case '\001':
		return (MODEKEYCMD_STARTOFLINE);
	case '\007':
		return (MODEKEYCMD_CLEARSELECTION);
	case '\027':
	case KEYC_ADDESC('w'):
		return (MODEKEYCMD_COPYSELECTION);
	case '\016':
	case KEYC_DOWN:
		return (MODEKEYCMD_DOWN);
	case '\005':
		return (MODEKEYCMD_ENDOFLINE);
	case '\002':
	case KEYC_LEFT:
		return (MODEKEYCMD_LEFT);
	case ' ':
		if (mdata->flags & MODEKEY_CANEDIT)
			break;
		/* FALLTHROUGH */
	case '\026':
	case KEYC_NPAGE:
		return (MODEKEYCMD_NEXTPAGE);
	case KEYC_ADDESC('f'):
		return (MODEKEYCMD_NEXTWORD);
	case '\031':
		return (MODEKEYCMD_PASTE);
	case KEYC_ADDESC('v'):
	case KEYC_PPAGE:
		return (MODEKEYCMD_PREVIOUSPAGE);
	case KEYC_ADDESC('b'):
		return (MODEKEYCMD_PREVIOUSWORD);
	case '\006':
	case KEYC_RIGHT:
		return (MODEKEYCMD_RIGHT);
	case '\000':
		return (MODEKEYCMD_STARTSELECTION);
	case '\020':
	case KEYC_UP:
		return (MODEKEYCMD_UP);
	case 'q':
		if (mdata->flags & MODEKEY_CANEDIT)
			break;
		/* FALLTHROUGH */
	case '\003':
	case '\033':
		return (MODEKEYCMD_QUIT);
	}

	return (MODEKEYCMD_OTHERKEY);
}
