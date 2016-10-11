/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <string.h>

#include "tmux.h"

/*
 * Mode keys. These are the key bindings used when editing (status prompt), and
 * in the modes. They are split into two sets of three tables, one set of three
 * for vi and the other for emacs key bindings. The three tables are for
 * editing, for menu-like modes (choice, more), and for copy modes (copy,
 * scroll).
 *
 * The fixed tables of struct mode_key_entry below are the defaults: they are
 * built into a tree of struct mode_key_binding by mode_key_init_trees, which
 * can then be modified.
 *
 * vi command mode is handled by having a mode flag in the struct which allows
 * two sets of bindings to be swapped between. A couple of editing commands
 * (any matching MODEKEYEDIT_SWITCHMODE*) are special-cased to do this.
 */

/* Entry in the default mode key tables. */
struct mode_key_entry {
	key_code		key;

	/*
	 * Editing mode for vi: 0 is edit mode, keys not in the table are
	 * returned as MODEKEY_OTHER; 1 is command mode, keys not in the table
	 * are returned as MODEKEY_NONE. This is also matched on, allowing some
	 * keys to be bound in edit mode.
	 */
	int			mode;
	enum mode_key_cmd	cmd;
};

/* Edit keys command strings. */
static const struct mode_key_cmdstr mode_key_cmdstr_edit[] = {
	{ MODEKEYEDIT_BACKSPACE, "backspace" },
	{ MODEKEYEDIT_CANCEL, "cancel" },
	{ MODEKEYEDIT_COMPLETE, "complete" },
	{ MODEKEYEDIT_CURSORLEFT, "cursor-left" },
	{ MODEKEYEDIT_CURSORRIGHT, "cursor-right" },
	{ MODEKEYEDIT_DELETE, "delete" },
	{ MODEKEYEDIT_DELETELINE, "delete-line" },
	{ MODEKEYEDIT_DELETETOENDOFLINE, "delete-end-of-line" },
	{ MODEKEYEDIT_DELETEWORD, "delete-word" },
	{ MODEKEYEDIT_ENDOFLINE, "end-of-line" },
	{ MODEKEYEDIT_ENTER, "enter" },
	{ MODEKEYEDIT_HISTORYDOWN, "history-down" },
	{ MODEKEYEDIT_HISTORYUP, "history-up" },
	{ MODEKEYEDIT_NEXTSPACE, "next-space" },
	{ MODEKEYEDIT_NEXTSPACEEND, "next-space-end" },
	{ MODEKEYEDIT_NEXTWORD, "next-word" },
	{ MODEKEYEDIT_NEXTWORDEND, "next-word-end" },
	{ MODEKEYEDIT_PASTE, "paste" },
	{ MODEKEYEDIT_PREVIOUSSPACE, "previous-space" },
	{ MODEKEYEDIT_PREVIOUSWORD, "previous-word" },
	{ MODEKEYEDIT_STARTOFLINE, "start-of-line" },
	{ MODEKEYEDIT_SWITCHMODE, "switch-mode" },
	{ MODEKEYEDIT_SWITCHMODEAPPEND, "switch-mode-append" },
	{ MODEKEYEDIT_SWITCHMODEAPPENDLINE, "switch-mode-append-line" },
	{ MODEKEYEDIT_SWITCHMODEBEGINLINE, "switch-mode-begin-line" },
	{ MODEKEYEDIT_SWITCHMODECHANGELINE, "switch-mode-change-line" },
	{ MODEKEYEDIT_SWITCHMODESUBSTITUTE, "switch-mode-substitute" },
	{ MODEKEYEDIT_SWITCHMODESUBSTITUTELINE, "switch-mode-substitute-line" },
	{ MODEKEYEDIT_TRANSPOSECHARS, "transpose-chars" },

	{ 0, NULL }
};

/* Choice keys command strings. */
static const struct mode_key_cmdstr mode_key_cmdstr_choice[] = {
	{ MODEKEYCHOICE_BACKSPACE, "backspace" },
	{ MODEKEYCHOICE_BOTTOMLINE, "bottom-line"},
	{ MODEKEYCHOICE_CANCEL, "cancel" },
	{ MODEKEYCHOICE_CHOOSE, "choose" },
	{ MODEKEYCHOICE_DOWN, "down" },
	{ MODEKEYCHOICE_ENDOFLIST, "end-of-list"},
	{ MODEKEYCHOICE_PAGEDOWN, "page-down" },
	{ MODEKEYCHOICE_PAGEUP, "page-up" },
	{ MODEKEYCHOICE_SCROLLDOWN, "scroll-down" },
	{ MODEKEYCHOICE_SCROLLUP, "scroll-up" },
	{ MODEKEYCHOICE_STARTNUMBERPREFIX, "start-number-prefix" },
	{ MODEKEYCHOICE_STARTOFLIST, "start-of-list"},
	{ MODEKEYCHOICE_TOPLINE, "top-line"},
	{ MODEKEYCHOICE_TREE_COLLAPSE, "tree-collapse" },
	{ MODEKEYCHOICE_TREE_COLLAPSE_ALL, "tree-collapse-all" },
	{ MODEKEYCHOICE_TREE_EXPAND, "tree-expand" },
	{ MODEKEYCHOICE_TREE_EXPAND_ALL, "tree-expand-all" },
	{ MODEKEYCHOICE_TREE_TOGGLE, "tree-toggle" },
	{ MODEKEYCHOICE_UP, "up" },

	{ 0, NULL }
};

/* vi editing keys. */
static const struct mode_key_entry mode_key_vi_edit[] = {
	{ '\003' /* C-c */,	    0, MODEKEYEDIT_CANCEL },
	{ '\010' /* C-h */,	    0, MODEKEYEDIT_BACKSPACE },
	{ '\011' /* Tab */,	    0, MODEKEYEDIT_COMPLETE },
	{ '\025' /* C-u */,	    0, MODEKEYEDIT_DELETELINE },
	{ '\027' /* C-w */,	    0, MODEKEYEDIT_DELETEWORD },
	{ '\033' /* Escape */,	    0, MODEKEYEDIT_SWITCHMODE },
	{ '\n',			    0, MODEKEYEDIT_ENTER },
	{ '\r',			    0, MODEKEYEDIT_ENTER },
	{ KEYC_BSPACE,		    0, MODEKEYEDIT_BACKSPACE },
	{ KEYC_DC,		    0, MODEKEYEDIT_DELETE },
	{ KEYC_DOWN,		    0, MODEKEYEDIT_HISTORYDOWN },
	{ KEYC_LEFT,		    0, MODEKEYEDIT_CURSORLEFT },
	{ KEYC_RIGHT,		    0, MODEKEYEDIT_CURSORRIGHT },
	{ KEYC_UP,		    0, MODEKEYEDIT_HISTORYUP },
	{ KEYC_HOME,		    0, MODEKEYEDIT_STARTOFLINE },
	{ KEYC_END,		    0, MODEKEYEDIT_ENDOFLINE },

	{ '$',			    1, MODEKEYEDIT_ENDOFLINE },
	{ '0',			    1, MODEKEYEDIT_STARTOFLINE },
	{ 'A',			    1, MODEKEYEDIT_SWITCHMODEAPPENDLINE },
	{ 'B',			    1, MODEKEYEDIT_PREVIOUSSPACE },
	{ 'C',			    1, MODEKEYEDIT_SWITCHMODECHANGELINE },
	{ 'D',			    1, MODEKEYEDIT_DELETETOENDOFLINE },
	{ 'E',			    1, MODEKEYEDIT_NEXTSPACEEND },
	{ 'I',			    1, MODEKEYEDIT_SWITCHMODEBEGINLINE },
	{ 'S',			    1, MODEKEYEDIT_SWITCHMODESUBSTITUTELINE },
	{ 'W',			    1, MODEKEYEDIT_NEXTSPACE },
	{ 'X',			    1, MODEKEYEDIT_BACKSPACE },
	{ '\003' /* C-c */,	    1, MODEKEYEDIT_CANCEL },
	{ '\010' /* C-h */,	    1, MODEKEYEDIT_BACKSPACE },
	{ '\n',			    1, MODEKEYEDIT_ENTER },
	{ '\r',			    1, MODEKEYEDIT_ENTER },
	{ '^',			    1, MODEKEYEDIT_STARTOFLINE },
	{ 'a',			    1, MODEKEYEDIT_SWITCHMODEAPPEND },
	{ 'b',			    1, MODEKEYEDIT_PREVIOUSWORD },
	{ 'd',			    1, MODEKEYEDIT_DELETELINE },
	{ 'e',			    1, MODEKEYEDIT_NEXTWORDEND },
	{ 'h',			    1, MODEKEYEDIT_CURSORLEFT },
	{ 'i',			    1, MODEKEYEDIT_SWITCHMODE },
	{ 'j',			    1, MODEKEYEDIT_HISTORYDOWN },
	{ 'k',			    1, MODEKEYEDIT_HISTORYUP },
	{ 'l',			    1, MODEKEYEDIT_CURSORRIGHT },
	{ 'p',			    1, MODEKEYEDIT_PASTE },
	{ 's',			    1, MODEKEYEDIT_SWITCHMODESUBSTITUTE },
	{ 'w',			    1, MODEKEYEDIT_NEXTWORD },
	{ 'x',			    1, MODEKEYEDIT_DELETE },
	{ KEYC_BSPACE,		    1, MODEKEYEDIT_BACKSPACE },
	{ KEYC_DC,		    1, MODEKEYEDIT_DELETE },
	{ KEYC_DOWN,		    1, MODEKEYEDIT_HISTORYDOWN },
	{ KEYC_LEFT,		    1, MODEKEYEDIT_CURSORLEFT },
	{ KEYC_RIGHT,		    1, MODEKEYEDIT_CURSORRIGHT },
	{ KEYC_UP,		    1, MODEKEYEDIT_HISTORYUP },

	{ 0,			   -1, 0 }
};
struct mode_key_tree mode_key_tree_vi_edit;

/* vi choice selection keys. */
static const struct mode_key_entry mode_key_vi_choice[] = {
	{ '0' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '1' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '2' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '3' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '4' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '5' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '6' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '7' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '8' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '9' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '\002' /* C-b */,	    0, MODEKEYCHOICE_PAGEUP },
	{ '\003' /* C-c */,	    0, MODEKEYCHOICE_CANCEL },
	{ '\005' /* C-e */,	    0, MODEKEYCHOICE_SCROLLDOWN },
	{ '\006' /* C-f */,	    0, MODEKEYCHOICE_PAGEDOWN },
	{ '\031' /* C-y */,	    0, MODEKEYCHOICE_SCROLLUP },
	{ '\n',			    0, MODEKEYCHOICE_CHOOSE },
	{ '\r',			    0, MODEKEYCHOICE_CHOOSE },
	{ 'j',			    0, MODEKEYCHOICE_DOWN },
	{ 'k',			    0, MODEKEYCHOICE_UP },
	{ 'q',			    0, MODEKEYCHOICE_CANCEL },
	{ KEYC_HOME,                0, MODEKEYCHOICE_STARTOFLIST },
	{ 'g',                      0, MODEKEYCHOICE_STARTOFLIST },
	{ 'H',                      0, MODEKEYCHOICE_TOPLINE },
	{ 'L',                      0, MODEKEYCHOICE_BOTTOMLINE },
	{ 'G',                      0, MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_END,                 0, MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_BSPACE,		    0, MODEKEYCHOICE_BACKSPACE },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCHOICE_SCROLLDOWN },
	{ KEYC_DOWN,		    0, MODEKEYCHOICE_DOWN },
	{ KEYC_NPAGE,		    0, MODEKEYCHOICE_PAGEDOWN },
	{ KEYC_PPAGE,		    0, MODEKEYCHOICE_PAGEUP },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCHOICE_SCROLLUP },
	{ KEYC_UP,		    0, MODEKEYCHOICE_UP },
	{ ' ',			    0, MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_LEFT,		    0, MODEKEYCHOICE_TREE_COLLAPSE },
	{ KEYC_RIGHT,		    0, MODEKEYCHOICE_TREE_EXPAND },
	{ KEYC_LEFT | KEYC_CTRL,    0, MODEKEYCHOICE_TREE_COLLAPSE_ALL },
	{ KEYC_RIGHT | KEYC_CTRL,   0, MODEKEYCHOICE_TREE_EXPAND_ALL },
	{ KEYC_MOUSEDOWN1_PANE,     0, MODEKEYCHOICE_CHOOSE },
	{ KEYC_MOUSEDOWN3_PANE,     0, MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCHOICE_UP },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCHOICE_DOWN },

	{ 0,			   -1, 0 }
};
struct mode_key_tree mode_key_tree_vi_choice;

/* emacs editing keys. */
static const struct mode_key_entry mode_key_emacs_edit[] = {
	{ '\001' /* C-a */,	    0, MODEKEYEDIT_STARTOFLINE },
	{ '\002' /* C-b */,	    0, MODEKEYEDIT_CURSORLEFT },
	{ '\003' /* C-c */,	    0, MODEKEYEDIT_CANCEL },
	{ '\004' /* C-d */,	    0, MODEKEYEDIT_DELETE },
	{ '\005' /* C-e */,	    0, MODEKEYEDIT_ENDOFLINE },
	{ '\006' /* C-f */,	    0, MODEKEYEDIT_CURSORRIGHT },
	{ '\010' /* C-H */,	    0, MODEKEYEDIT_BACKSPACE },
	{ '\011' /* Tab */,	    0, MODEKEYEDIT_COMPLETE },
	{ '\013' /* C-k */,	    0, MODEKEYEDIT_DELETETOENDOFLINE },
	{ '\016' /* C-n */,	    0, MODEKEYEDIT_HISTORYDOWN },
	{ '\020' /* C-p */,	    0, MODEKEYEDIT_HISTORYUP },
	{ '\024' /* C-t */,	    0, MODEKEYEDIT_TRANSPOSECHARS },
	{ '\025' /* C-u */,	    0, MODEKEYEDIT_DELETELINE },
	{ '\027' /* C-w */,	    0, MODEKEYEDIT_DELETEWORD },
	{ '\031' /* C-y */,	    0, MODEKEYEDIT_PASTE },
	{ '\033' /* Escape */,	    0, MODEKEYEDIT_CANCEL },
	{ '\n',			    0, MODEKEYEDIT_ENTER },
	{ '\r',			    0, MODEKEYEDIT_ENTER },
	{ 'b' | KEYC_ESCAPE,	    0, MODEKEYEDIT_PREVIOUSWORD },
	{ 'f' | KEYC_ESCAPE,	    0, MODEKEYEDIT_NEXTWORDEND },
	{ 'm' | KEYC_ESCAPE,	    0, MODEKEYEDIT_STARTOFLINE },
	{ KEYC_BSPACE,		    0, MODEKEYEDIT_BACKSPACE },
	{ KEYC_DC,		    0, MODEKEYEDIT_DELETE },
	{ KEYC_DOWN,		    0, MODEKEYEDIT_HISTORYDOWN },
	{ KEYC_LEFT,		    0, MODEKEYEDIT_CURSORLEFT },
	{ KEYC_RIGHT,		    0, MODEKEYEDIT_CURSORRIGHT },
	{ KEYC_UP,		    0, MODEKEYEDIT_HISTORYUP },
	{ KEYC_HOME,		    0, MODEKEYEDIT_STARTOFLINE },
	{ KEYC_END,		    0, MODEKEYEDIT_ENDOFLINE },

	{ 0,			   -1, 0 }
};
struct mode_key_tree mode_key_tree_emacs_edit;

/* emacs choice selection keys. */
static const struct mode_key_entry mode_key_emacs_choice[] = {
	{ '0' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '1' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '2' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '3' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '4' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '5' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '6' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '7' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '8' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '9' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '\003' /* C-c */,	    0, MODEKEYCHOICE_CANCEL },
	{ '\016' /* C-n */,	    0, MODEKEYCHOICE_DOWN },
	{ '\020' /* C-p */,	    0, MODEKEYCHOICE_UP },
	{ '\026' /* C-v */,	    0, MODEKEYCHOICE_PAGEDOWN },
	{ '\033' /* Escape */,	    0, MODEKEYCHOICE_CANCEL },
	{ '\n',			    0, MODEKEYCHOICE_CHOOSE },
	{ '\r',			    0, MODEKEYCHOICE_CHOOSE },
	{ 'q',			    0, MODEKEYCHOICE_CANCEL },
	{ 'v' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_PAGEUP },
	{ KEYC_HOME,                0, MODEKEYCHOICE_STARTOFLIST },
	{ '<' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTOFLIST },
	{ 'R' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_TOPLINE },
	{ '>' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_END,                 0, MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_BSPACE,		    0, MODEKEYCHOICE_BACKSPACE },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCHOICE_SCROLLDOWN },
	{ KEYC_DOWN,		    0, MODEKEYCHOICE_DOWN },
	{ KEYC_NPAGE,		    0, MODEKEYCHOICE_PAGEDOWN },
	{ KEYC_PPAGE,		    0, MODEKEYCHOICE_PAGEUP },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCHOICE_SCROLLUP },
	{ KEYC_UP,		    0, MODEKEYCHOICE_UP },
	{ ' ',			    0, MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_LEFT,		    0, MODEKEYCHOICE_TREE_COLLAPSE },
	{ KEYC_RIGHT,		    0, MODEKEYCHOICE_TREE_EXPAND },
	{ KEYC_LEFT | KEYC_CTRL,    0, MODEKEYCHOICE_TREE_COLLAPSE_ALL },
	{ KEYC_RIGHT | KEYC_CTRL,   0, MODEKEYCHOICE_TREE_EXPAND_ALL },
	{ KEYC_MOUSEDOWN1_PANE,     0, MODEKEYCHOICE_CHOOSE },
	{ KEYC_MOUSEDOWN3_PANE,     0, MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCHOICE_UP },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCHOICE_DOWN },

	{ 0,			   -1, 0 }
};
struct mode_key_tree mode_key_tree_emacs_choice;

/* Table mapping key table names to default settings and trees. */
const struct mode_key_table mode_key_tables[] = {
	{ "vi-edit", mode_key_cmdstr_edit,
	  &mode_key_tree_vi_edit, mode_key_vi_edit },
	{ "vi-choice", mode_key_cmdstr_choice,
	  &mode_key_tree_vi_choice, mode_key_vi_choice },
	{ "emacs-edit", mode_key_cmdstr_edit,
	  &mode_key_tree_emacs_edit, mode_key_emacs_edit },
	{ "emacs-choice", mode_key_cmdstr_choice,
	  &mode_key_tree_emacs_choice, mode_key_emacs_choice },

	{ NULL, NULL, NULL, NULL }
};

RB_GENERATE(mode_key_tree, mode_key_binding, entry, mode_key_cmp);

int
mode_key_cmp(struct mode_key_binding *mbind1, struct mode_key_binding *mbind2)
{
	if (mbind1->mode < mbind2->mode)
		return (-1);
	if (mbind1->mode > mbind2->mode)
		return (1);
	if (mbind1->key < mbind2->key)
		return (-1);
	if (mbind1->key > mbind2->key)
		return (1);
	return (0);
}

const char *
mode_key_tostring(const struct mode_key_cmdstr *cmdstr, enum mode_key_cmd cmd)
{
	for (; cmdstr->name != NULL; cmdstr++) {
		if (cmdstr->cmd == cmd)
			return (cmdstr->name);
	}
	return (NULL);
}

enum mode_key_cmd
mode_key_fromstring(const struct mode_key_cmdstr *cmdstr, const char *name)
{
	for (; cmdstr->name != NULL; cmdstr++) {
		if (strcasecmp(cmdstr->name, name) == 0)
			return (cmdstr->cmd);
	}
	return (MODEKEY_NONE);
}

const struct mode_key_table *
mode_key_findtable(const char *name)
{
	const struct mode_key_table	*mtab;

	for (mtab = mode_key_tables; mtab->name != NULL; mtab++) {
		if (strcasecmp(name, mtab->name) == 0)
			return (mtab);
	}
	return (NULL);
}

void
mode_key_init_trees(void)
{
	const struct mode_key_table	*mtab;
	const struct mode_key_entry	*ment;
	struct mode_key_binding		*mbind;

	for (mtab = mode_key_tables; mtab->name != NULL; mtab++) {
		RB_INIT(mtab->tree);
		for (ment = mtab->table; ment->mode != -1; ment++) {
			mbind = xmalloc(sizeof *mbind);
			mbind->key = ment->key;
			mbind->mode = ment->mode;
			mbind->cmd = ment->cmd;
			RB_INSERT(mode_key_tree, mtab->tree, mbind);
		}
	}
}

void
mode_key_init(struct mode_key_data *mdata, struct mode_key_tree *mtree)
{
	mdata->tree = mtree;
	mdata->mode = 0;
}

enum mode_key_cmd
mode_key_lookup(struct mode_key_data *mdata, key_code key)
{
	struct mode_key_binding	*mbind, mtmp;

	mtmp.key = key;
	mtmp.mode = mdata->mode;
	if ((mbind = RB_FIND(mode_key_tree, mdata->tree, &mtmp)) == NULL) {
		if (mdata->mode != 0)
			return (MODEKEY_NONE);
		return (MODEKEY_OTHER);
	}

	switch (mbind->cmd) {
	case MODEKEYEDIT_SWITCHMODE:
	case MODEKEYEDIT_SWITCHMODEAPPEND:
	case MODEKEYEDIT_SWITCHMODEAPPENDLINE:
	case MODEKEYEDIT_SWITCHMODEBEGINLINE:
	case MODEKEYEDIT_SWITCHMODECHANGELINE:
	case MODEKEYEDIT_SWITCHMODESUBSTITUTE:
	case MODEKEYEDIT_SWITCHMODESUBSTITUTELINE:
		mdata->mode = 1 - mdata->mode;
		/* FALLTHROUGH */
	default:
		return (mbind->cmd);
	}
}
