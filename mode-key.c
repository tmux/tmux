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

/* Command to string mapping. */
struct mode_key_cmdstr {
	enum mode_key_cmd	 cmd;
	const char		*name;
};

/* Entry in the default mode key tables. */
struct mode_key_entry {
	key_code		key;
	enum mode_key_cmd	cmd;
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

/* vi choice selection keys. */
static const struct mode_key_entry mode_key_vi_choice[] = {
	{ '0' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '1' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '2' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '3' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '4' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '5' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '6' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '7' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '8' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '9' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '\002' /* C-b */,	    MODEKEYCHOICE_PAGEUP },
	{ '\003' /* C-c */,	    MODEKEYCHOICE_CANCEL },
	{ '\005' /* C-e */,	    MODEKEYCHOICE_SCROLLDOWN },
	{ '\006' /* C-f */,	    MODEKEYCHOICE_PAGEDOWN },
	{ '\031' /* C-y */,	    MODEKEYCHOICE_SCROLLUP },
	{ '\n',			    MODEKEYCHOICE_CHOOSE },
	{ '\r',			    MODEKEYCHOICE_CHOOSE },
	{ 'j',			    MODEKEYCHOICE_DOWN },
	{ 'k',			    MODEKEYCHOICE_UP },
	{ 'q',			    MODEKEYCHOICE_CANCEL },
	{ KEYC_HOME,                MODEKEYCHOICE_STARTOFLIST },
	{ 'g',                      MODEKEYCHOICE_STARTOFLIST },
	{ 'H',                      MODEKEYCHOICE_TOPLINE },
	{ 'L',                      MODEKEYCHOICE_BOTTOMLINE },
	{ 'G',                      MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_END,                 MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_BSPACE,		    MODEKEYCHOICE_BACKSPACE },
	{ KEYC_DOWN | KEYC_CTRL,    MODEKEYCHOICE_SCROLLDOWN },
	{ KEYC_DOWN,		    MODEKEYCHOICE_DOWN },
	{ KEYC_NPAGE,		    MODEKEYCHOICE_PAGEDOWN },
	{ KEYC_PPAGE,		    MODEKEYCHOICE_PAGEUP },
	{ KEYC_UP | KEYC_CTRL,	    MODEKEYCHOICE_SCROLLUP },
	{ KEYC_UP,		    MODEKEYCHOICE_UP },
	{ ' ',			    MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_LEFT,		    MODEKEYCHOICE_TREE_COLLAPSE },
	{ KEYC_RIGHT,		    MODEKEYCHOICE_TREE_EXPAND },
	{ KEYC_LEFT | KEYC_CTRL,    MODEKEYCHOICE_TREE_COLLAPSE_ALL },
	{ KEYC_RIGHT | KEYC_CTRL,   MODEKEYCHOICE_TREE_EXPAND_ALL },
	{ KEYC_MOUSEDOWN1_PANE,     MODEKEYCHOICE_CHOOSE },
	{ KEYC_MOUSEDOWN3_PANE,     MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_WHEELUP_PANE,        MODEKEYCHOICE_UP },
	{ KEYC_WHEELDOWN_PANE,      MODEKEYCHOICE_DOWN },

	{ KEYC_NONE, -1 }
};
struct mode_key_tree mode_key_tree_vi_choice;

/* emacs choice selection keys. */
static const struct mode_key_entry mode_key_emacs_choice[] = {
	{ '0' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '1' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '2' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '3' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '4' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '5' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '6' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '7' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '8' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '9' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTNUMBERPREFIX },
	{ '\003' /* C-c */,	    MODEKEYCHOICE_CANCEL },
	{ '\016' /* C-n */,	    MODEKEYCHOICE_DOWN },
	{ '\020' /* C-p */,	    MODEKEYCHOICE_UP },
	{ '\026' /* C-v */,	    MODEKEYCHOICE_PAGEDOWN },
	{ '\033' /* Escape */,	    MODEKEYCHOICE_CANCEL },
	{ '\n',			    MODEKEYCHOICE_CHOOSE },
	{ '\r',			    MODEKEYCHOICE_CHOOSE },
	{ 'q',			    MODEKEYCHOICE_CANCEL },
	{ 'v' | KEYC_ESCAPE,	    MODEKEYCHOICE_PAGEUP },
	{ KEYC_HOME,                MODEKEYCHOICE_STARTOFLIST },
	{ '<' | KEYC_ESCAPE,	    MODEKEYCHOICE_STARTOFLIST },
	{ 'R' | KEYC_ESCAPE,	    MODEKEYCHOICE_TOPLINE },
	{ '>' | KEYC_ESCAPE,	    MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_END,                 MODEKEYCHOICE_ENDOFLIST },
	{ KEYC_BSPACE,		    MODEKEYCHOICE_BACKSPACE },
	{ KEYC_DOWN | KEYC_CTRL,    MODEKEYCHOICE_SCROLLDOWN },
	{ KEYC_DOWN,		    MODEKEYCHOICE_DOWN },
	{ KEYC_NPAGE,		    MODEKEYCHOICE_PAGEDOWN },
	{ KEYC_PPAGE,		    MODEKEYCHOICE_PAGEUP },
	{ KEYC_UP | KEYC_CTRL,	    MODEKEYCHOICE_SCROLLUP },
	{ KEYC_UP,		    MODEKEYCHOICE_UP },
	{ ' ',			    MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_LEFT,		    MODEKEYCHOICE_TREE_COLLAPSE },
	{ KEYC_RIGHT,		    MODEKEYCHOICE_TREE_EXPAND },
	{ KEYC_LEFT | KEYC_CTRL,    MODEKEYCHOICE_TREE_COLLAPSE_ALL },
	{ KEYC_RIGHT | KEYC_CTRL,   MODEKEYCHOICE_TREE_EXPAND_ALL },
	{ KEYC_MOUSEDOWN1_PANE,     MODEKEYCHOICE_CHOOSE },
	{ KEYC_MOUSEDOWN3_PANE,     MODEKEYCHOICE_TREE_TOGGLE },
	{ KEYC_WHEELUP_PANE,        MODEKEYCHOICE_UP },
	{ KEYC_WHEELDOWN_PANE,      MODEKEYCHOICE_DOWN },

	{ KEYC_NONE, -1 }
};
struct mode_key_tree mode_key_tree_emacs_choice;

/* Table mapping key table names to default settings and trees. */
static const struct mode_key_table mode_key_tables[] = {
	{ "vi-choice", mode_key_cmdstr_choice,
	  &mode_key_tree_vi_choice, mode_key_vi_choice },
	{ "emacs-choice", mode_key_cmdstr_choice,
	  &mode_key_tree_emacs_choice, mode_key_emacs_choice },

	{ NULL, NULL, NULL, NULL }
};

RB_GENERATE(mode_key_tree, mode_key_binding, entry, mode_key_cmp);

int
mode_key_cmp(struct mode_key_binding *mbind1, struct mode_key_binding *mbind2)
{
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
		for (ment = mtab->table; ment->key != KEYC_NONE; ment++) {
			mbind = xmalloc(sizeof *mbind);
			mbind->key = ment->key;
			mbind->cmd = ment->cmd;
			RB_INSERT(mode_key_tree, mtab->tree, mbind);
		}
	}
}

void
mode_key_init(struct mode_key_data *mdata, struct mode_key_tree *mtree)
{
	mdata->tree = mtree;
}

enum mode_key_cmd
mode_key_lookup(struct mode_key_data *mdata, key_code key)
{
	struct mode_key_binding	*mbind, mtmp;

	mtmp.key = key;
	if ((mbind = RB_FIND(mode_key_tree, mdata->tree, &mtmp)) == NULL)
		return (MODEKEY_OTHER);
	return (mbind->cmd);
}
