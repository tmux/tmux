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
	u_int			repeat;
};

/* Edit keys command strings. */
const struct mode_key_cmdstr mode_key_cmdstr_edit[] = {
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
const struct mode_key_cmdstr mode_key_cmdstr_choice[] = {
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

/* Copy keys command strings. */
const struct mode_key_cmdstr mode_key_cmdstr_copy[] = {
	{ MODEKEYCOPY_APPENDSELECTION, "append-selection" },
	{ MODEKEYCOPY_BACKTOINDENTATION, "back-to-indentation" },
	{ MODEKEYCOPY_BOTTOMLINE, "bottom-line" },
	{ MODEKEYCOPY_CANCEL, "cancel" },
	{ MODEKEYCOPY_CLEARSELECTION, "clear-selection" },
	{ MODEKEYCOPY_COPYPIPE, "copy-pipe" },
	{ MODEKEYCOPY_COPYLINE, "copy-line" },
	{ MODEKEYCOPY_COPYENDOFLINE, "copy-end-of-line" },
	{ MODEKEYCOPY_COPYSELECTION, "copy-selection" },
	{ MODEKEYCOPY_DOWN, "cursor-down" },
	{ MODEKEYCOPY_ENDOFLINE, "end-of-line" },
	{ MODEKEYCOPY_GOTOLINE, "goto-line" },
	{ MODEKEYCOPY_HALFPAGEDOWN, "halfpage-down" },
	{ MODEKEYCOPY_HALFPAGEUP, "halfpage-up" },
	{ MODEKEYCOPY_HISTORYBOTTOM, "history-bottom" },
	{ MODEKEYCOPY_HISTORYTOP, "history-top" },
	{ MODEKEYCOPY_JUMP, "jump-forward" },
	{ MODEKEYCOPY_JUMPAGAIN, "jump-again" },
	{ MODEKEYCOPY_JUMPREVERSE, "jump-reverse" },
	{ MODEKEYCOPY_JUMPBACK, "jump-backward" },
	{ MODEKEYCOPY_JUMPTO, "jump-to-forward" },
	{ MODEKEYCOPY_JUMPTOBACK, "jump-to-backward" },
	{ MODEKEYCOPY_LEFT, "cursor-left" },
	{ MODEKEYCOPY_RECTANGLETOGGLE, "rectangle-toggle" },
	{ MODEKEYCOPY_MIDDLELINE, "middle-line" },
	{ MODEKEYCOPY_NEXTPAGE, "page-down" },
	{ MODEKEYCOPY_NEXTPARAGRAPH, "next-paragraph" },
	{ MODEKEYCOPY_NEXTSPACE, "next-space" },
	{ MODEKEYCOPY_NEXTSPACEEND, "next-space-end" },
	{ MODEKEYCOPY_NEXTWORD, "next-word" },
	{ MODEKEYCOPY_NEXTWORDEND, "next-word-end" },
	{ MODEKEYCOPY_OTHEREND, "other-end" },
	{ MODEKEYCOPY_PREVIOUSPAGE, "page-up" },
	{ MODEKEYCOPY_PREVIOUSPARAGRAPH, "previous-paragraph" },
	{ MODEKEYCOPY_PREVIOUSSPACE, "previous-space" },
	{ MODEKEYCOPY_PREVIOUSWORD, "previous-word" },
	{ MODEKEYCOPY_RIGHT, "cursor-right" },
	{ MODEKEYCOPY_SCROLLDOWN, "scroll-down" },
	{ MODEKEYCOPY_SCROLLUP, "scroll-up" },
	{ MODEKEYCOPY_SEARCHAGAIN, "search-again" },
	{ MODEKEYCOPY_SEARCHDOWN, "search-forward" },
	{ MODEKEYCOPY_SEARCHREVERSE, "search-reverse" },
	{ MODEKEYCOPY_SEARCHUP, "search-backward" },
	{ MODEKEYCOPY_SELECTLINE, "select-line" },
	{ MODEKEYCOPY_STARTNAMEDBUFFER, "start-named-buffer" },
	{ MODEKEYCOPY_STARTNUMBERPREFIX, "start-number-prefix" },
	{ MODEKEYCOPY_STARTOFLINE, "start-of-line" },
	{ MODEKEYCOPY_STARTSELECTION, "begin-selection" },
	{ MODEKEYCOPY_TOPLINE, "top-line" },
	{ MODEKEYCOPY_UP, "cursor-up" },

	{ 0, NULL }
};

/* vi editing keys. */
const struct mode_key_entry mode_key_vi_edit[] = {
	{ '\003' /* C-c */,	    0, MODEKEYEDIT_CANCEL, 1 },
	{ '\010' /* C-h */,	    0, MODEKEYEDIT_BACKSPACE, 1 },
	{ '\011' /* Tab */,	    0, MODEKEYEDIT_COMPLETE, 1 },
	{ '\025' /* C-u */,	    0, MODEKEYEDIT_DELETELINE, 1 },
	{ '\027' /* C-w */,	    0, MODEKEYEDIT_DELETEWORD, 1 },
	{ '\033' /* Escape */,	    0, MODEKEYEDIT_SWITCHMODE, 1 },
	{ '\n',			    0, MODEKEYEDIT_ENTER, 1 },
	{ '\r',			    0, MODEKEYEDIT_ENTER, 1 },
	{ KEYC_BSPACE,		    0, MODEKEYEDIT_BACKSPACE, 1 },
	{ KEYC_DC,		    0, MODEKEYEDIT_DELETE, 1 },
	{ KEYC_DOWN,		    0, MODEKEYEDIT_HISTORYDOWN, 1 },
	{ KEYC_LEFT,		    0, MODEKEYEDIT_CURSORLEFT, 1 },
	{ KEYC_RIGHT,		    0, MODEKEYEDIT_CURSORRIGHT, 1 },
	{ KEYC_UP,		    0, MODEKEYEDIT_HISTORYUP, 1 },
	{ KEYC_HOME,		    0, MODEKEYEDIT_STARTOFLINE, 1 },
	{ KEYC_END,		    0, MODEKEYEDIT_ENDOFLINE, 1 },

	{ '$',			    1, MODEKEYEDIT_ENDOFLINE, 1 },
	{ '0',			    1, MODEKEYEDIT_STARTOFLINE, 1 },
	{ 'A',			    1, MODEKEYEDIT_SWITCHMODEAPPENDLINE, 1 },
	{ 'B',			    1, MODEKEYEDIT_PREVIOUSSPACE, 1 },
	{ 'C',			    1, MODEKEYEDIT_SWITCHMODECHANGELINE, 1 },
	{ 'D',			    1, MODEKEYEDIT_DELETETOENDOFLINE, 1 },
	{ 'E',			    1, MODEKEYEDIT_NEXTSPACEEND, 1 },
	{ 'I',			    1, MODEKEYEDIT_SWITCHMODEBEGINLINE, 1 },
	{ 'S',			    1, MODEKEYEDIT_SWITCHMODESUBSTITUTELINE, 1 },
	{ 'W',			    1, MODEKEYEDIT_NEXTSPACE, 1 },
	{ 'X',			    1, MODEKEYEDIT_BACKSPACE, 1 },
	{ '\003' /* C-c */,	    1, MODEKEYEDIT_CANCEL, 1 },
	{ '\010' /* C-h */,	    1, MODEKEYEDIT_BACKSPACE, 1 },
	{ '\n',			    1, MODEKEYEDIT_ENTER, 1 },
	{ '\r',			    1, MODEKEYEDIT_ENTER, 1 },
	{ '^',			    1, MODEKEYEDIT_STARTOFLINE, 1 },
	{ 'a',			    1, MODEKEYEDIT_SWITCHMODEAPPEND, 1 },
	{ 'b',			    1, MODEKEYEDIT_PREVIOUSWORD, 1 },
	{ 'd',			    1, MODEKEYEDIT_DELETELINE, 1 },
	{ 'e',			    1, MODEKEYEDIT_NEXTWORDEND, 1 },
	{ 'h',			    1, MODEKEYEDIT_CURSORLEFT, 1 },
	{ 'i',			    1, MODEKEYEDIT_SWITCHMODE, 1 },
	{ 'j',			    1, MODEKEYEDIT_HISTORYDOWN, 1 },
	{ 'k',			    1, MODEKEYEDIT_HISTORYUP, 1 },
	{ 'l',			    1, MODEKEYEDIT_CURSORRIGHT, 1 },
	{ 'p',			    1, MODEKEYEDIT_PASTE, 1 },
	{ 's',			    1, MODEKEYEDIT_SWITCHMODESUBSTITUTE, 1 },
	{ 'w',			    1, MODEKEYEDIT_NEXTWORD, 1 },
	{ 'x',			    1, MODEKEYEDIT_DELETE, 1 },
	{ KEYC_BSPACE,		    1, MODEKEYEDIT_BACKSPACE, 1 },
	{ KEYC_DC,		    1, MODEKEYEDIT_DELETE, 1 },
	{ KEYC_DOWN,		    1, MODEKEYEDIT_HISTORYDOWN, 1 },
	{ KEYC_LEFT,		    1, MODEKEYEDIT_CURSORLEFT, 1 },
	{ KEYC_RIGHT,		    1, MODEKEYEDIT_CURSORRIGHT, 1 },
	{ KEYC_UP,		    1, MODEKEYEDIT_HISTORYUP, 1 },

	{ 0,			   -1, 0, 1 }
};
struct mode_key_tree mode_key_tree_vi_edit;

/* vi choice selection keys. */
const struct mode_key_entry mode_key_vi_choice[] = {
	{ '0' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '1' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '2' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '3' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '4' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '5' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '6' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '7' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '8' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '9' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '\002' /* C-b */,	    0, MODEKEYCHOICE_PAGEUP, 1 },
	{ '\003' /* C-c */,	    0, MODEKEYCHOICE_CANCEL, 1 },
	{ '\005' /* C-e */,	    0, MODEKEYCHOICE_SCROLLDOWN, 1 },
	{ '\006' /* C-f */,	    0, MODEKEYCHOICE_PAGEDOWN, 1 },
	{ '\031' /* C-y */,	    0, MODEKEYCHOICE_SCROLLUP, 1 },
	{ '\n',			    0, MODEKEYCHOICE_CHOOSE, 1 },
	{ '\r',			    0, MODEKEYCHOICE_CHOOSE, 1 },
	{ 'j',			    0, MODEKEYCHOICE_DOWN, 1 },
	{ 'k',			    0, MODEKEYCHOICE_UP, 1 },
	{ 'q',			    0, MODEKEYCHOICE_CANCEL, 1 },
	{ KEYC_HOME,                0, MODEKEYCHOICE_STARTOFLIST, 1 },
	{ 'g',                      0, MODEKEYCHOICE_STARTOFLIST, 1 },
	{ 'H',                      0, MODEKEYCHOICE_TOPLINE, 1 },
	{ 'L',                      0, MODEKEYCHOICE_BOTTOMLINE, 1 },
	{ 'G',                      0, MODEKEYCHOICE_ENDOFLIST, 1 },
	{ KEYC_END,                 0, MODEKEYCHOICE_ENDOFLIST, 1 },
	{ KEYC_BSPACE,		    0, MODEKEYCHOICE_BACKSPACE, 1 },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCHOICE_SCROLLDOWN, 1 },
	{ KEYC_DOWN,		    0, MODEKEYCHOICE_DOWN, 1 },
	{ KEYC_NPAGE,		    0, MODEKEYCHOICE_PAGEDOWN, 1 },
	{ KEYC_PPAGE,		    0, MODEKEYCHOICE_PAGEUP, 1 },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCHOICE_SCROLLUP, 1 },
	{ KEYC_UP,		    0, MODEKEYCHOICE_UP, 1 },
	{ ' ',			    0, MODEKEYCHOICE_TREE_TOGGLE, 1 },
	{ KEYC_LEFT,		    0, MODEKEYCHOICE_TREE_COLLAPSE, 1 },
	{ KEYC_RIGHT,		    0, MODEKEYCHOICE_TREE_EXPAND, 1 },
	{ KEYC_LEFT | KEYC_CTRL,    0, MODEKEYCHOICE_TREE_COLLAPSE_ALL, 1 },
	{ KEYC_RIGHT | KEYC_CTRL,   0, MODEKEYCHOICE_TREE_EXPAND_ALL, 1 },
	{ KEYC_MOUSEDOWN1_PANE,     0, MODEKEYCHOICE_CHOOSE, 1 },
	{ KEYC_MOUSEDOWN3_PANE,     0, MODEKEYCHOICE_TREE_TOGGLE, 1 },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCHOICE_UP, 1 },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCHOICE_DOWN, 1 },

	{ 0,			   -1, 0, 1 }
};
struct mode_key_tree mode_key_tree_vi_choice;

/* vi copy mode keys. */
const struct mode_key_entry mode_key_vi_copy[] = {
	{ ' ',			    0, MODEKEYCOPY_STARTSELECTION, 1 },
	{ '"',			    0, MODEKEYCOPY_STARTNAMEDBUFFER, 1 },
	{ '$',			    0, MODEKEYCOPY_ENDOFLINE, 1 },
	{ ',',			    0, MODEKEYCOPY_JUMPREVERSE, 1 },
	{ ';',			    0, MODEKEYCOPY_JUMPAGAIN, 1 },
	{ '/',			    0, MODEKEYCOPY_SEARCHDOWN, 1 },
	{ '0',			    0, MODEKEYCOPY_STARTOFLINE, 1 },
	{ '1',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '2',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '3',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '4',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '5',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '6',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '7',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '8',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '9',			    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ ':',			    0, MODEKEYCOPY_GOTOLINE, 1 },
	{ '?',			    0, MODEKEYCOPY_SEARCHUP, 1 },
	{ 'A',			    0, MODEKEYCOPY_APPENDSELECTION, 1 },
	{ 'B',			    0, MODEKEYCOPY_PREVIOUSSPACE, 1 },
	{ 'D',			    0, MODEKEYCOPY_COPYENDOFLINE, 1 },
	{ 'E',			    0, MODEKEYCOPY_NEXTSPACEEND, 1 },
	{ 'F',			    0, MODEKEYCOPY_JUMPBACK, 1 },
	{ 'G',			    0, MODEKEYCOPY_HISTORYBOTTOM, 1 },
	{ 'H',			    0, MODEKEYCOPY_TOPLINE, 1 },
	{ 'J',			    0, MODEKEYCOPY_SCROLLDOWN, 1 },
	{ 'K',			    0, MODEKEYCOPY_SCROLLUP, 1 },
	{ 'L',			    0, MODEKEYCOPY_BOTTOMLINE, 1 },
	{ 'M',			    0, MODEKEYCOPY_MIDDLELINE, 1 },
	{ 'N',			    0, MODEKEYCOPY_SEARCHREVERSE, 1 },
	{ 'T',			    0, MODEKEYCOPY_JUMPTOBACK, 1 },
	{ 'V',			    0, MODEKEYCOPY_SELECTLINE, 1 },
	{ 'W',			    0, MODEKEYCOPY_NEXTSPACE, 1 },
	{ '\002' /* C-b */,	    0, MODEKEYCOPY_PREVIOUSPAGE, 1 },
	{ '\003' /* C-c */,	    0, MODEKEYCOPY_CANCEL, 1 },
	{ '\004' /* C-d */,	    0, MODEKEYCOPY_HALFPAGEDOWN, 1 },
	{ '\005' /* C-e */,	    0, MODEKEYCOPY_SCROLLDOWN, 1 },
	{ '\006' /* C-f */,	    0, MODEKEYCOPY_NEXTPAGE, 1 },
	{ '\010' /* C-h */,	    0, MODEKEYCOPY_LEFT, 1 },
	{ '\025' /* C-u */,	    0, MODEKEYCOPY_HALFPAGEUP, 1 },
	{ '\031' /* C-y */,	    0, MODEKEYCOPY_SCROLLUP, 1 },
	{ '\033' /* Escape */,	    0, MODEKEYCOPY_CLEARSELECTION, 1 },
	{ '\n',			    0, MODEKEYCOPY_COPYSELECTION, 1 },
	{ '\r',			    0, MODEKEYCOPY_COPYSELECTION, 1 },
	{ '^',			    0, MODEKEYCOPY_BACKTOINDENTATION, 1 },
	{ 'b',			    0, MODEKEYCOPY_PREVIOUSWORD, 1 },
	{ 'e',			    0, MODEKEYCOPY_NEXTWORDEND, 1 },
	{ 'f',			    0, MODEKEYCOPY_JUMP, 1 },
	{ 'g',			    0, MODEKEYCOPY_HISTORYTOP, 1 },
	{ 'h',			    0, MODEKEYCOPY_LEFT, 1 },
	{ 'j',			    0, MODEKEYCOPY_DOWN, 1 },
	{ 'k',			    0, MODEKEYCOPY_UP, 1 },
	{ 'l',			    0, MODEKEYCOPY_RIGHT, 1 },
	{ 'n',			    0, MODEKEYCOPY_SEARCHAGAIN, 1 },
	{ 'o',			    0, MODEKEYCOPY_OTHEREND, 1 },
	{ 't',			    0, MODEKEYCOPY_JUMPTO, 1 },
	{ 'q',			    0, MODEKEYCOPY_CANCEL, 1 },
	{ 'v',			    0, MODEKEYCOPY_RECTANGLETOGGLE, 1 },
	{ 'w',			    0, MODEKEYCOPY_NEXTWORD, 1 },
	{ '{',			    0, MODEKEYCOPY_PREVIOUSPARAGRAPH, 1 },
	{ '}',			    0, MODEKEYCOPY_NEXTPARAGRAPH, 1 },
	{ KEYC_BSPACE,		    0, MODEKEYCOPY_LEFT, 1 },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCOPY_SCROLLDOWN, 1 },
	{ KEYC_DOWN,		    0, MODEKEYCOPY_DOWN, 1 },
	{ KEYC_LEFT,		    0, MODEKEYCOPY_LEFT, 1 },
	{ KEYC_NPAGE,		    0, MODEKEYCOPY_NEXTPAGE, 1 },
	{ KEYC_PPAGE,		    0, MODEKEYCOPY_PREVIOUSPAGE, 1 },
	{ KEYC_RIGHT,		    0, MODEKEYCOPY_RIGHT, 1 },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCOPY_SCROLLUP, 1 },
	{ KEYC_UP,		    0, MODEKEYCOPY_UP, 1 },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCOPY_SCROLLUP, 1 },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCOPY_SCROLLDOWN, 1 },
	{ KEYC_MOUSEDRAG1_PANE,     0, MODEKEYCOPY_STARTSELECTION, 1 },
	{ KEYC_MOUSEDRAGEND1_PANE,  0, MODEKEYCOPY_COPYSELECTION, 1 },

	{ 0,			   -1, 0, 1 }
};
struct mode_key_tree mode_key_tree_vi_copy;

/* emacs editing keys. */
const struct mode_key_entry mode_key_emacs_edit[] = {
	{ '\001' /* C-a */,	    0, MODEKEYEDIT_STARTOFLINE, 1 },
	{ '\002' /* C-b */,	    0, MODEKEYEDIT_CURSORLEFT, 1 },
	{ '\003' /* C-c */,	    0, MODEKEYEDIT_CANCEL, 1 },
	{ '\004' /* C-d */,	    0, MODEKEYEDIT_DELETE, 1 },
	{ '\005' /* C-e */,	    0, MODEKEYEDIT_ENDOFLINE, 1 },
	{ '\006' /* C-f */,	    0, MODEKEYEDIT_CURSORRIGHT, 1 },
	{ '\010' /* C-H */,	    0, MODEKEYEDIT_BACKSPACE, 1 },
	{ '\011' /* Tab */,	    0, MODEKEYEDIT_COMPLETE, 1 },
	{ '\013' /* C-k */,	    0, MODEKEYEDIT_DELETETOENDOFLINE, 1 },
	{ '\016' /* C-n */,	    0, MODEKEYEDIT_HISTORYDOWN, 1 },
	{ '\020' /* C-p */,	    0, MODEKEYEDIT_HISTORYUP, 1 },
	{ '\024' /* C-t */,	    0, MODEKEYEDIT_TRANSPOSECHARS, 1 },
	{ '\025' /* C-u */,	    0, MODEKEYEDIT_DELETELINE, 1 },
	{ '\027' /* C-w */,	    0, MODEKEYEDIT_DELETEWORD, 1 },
	{ '\031' /* C-y */,	    0, MODEKEYEDIT_PASTE, 1 },
	{ '\033' /* Escape */,	    0, MODEKEYEDIT_CANCEL, 1 },
	{ '\n',			    0, MODEKEYEDIT_ENTER, 1 },
	{ '\r',			    0, MODEKEYEDIT_ENTER, 1 },
	{ 'b' | KEYC_ESCAPE,	    0, MODEKEYEDIT_PREVIOUSWORD, 1 },
	{ 'f' | KEYC_ESCAPE,	    0, MODEKEYEDIT_NEXTWORDEND, 1 },
	{ 'm' | KEYC_ESCAPE,	    0, MODEKEYEDIT_STARTOFLINE, 1 },
	{ KEYC_BSPACE,		    0, MODEKEYEDIT_BACKSPACE, 1 },
	{ KEYC_DC,		    0, MODEKEYEDIT_DELETE, 1 },
	{ KEYC_DOWN,		    0, MODEKEYEDIT_HISTORYDOWN, 1 },
	{ KEYC_LEFT,		    0, MODEKEYEDIT_CURSORLEFT, 1 },
	{ KEYC_RIGHT,		    0, MODEKEYEDIT_CURSORRIGHT, 1 },
	{ KEYC_UP,		    0, MODEKEYEDIT_HISTORYUP, 1 },
	{ KEYC_HOME,		    0, MODEKEYEDIT_STARTOFLINE, 1 },
	{ KEYC_END,		    0, MODEKEYEDIT_ENDOFLINE, 1 },

	{ 0,			   -1, 0, 1 }
};
struct mode_key_tree mode_key_tree_emacs_edit;

/* emacs choice selection keys. */
const struct mode_key_entry mode_key_emacs_choice[] = {
	{ '0' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '1' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '2' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '3' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '4' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '5' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '6' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '7' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '8' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '9' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTNUMBERPREFIX, 1 },
	{ '\003' /* C-c */,	    0, MODEKEYCHOICE_CANCEL, 1 },
	{ '\016' /* C-n */,	    0, MODEKEYCHOICE_DOWN, 1 },
	{ '\020' /* C-p */,	    0, MODEKEYCHOICE_UP, 1 },
	{ '\026' /* C-v */,	    0, MODEKEYCHOICE_PAGEDOWN, 1 },
	{ '\033' /* Escape */,	    0, MODEKEYCHOICE_CANCEL, 1 },
	{ '\n',			    0, MODEKEYCHOICE_CHOOSE, 1 },
	{ '\r',			    0, MODEKEYCHOICE_CHOOSE, 1 },
	{ 'q',			    0, MODEKEYCHOICE_CANCEL, 1 },
	{ 'v' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_PAGEUP, 1 },
	{ KEYC_HOME,                0, MODEKEYCHOICE_STARTOFLIST, 1 },
	{ '<' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_STARTOFLIST, 1 },
	{ 'R' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_TOPLINE, 1 },
	{ '>' | KEYC_ESCAPE,	    0, MODEKEYCHOICE_ENDOFLIST, 1 },
	{ KEYC_END,                 0, MODEKEYCHOICE_ENDOFLIST, 1 },
	{ KEYC_BSPACE,		    0, MODEKEYCHOICE_BACKSPACE, 1 },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCHOICE_SCROLLDOWN, 1 },
	{ KEYC_DOWN,		    0, MODEKEYCHOICE_DOWN, 1 },
	{ KEYC_NPAGE,		    0, MODEKEYCHOICE_PAGEDOWN, 1 },
	{ KEYC_PPAGE,		    0, MODEKEYCHOICE_PAGEUP, 1 },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCHOICE_SCROLLUP, 1 },
	{ KEYC_UP,		    0, MODEKEYCHOICE_UP, 1 },
	{ ' ',			    0, MODEKEYCHOICE_TREE_TOGGLE, 1 },
	{ KEYC_LEFT,		    0, MODEKEYCHOICE_TREE_COLLAPSE, 1 },
	{ KEYC_RIGHT,		    0, MODEKEYCHOICE_TREE_EXPAND, 1 },
	{ KEYC_LEFT | KEYC_CTRL,    0, MODEKEYCHOICE_TREE_COLLAPSE_ALL, 1 },
	{ KEYC_RIGHT | KEYC_CTRL,   0, MODEKEYCHOICE_TREE_EXPAND_ALL, 1 },
	{ KEYC_MOUSEDOWN1_PANE,     0, MODEKEYCHOICE_CHOOSE, 1 },
	{ KEYC_MOUSEDOWN3_PANE,     0, MODEKEYCHOICE_TREE_TOGGLE, 1 },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCHOICE_UP, 5 },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCHOICE_DOWN, 5 },

	{ 0,			   -1, 0, 1 }
};
struct mode_key_tree mode_key_tree_emacs_choice;

/* emacs copy mode keys. */
const struct mode_key_entry mode_key_emacs_copy[] = {
	{ ' ',			    0, MODEKEYCOPY_NEXTPAGE, 1 },
	{ ',',			    0, MODEKEYCOPY_JUMPREVERSE, 1 },
	{ ';',			    0, MODEKEYCOPY_JUMPAGAIN, 1 },
	{ '1' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '2' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '3' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '4' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '5' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '6' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '7' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '8' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '9' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX, 1 },
	{ '<' | KEYC_ESCAPE,	    0, MODEKEYCOPY_HISTORYTOP, 1 },
	{ '>' | KEYC_ESCAPE,	    0, MODEKEYCOPY_HISTORYBOTTOM, 1 },
	{ 'F',			    0, MODEKEYCOPY_JUMPBACK, 1 },
	{ 'N',			    0, MODEKEYCOPY_SEARCHREVERSE, 1 },
	{ 'R' | KEYC_ESCAPE,	    0, MODEKEYCOPY_TOPLINE, 1 },
	{ 'R',			    0, MODEKEYCOPY_RECTANGLETOGGLE, 1 },
	{ 'T',			    0, MODEKEYCOPY_JUMPTOBACK, 1 },
	{ '\000' /* C-Space */,	    0, MODEKEYCOPY_STARTSELECTION, 1 },
	{ '\001' /* C-a */,	    0, MODEKEYCOPY_STARTOFLINE, 1 },
	{ '\002' /* C-b */,	    0, MODEKEYCOPY_LEFT, 1 },
	{ '\003' /* C-c */,	    0, MODEKEYCOPY_CANCEL, 1 },
	{ '\005' /* C-e */,	    0, MODEKEYCOPY_ENDOFLINE, 1 },
	{ '\006' /* C-f */,	    0, MODEKEYCOPY_RIGHT, 1 },
	{ '\007' /* C-g */,	    0, MODEKEYCOPY_CLEARSELECTION, 1 },
	{ '\013' /* C-k */,	    0, MODEKEYCOPY_COPYENDOFLINE, 1 },
	{ '\016' /* C-n */,	    0, MODEKEYCOPY_DOWN, 1 },
	{ '\020' /* C-p */,	    0, MODEKEYCOPY_UP, 1 },
	{ '\022' /* C-r */,	    0, MODEKEYCOPY_SEARCHUP, 1 },
	{ '\023' /* C-s */,	    0, MODEKEYCOPY_SEARCHDOWN, 1 },
	{ '\026' /* C-v */,	    0, MODEKEYCOPY_NEXTPAGE, 1 },
	{ '\027' /* C-w */,	    0, MODEKEYCOPY_COPYSELECTION, 1 },
	{ '\033' /* Escape */,	    0, MODEKEYCOPY_CANCEL, 1 },
	{ 'b' | KEYC_ESCAPE,	    0, MODEKEYCOPY_PREVIOUSWORD, 1 },
	{ 'f',			    0, MODEKEYCOPY_JUMP, 1 },
	{ 'f' | KEYC_ESCAPE,	    0, MODEKEYCOPY_NEXTWORDEND, 1 },
	{ 'g',			    0, MODEKEYCOPY_GOTOLINE, 1 },
	{ 'm' | KEYC_ESCAPE,	    0, MODEKEYCOPY_BACKTOINDENTATION, 1 },
	{ 'n',			    0, MODEKEYCOPY_SEARCHAGAIN, 1 },
	{ 'q',			    0, MODEKEYCOPY_CANCEL, 1 },
	{ 'r' | KEYC_ESCAPE,	    0, MODEKEYCOPY_MIDDLELINE, 1 },
	{ 't',			    0, MODEKEYCOPY_JUMPTO, 1 },
	{ 'v' | KEYC_ESCAPE,	    0, MODEKEYCOPY_PREVIOUSPAGE, 1 },
	{ 'w' | KEYC_ESCAPE,	    0, MODEKEYCOPY_COPYSELECTION, 1 },
	{ '{' | KEYC_ESCAPE,	    0, MODEKEYCOPY_PREVIOUSPARAGRAPH, 1 },
	{ '}' | KEYC_ESCAPE,	    0, MODEKEYCOPY_NEXTPARAGRAPH, 1 },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCOPY_SCROLLDOWN, 1 },
	{ KEYC_DOWN | KEYC_ESCAPE,  0, MODEKEYCOPY_HALFPAGEDOWN, 1 },
	{ KEYC_DOWN,		    0, MODEKEYCOPY_DOWN, 1 },
	{ KEYC_LEFT,		    0, MODEKEYCOPY_LEFT, 1 },
	{ KEYC_NPAGE,		    0, MODEKEYCOPY_NEXTPAGE, 1 },
	{ KEYC_PPAGE,		    0, MODEKEYCOPY_PREVIOUSPAGE, 1 },
	{ KEYC_RIGHT,		    0, MODEKEYCOPY_RIGHT, 1 },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCOPY_SCROLLUP, 1 },
	{ KEYC_UP | KEYC_ESCAPE,    0, MODEKEYCOPY_HALFPAGEUP, 1 },
	{ KEYC_UP,		    0, MODEKEYCOPY_UP, 1 },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCOPY_SCROLLUP, 5 },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCOPY_SCROLLDOWN, 5 },
	{ KEYC_MOUSEDRAG1_PANE,     0, MODEKEYCOPY_STARTSELECTION, 1 },
	{ KEYC_MOUSEDRAGEND1_PANE,  0, MODEKEYCOPY_COPYSELECTION, 1 },

	{ 0,			   -1, 0, 1 }
};
struct mode_key_tree mode_key_tree_emacs_copy;

/* Table mapping key table names to default settings and trees. */
const struct mode_key_table mode_key_tables[] = {
	{ "vi-edit", mode_key_cmdstr_edit,
	  &mode_key_tree_vi_edit, mode_key_vi_edit },
	{ "vi-choice", mode_key_cmdstr_choice,
	  &mode_key_tree_vi_choice, mode_key_vi_choice },
	{ "vi-copy", mode_key_cmdstr_copy,
	  &mode_key_tree_vi_copy, mode_key_vi_copy },
	{ "emacs-edit", mode_key_cmdstr_edit,
	  &mode_key_tree_emacs_edit, mode_key_emacs_edit },
	{ "emacs-choice", mode_key_cmdstr_choice,
	  &mode_key_tree_emacs_choice, mode_key_emacs_choice },
	{ "emacs-copy", mode_key_cmdstr_copy,
	  &mode_key_tree_emacs_copy, mode_key_emacs_copy },

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
			mbind->repeat = ment->repeat;
			mbind->mode = ment->mode;
			mbind->cmd = ment->cmd;
			mbind->arg = NULL;
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
mode_key_lookup(struct mode_key_data *mdata, key_code key, const char **arg,
    u_int *repeat)
{
	struct mode_key_binding	*mbind, mtmp;

	mtmp.key = key;
	mtmp.mode = mdata->mode;
	if ((mbind = RB_FIND(mode_key_tree, mdata->tree, &mtmp)) == NULL) {
		if (mdata->mode != 0)
			return (MODEKEY_NONE);
		return (MODEKEY_OTHER);
	}
	if (repeat != NULL)
		*repeat = mbind->repeat;

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
		if (arg != NULL)
			*arg = mbind->arg;
		return (mbind->cmd);
	}
}
