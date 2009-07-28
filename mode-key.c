/* $Id: mode-key.c,v 1.20 2009-07-28 22:58:20 tcunha Exp $ */

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

/* vi editing keys. */
const struct mode_key_entry mode_key_vi_edit[] = {
	{ '\003' /* C-c */,	0, MODEKEYEDIT_CANCEL },
	{ '\010' /* C-h */, 	0, MODEKEYEDIT_BACKSPACE },
	{ '\011' /* Tab */,	0, MODEKEYEDIT_COMPLETE },
	{ '\033' /* Escape */,	0, MODEKEYEDIT_SWITCHMODE },
	{ '\r',			0, MODEKEYEDIT_ENTER },
	{ KEYC_BSPACE,		0, MODEKEYEDIT_BACKSPACE },
	{ KEYC_DC,		0, MODEKEYEDIT_DELETE },

	{ '$',			1, MODEKEYEDIT_ENDOFLINE },
	{ '0',			1, MODEKEYEDIT_STARTOFLINE },
	{ 'D',			1, MODEKEYEDIT_DELETETOENDOFLINE },
	{ '\003' /* C-c */,	1, MODEKEYEDIT_CANCEL },
	{ '\010' /* C-h */, 	1, MODEKEYEDIT_BACKSPACE },
	{ '\r',			1, MODEKEYEDIT_ENTER },
	{ '^',			1, MODEKEYEDIT_STARTOFLINE },
	{ 'a',			1, MODEKEYEDIT_SWITCHMODEAPPEND },
	{ 'h',			1, MODEKEYEDIT_CURSORLEFT },
	{ 'i',			1, MODEKEYEDIT_SWITCHMODE },
	{ 'j',			1, MODEKEYEDIT_HISTORYDOWN },
	{ 'k',			1, MODEKEYEDIT_HISTORYUP },
	{ 'l',			1, MODEKEYEDIT_CURSORRIGHT },
	{ 'p',			1, MODEKEYEDIT_PASTE },
	{ KEYC_BSPACE,		1, MODEKEYEDIT_BACKSPACE },
	{ KEYC_DC,		1, MODEKEYEDIT_DELETE },
	{ KEYC_DOWN,		1, MODEKEYEDIT_HISTORYDOWN },
	{ KEYC_LEFT,		1, MODEKEYEDIT_CURSORLEFT },
	{ KEYC_RIGHT,		1, MODEKEYEDIT_CURSORRIGHT },
	{ KEYC_UP,		1, MODEKEYEDIT_HISTORYUP },

	{ 0,		       -1, 0 }
};

/* vi choice selection keys. */
const struct mode_key_entry mode_key_vi_choice[] = {
	{ '\003' /* C-c */,	0, MODEKEYCHOICE_CANCEL },
	{ '\r',			0, MODEKEYCHOICE_CHOOSE },
	{ 'j',			0, MODEKEYCHOICE_DOWN },
	{ 'k',			0, MODEKEYCHOICE_UP },
	{ 'q',			0, MODEKEYCHOICE_CANCEL },
	{ KEYC_DOWN,		0, MODEKEYCHOICE_DOWN },
	{ KEYC_NPAGE,		0, MODEKEYCHOICE_PAGEDOWN },
	{ KEYC_PPAGE,		0, MODEKEYCHOICE_PAGEUP },
	{ KEYC_UP,		0, MODEKEYCHOICE_UP },

	{ 0,			-1, 0 }
};

/* vi copy mode keys. */
const struct mode_key_entry mode_key_vi_copy[] = {
	{ ' ',			0, MODEKEYCOPY_STARTSELECTION },
	{ '$',			0, MODEKEYCOPY_ENDOFLINE },
	{ '0',			0, MODEKEYCOPY_STARTOFLINE },
	{ '\003' /* C-c */,	0, MODEKEYCOPY_CANCEL },
	{ '\006' /* C-f */,	0, MODEKEYCOPY_NEXTPAGE },
	{ '\010' /* C-h */,	0, MODEKEYCOPY_LEFT },
	{ '\025' /* C-u */,	0, MODEKEYCOPY_PREVIOUSPAGE },
	{ '\033' /* Escape */,	0, MODEKEYCOPY_CLEARSELECTION },
	{ '\r',			0, MODEKEYCOPY_COPYSELECTION },
	{ '^',			0, MODEKEYCOPY_BACKTOINDENTATION },
	{ 'b',			0, MODEKEYCOPY_PREVIOUSWORD },
	{ 'h',			0, MODEKEYCOPY_LEFT },
	{ 'j',			0, MODEKEYCOPY_DOWN },
	{ 'k',			0, MODEKEYCOPY_UP },
	{ 'l',			0, MODEKEYCOPY_RIGHT },
	{ 'q',			0, MODEKEYCOPY_CANCEL },
	{ 'w',			0, MODEKEYCOPY_NEXTWORD },
	{ KEYC_BSPACE,		0, MODEKEYCOPY_LEFT },
	{ KEYC_DOWN,		0, MODEKEYCOPY_DOWN },
	{ KEYC_LEFT,		0, MODEKEYCOPY_LEFT },
	{ KEYC_NPAGE,		0, MODEKEYCOPY_NEXTPAGE },
	{ KEYC_PPAGE,		0, MODEKEYCOPY_PREVIOUSPAGE },
	{ KEYC_RIGHT,		0, MODEKEYCOPY_RIGHT },
	{ KEYC_UP,		0, MODEKEYCOPY_UP },

	{ 0,			-1, 0 }
};

/* emacs editing keys. */
const struct mode_key_entry mode_key_emacs_edit[] = {
	{ '\001' /* C-a */,	0, MODEKEYEDIT_STARTOFLINE }, 
	{ '\002' /* C-p */,	0, MODEKEYEDIT_CURSORLEFT },
	{ '\004' /* C-d */,	0, MODEKEYEDIT_DELETE },
	{ '\005' /* C-e	*/,	0, MODEKEYEDIT_ENDOFLINE },
	{ '\006' /* C-f */,	0, MODEKEYEDIT_CURSORRIGHT },
	{ '\010' /* C-H */, 	0, MODEKEYEDIT_BACKSPACE },
	{ '\011' /* Tab */,	0, MODEKEYEDIT_COMPLETE },
	{ '\013' /* C-k	*/,	0, MODEKEYEDIT_DELETETOENDOFLINE },
	{ '\016' /* C-n */,	0, MODEKEYEDIT_HISTORYDOWN },
	{ '\020' /* C-p */,	0, MODEKEYEDIT_HISTORYUP },
	{ '\031' /* C-y */,	0, MODEKEYEDIT_PASTE },
	{ '\r',			0, MODEKEYEDIT_ENTER },
	{ 'm' | KEYC_ESCAPE,	0, MODEKEYEDIT_STARTOFLINE }, 
	{ KEYC_BSPACE,		0, MODEKEYEDIT_BACKSPACE },
	{ KEYC_DC,		0, MODEKEYEDIT_DELETE },
	{ KEYC_DOWN,		0, MODEKEYEDIT_HISTORYDOWN },
	{ KEYC_LEFT,		0, MODEKEYEDIT_CURSORLEFT },
	{ KEYC_RIGHT,		0, MODEKEYEDIT_CURSORRIGHT },
	{ KEYC_UP,		0, MODEKEYEDIT_HISTORYUP },

	{ 0,		       -1, 0 }
};

/* emacs choice selection keys. */
const struct mode_key_entry mode_key_emacs_choice[] = {
	{ '\003' /* C-c */,	0, MODEKEYCHOICE_CANCEL },
	{ '\033' /* Escape */,	0, MODEKEYCHOICE_CANCEL },
	{ '\r',			0, MODEKEYCHOICE_CHOOSE },
	{ 'q',			0, MODEKEYCHOICE_CANCEL },
	{ KEYC_DOWN,		0, MODEKEYCHOICE_DOWN },
	{ KEYC_NPAGE,		0, MODEKEYCHOICE_PAGEDOWN },
	{ KEYC_PPAGE,		0, MODEKEYCHOICE_PAGEUP },
	{ KEYC_UP,		0, MODEKEYCHOICE_UP },

	{ 0,			-1, 0 }
};

/* emacs copy mode keys. */
const struct mode_key_entry mode_key_emacs_copy[] = {
	{ ' ',			0, MODEKEYCOPY_NEXTPAGE },
	{ '\000' /* C-Space */,	0, MODEKEYCOPY_STARTSELECTION },
	{ '\001' /* C-a */,	0, MODEKEYCOPY_STARTOFLINE },
	{ '\002' /* C-b */,	0, MODEKEYCOPY_LEFT },
	{ '\003' /* C-c */,	0, MODEKEYCOPY_CANCEL },
	{ '\005' /* C-e */,	0, MODEKEYCOPY_ENDOFLINE },
	{ '\006' /* C-f */,	0, MODEKEYCOPY_RIGHT },
	{ '\007' /* C-g */,	0, MODEKEYCOPY_CLEARSELECTION },
	{ '\016' /* C-n */,	0, MODEKEYCOPY_DOWN },
	{ '\020' /* C-p */,	0, MODEKEYCOPY_UP },
	{ '\026' /* C-v */,	0, MODEKEYCOPY_NEXTPAGE },
	{ '\027' /* C-w */,	0, MODEKEYCOPY_COPYSELECTION },
	{ '\033' /* Escape */,	0, MODEKEYCOPY_CANCEL },
	{ 'b' | KEYC_ESCAPE,	0, MODEKEYCOPY_PREVIOUSWORD },
	{ 'f' | KEYC_ESCAPE,	0, MODEKEYCOPY_NEXTWORD },
	{ 'm' | KEYC_ESCAPE,	0, MODEKEYCOPY_BACKTOINDENTATION },
	{ 'q',			0, MODEKEYCOPY_CANCEL },
	{ 'v' | KEYC_ESCAPE,	0, MODEKEYCOPY_PREVIOUSPAGE },
	{ 'w' | KEYC_ESCAPE,	0, MODEKEYCOPY_COPYSELECTION },
	{ KEYC_DOWN,		0, MODEKEYCOPY_DOWN },
	{ KEYC_LEFT,		0, MODEKEYCOPY_LEFT },
	{ KEYC_NPAGE,		0, MODEKEYCOPY_NEXTPAGE },
	{ KEYC_PPAGE,		0, MODEKEYCOPY_PREVIOUSPAGE },
	{ KEYC_RIGHT,		0, MODEKEYCOPY_RIGHT },
	{ KEYC_UP,		0, MODEKEYCOPY_UP },

	{ 0,			-1, 0 }	
};

void
mode_key_init(struct mode_key_data *mdata, const struct mode_key_entry *table)
{
	mdata->table = table;
	mdata->mode = 0;
}

enum mode_key_cmd
mode_key_lookup(struct mode_key_data *mdata, int key)
{
	const struct mode_key_entry	*ment;
	int				 mode;

	mode = mdata->mode;
	for (ment = mdata->table; ment->mode != -1; ment++) {
		if (ment->mode == mode && key == ment->key) {
			switch (ment->cmd) {
			case MODEKEYEDIT_SWITCHMODE:
			case MODEKEYEDIT_SWITCHMODEAPPEND:
				mdata->mode = 1 - mdata->mode;
				/* FALLTHROUGH */
			default:
				return (ment->cmd);
			}
		}
	}
	if (mode != 0)
		return (MODEKEY_NONE);
	return (MODEKEY_OTHER);
}
