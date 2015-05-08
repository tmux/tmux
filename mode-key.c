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
	int			key;

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
	{ MODEKEYCOPY_NEXTSPACE, "next-space" },
	{ MODEKEYCOPY_NEXTSPACEEND, "next-space-end" },
	{ MODEKEYCOPY_NEXTWORD, "next-word" },
	{ MODEKEYCOPY_NEXTWORDEND, "next-word-end" },
	{ MODEKEYCOPY_OTHEREND, "other-end" },
	{ MODEKEYCOPY_PREVIOUSPAGE, "page-up" },
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
const struct mode_key_entry mode_key_vi_choice[] = {
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

/* vi copy mode keys. */
const struct mode_key_entry mode_key_vi_copy[] = {
	{ ' ',			    0, MODEKEYCOPY_STARTSELECTION },
	{ '"',			    0, MODEKEYCOPY_STARTNAMEDBUFFER },
	{ '$',			    0, MODEKEYCOPY_ENDOFLINE },
	{ ',',			    0, MODEKEYCOPY_JUMPREVERSE },
	{ ';',			    0, MODEKEYCOPY_JUMPAGAIN },
	{ '/',			    0, MODEKEYCOPY_SEARCHDOWN },
	{ '0',			    0, MODEKEYCOPY_STARTOFLINE },
	{ '1',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '2',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '3',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '4',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '5',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '6',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '7',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '8',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '9',			    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ ':',			    0, MODEKEYCOPY_GOTOLINE },
	{ '?',			    0, MODEKEYCOPY_SEARCHUP },
	{ 'A',			    0, MODEKEYCOPY_APPENDSELECTION },
	{ 'B',			    0, MODEKEYCOPY_PREVIOUSSPACE },
	{ 'D',			    0, MODEKEYCOPY_COPYENDOFLINE },
	{ 'E',			    0, MODEKEYCOPY_NEXTSPACEEND },
	{ 'F',			    0, MODEKEYCOPY_JUMPBACK },
	{ 'G',			    0, MODEKEYCOPY_HISTORYBOTTOM },
	{ 'H',			    0, MODEKEYCOPY_TOPLINE },
	{ 'J',			    0, MODEKEYCOPY_SCROLLDOWN },
	{ 'K',			    0, MODEKEYCOPY_SCROLLUP },
	{ 'L',			    0, MODEKEYCOPY_BOTTOMLINE },
	{ 'M',			    0, MODEKEYCOPY_MIDDLELINE },
	{ 'N',			    0, MODEKEYCOPY_SEARCHREVERSE },
	{ 'T',			    0, MODEKEYCOPY_JUMPTOBACK },
	{ 'V',			    0, MODEKEYCOPY_SELECTLINE },
	{ 'W',			    0, MODEKEYCOPY_NEXTSPACE },
	{ '\002' /* C-b */,	    0, MODEKEYCOPY_PREVIOUSPAGE },
	{ '\003' /* C-c */,	    0, MODEKEYCOPY_CANCEL },
	{ '\004' /* C-d */,	    0, MODEKEYCOPY_HALFPAGEDOWN },
	{ '\005' /* C-e */,	    0, MODEKEYCOPY_SCROLLDOWN },
	{ '\006' /* C-f */,	    0, MODEKEYCOPY_NEXTPAGE },
	{ '\010' /* C-h */,	    0, MODEKEYCOPY_LEFT },
	{ '\025' /* C-u */,	    0, MODEKEYCOPY_HALFPAGEUP },
	{ '\031' /* C-y */,	    0, MODEKEYCOPY_SCROLLUP },
	{ '\033' /* Escape */,	    0, MODEKEYCOPY_CLEARSELECTION },
	{ '\n',			    0, MODEKEYCOPY_COPYSELECTION },
	{ '\r',			    0, MODEKEYCOPY_COPYSELECTION },
	{ '^',			    0, MODEKEYCOPY_BACKTOINDENTATION },
	{ 'b',			    0, MODEKEYCOPY_PREVIOUSWORD },
	{ 'e',			    0, MODEKEYCOPY_NEXTWORDEND },
	{ 'f',			    0, MODEKEYCOPY_JUMP },
	{ 'g',			    0, MODEKEYCOPY_HISTORYTOP },
	{ 'h',			    0, MODEKEYCOPY_LEFT },
	{ 'j',			    0, MODEKEYCOPY_DOWN },
	{ 'k',			    0, MODEKEYCOPY_UP },
	{ 'l',			    0, MODEKEYCOPY_RIGHT },
	{ 'n',			    0, MODEKEYCOPY_SEARCHAGAIN },
	{ 'o',			    0, MODEKEYCOPY_OTHEREND },
	{ 't',			    0, MODEKEYCOPY_JUMPTO },
	{ 'q',			    0, MODEKEYCOPY_CANCEL },
	{ 'v',			    0, MODEKEYCOPY_RECTANGLETOGGLE },
	{ 'w',			    0, MODEKEYCOPY_NEXTWORD },
	{ KEYC_BSPACE,		    0, MODEKEYCOPY_LEFT },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCOPY_SCROLLDOWN },
	{ KEYC_DOWN,		    0, MODEKEYCOPY_DOWN },
	{ KEYC_LEFT,		    0, MODEKEYCOPY_LEFT },
	{ KEYC_NPAGE,		    0, MODEKEYCOPY_NEXTPAGE },
	{ KEYC_PPAGE,		    0, MODEKEYCOPY_PREVIOUSPAGE },
	{ KEYC_RIGHT,		    0, MODEKEYCOPY_RIGHT },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCOPY_SCROLLUP },
	{ KEYC_UP,		    0, MODEKEYCOPY_UP },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCOPY_SCROLLUP },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCOPY_SCROLLDOWN },
	{ KEYC_MOUSEDRAG1_PANE,     0, MODEKEYCOPY_STARTSELECTION },

	{ 0,			   -1, 0 }
};
struct mode_key_tree mode_key_tree_vi_copy;

/* emacs editing keys. */
const struct mode_key_entry mode_key_emacs_edit[] = {
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
const struct mode_key_entry mode_key_emacs_choice[] = {
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

/* emacs copy mode keys. */
const struct mode_key_entry mode_key_emacs_copy[] = {
	{ ' ',			    0, MODEKEYCOPY_NEXTPAGE },
	{ ',',			    0, MODEKEYCOPY_JUMPREVERSE },
	{ ';',			    0, MODEKEYCOPY_JUMPAGAIN },
	{ '1' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '2' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '3' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '4' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '5' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '6' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '7' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '8' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '9' | KEYC_ESCAPE,	    0, MODEKEYCOPY_STARTNUMBERPREFIX },
	{ '<' | KEYC_ESCAPE,	    0, MODEKEYCOPY_HISTORYTOP },
	{ '>' | KEYC_ESCAPE,	    0, MODEKEYCOPY_HISTORYBOTTOM },
	{ 'F',			    0, MODEKEYCOPY_JUMPBACK },
	{ 'N',			    0, MODEKEYCOPY_SEARCHREVERSE },
	{ 'R' | KEYC_ESCAPE,	    0, MODEKEYCOPY_TOPLINE },
	{ 'R',			    0, MODEKEYCOPY_RECTANGLETOGGLE },
	{ 'T',			    0, MODEKEYCOPY_JUMPTOBACK },
	{ '\000' /* C-Space */,	    0, MODEKEYCOPY_STARTSELECTION },
	{ '\001' /* C-a */,	    0, MODEKEYCOPY_STARTOFLINE },
	{ '\002' /* C-b */,	    0, MODEKEYCOPY_LEFT },
	{ '\003' /* C-c */,	    0, MODEKEYCOPY_CANCEL },
	{ '\005' /* C-e */,	    0, MODEKEYCOPY_ENDOFLINE },
	{ '\006' /* C-f */,	    0, MODEKEYCOPY_RIGHT },
	{ '\007' /* C-g */,	    0, MODEKEYCOPY_CLEARSELECTION },
	{ '\013' /* C-k */,	    0, MODEKEYCOPY_COPYENDOFLINE },
	{ '\016' /* C-n */,	    0, MODEKEYCOPY_DOWN },
	{ '\020' /* C-p */,	    0, MODEKEYCOPY_UP },
	{ '\022' /* C-r */,	    0, MODEKEYCOPY_SEARCHUP },
	{ '\023' /* C-s */,	    0, MODEKEYCOPY_SEARCHDOWN },
	{ '\026' /* C-v */,	    0, MODEKEYCOPY_NEXTPAGE },
	{ '\027' /* C-w */,	    0, MODEKEYCOPY_COPYSELECTION },
	{ '\033' /* Escape */,	    0, MODEKEYCOPY_CANCEL },
	{ 'b' | KEYC_ESCAPE,	    0, MODEKEYCOPY_PREVIOUSWORD },
	{ 'f',			    0, MODEKEYCOPY_JUMP },
	{ 'f' | KEYC_ESCAPE,	    0, MODEKEYCOPY_NEXTWORDEND },
	{ 'g',			    0, MODEKEYCOPY_GOTOLINE },
	{ 'm' | KEYC_ESCAPE,	    0, MODEKEYCOPY_BACKTOINDENTATION },
	{ 'n',			    0, MODEKEYCOPY_SEARCHAGAIN },
	{ 'q',			    0, MODEKEYCOPY_CANCEL },
	{ 'r' | KEYC_ESCAPE,	    0, MODEKEYCOPY_MIDDLELINE },
	{ 't',			    0, MODEKEYCOPY_JUMPTO },
	{ 'v' | KEYC_ESCAPE,	    0, MODEKEYCOPY_PREVIOUSPAGE },
	{ 'w' | KEYC_ESCAPE,	    0, MODEKEYCOPY_COPYSELECTION },
	{ KEYC_DOWN | KEYC_CTRL,    0, MODEKEYCOPY_SCROLLDOWN },
	{ KEYC_DOWN | KEYC_ESCAPE,  0, MODEKEYCOPY_HALFPAGEDOWN },
	{ KEYC_DOWN,		    0, MODEKEYCOPY_DOWN },
	{ KEYC_LEFT,		    0, MODEKEYCOPY_LEFT },
	{ KEYC_NPAGE,		    0, MODEKEYCOPY_NEXTPAGE },
	{ KEYC_PPAGE,		    0, MODEKEYCOPY_PREVIOUSPAGE },
	{ KEYC_RIGHT,		    0, MODEKEYCOPY_RIGHT },
	{ KEYC_UP | KEYC_CTRL,	    0, MODEKEYCOPY_SCROLLUP },
	{ KEYC_UP | KEYC_ESCAPE,    0, MODEKEYCOPY_HALFPAGEUP },
	{ KEYC_UP,		    0, MODEKEYCOPY_UP },
	{ KEYC_WHEELUP_PANE,        0, MODEKEYCOPY_SCROLLUP },
	{ KEYC_WHEELDOWN_PANE,      0, MODEKEYCOPY_SCROLLDOWN },
	{ KEYC_MOUSEDRAG1_PANE,     0, MODEKEYCOPY_STARTSELECTION },

	{ 0,			   -1, 0 }
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
	if (mbind1->mode != mbind2->mode)
		return (mbind1->mode - mbind2->mode);
	return (mbind1->key - mbind2->key);
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
mode_key_lookup(struct mode_key_data *mdata, int key, const char **arg)
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
		if (arg != NULL)
			*arg = mbind->arg;
		return (mbind->cmd);
	}
}
