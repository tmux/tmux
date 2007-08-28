/* $Id: local.c,v 1.5 2007-08-28 09:19:50 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <curses.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <term.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Functions to translate input and write output to the local client terminal.
 * This file should probably be called tty or terminal.c.
 */

int	local_cmp(const void *, const void *);
int	local_putc(int);
void	local_putp(const char *);

/* Local key types and key codes. */
struct local_key {
	const char	*name;
	char		*string;
	size_t		 size;
	int	 	 code;
};
struct local_key local_keys[] = {
	{ "ka1",   NULL, 0, KEYC_A1 },
	{ "ka3",   NULL, 0, KEYC_A3 },
	{ "kb2",   NULL, 0, KEYC_B2 },
	{ "kbs",   NULL, 0, KEYC_BACKSPACE },
	{ "kbeg",  NULL, 0, KEYC_BEG },
	{ "kcbt",  NULL, 0, KEYC_BTAB },
	{ "kc1",   NULL, 0, KEYC_C1 },
	{ "kc3",   NULL, 0, KEYC_C3 },
	{ "kcan",  NULL, 0, KEYC_CANCEL },
	{ "ktbc",  NULL, 0, KEYC_CATAB },
	{ "kclr",  NULL, 0, KEYC_CLEAR },
	{ "kclo",  NULL, 0, KEYC_CLOSE },
	{ "kcmd",  NULL, 0, KEYC_COMMAND },
	{ "kcpy",  NULL, 0, KEYC_COPY },
	{ "kcrt",  NULL, 0, KEYC_CREATE },
	{ "kctab", NULL, 0, KEYC_CTAB },
	{ "kdch1", NULL, 0, KEYC_DC },
	{ "kdl1",  NULL, 0, KEYC_DL },
	{ "kcud1", NULL, 0, KEYC_DOWN },
	{ "krmir", NULL, 0, KEYC_EIC },
	{ "kend",  NULL, 0, KEYC_END },
	{ "kent",  NULL, 0, KEYC_ENTER },
	{ "kel",   NULL, 0, KEYC_EOL },
	{ "ked",   NULL, 0, KEYC_EOS },
	{ "kext",  NULL, 0, KEYC_EXIT },
	{ "kf0",   NULL, 0, KEYC_F0 },
	{ "kf1",   NULL, 0, KEYC_F1 },
	{ "kf10",  NULL, 0, KEYC_F10 },
	{ "kf11",  NULL, 0, KEYC_F11 },
	{ "kf12",  NULL, 0, KEYC_F12 },
	{ "kf13",  NULL, 0, KEYC_F13 },
	{ "kf14",  NULL, 0, KEYC_F14 },
	{ "kf15",  NULL, 0, KEYC_F15 },
	{ "kf16",  NULL, 0, KEYC_F16 },
	{ "kf17",  NULL, 0, KEYC_F17 },
	{ "kf18",  NULL, 0, KEYC_F18 },
	{ "kf19",  NULL, 0, KEYC_F19 },
	{ "kf2",   NULL, 0, KEYC_F2 },
	{ "kf20",  NULL, 0, KEYC_F20 },
	{ "kf21",  NULL, 0, KEYC_F21 },
	{ "kf22",  NULL, 0, KEYC_F22 },
	{ "kf23",  NULL, 0, KEYC_F23 },
	{ "kf24",  NULL, 0, KEYC_F24 },
	{ "kf25",  NULL, 0, KEYC_F25 },
	{ "kf26",  NULL, 0, KEYC_F26 },
	{ "kf27",  NULL, 0, KEYC_F27 },
	{ "kf28",  NULL, 0, KEYC_F28 },
	{ "kf29",  NULL, 0, KEYC_F29 },
	{ "kf3",   NULL, 0, KEYC_F3 },
	{ "kf30",  NULL, 0, KEYC_F30 },
	{ "kf31",  NULL, 0, KEYC_F31 },
	{ "kf32",  NULL, 0, KEYC_F32 },
	{ "kf33",  NULL, 0, KEYC_F33 },
	{ "kf34",  NULL, 0, KEYC_F34 },
	{ "kf35",  NULL, 0, KEYC_F35 },
	{ "kf36",  NULL, 0, KEYC_F36 },
	{ "kf37",  NULL, 0, KEYC_F37 },
	{ "kf38",  NULL, 0, KEYC_F38 },
	{ "kf39",  NULL, 0, KEYC_F39 },
	{ "kf4",   NULL, 0, KEYC_F4 },
	{ "kf40",  NULL, 0, KEYC_F40 },
	{ "kf41",  NULL, 0, KEYC_F41 },
	{ "kf42",  NULL, 0, KEYC_F42 },
	{ "kf43",  NULL, 0, KEYC_F43 },
	{ "kf44",  NULL, 0, KEYC_F44 },
	{ "kf45",  NULL, 0, KEYC_F45 },
	{ "kf46",  NULL, 0, KEYC_F46 },
	{ "kf47",  NULL, 0, KEYC_F47 },
	{ "kf48",  NULL, 0, KEYC_F48 },
	{ "kf49",  NULL, 0, KEYC_F49 },
	{ "kf5",   NULL, 0, KEYC_F5 },
	{ "kf50",  NULL, 0, KEYC_F50 },
	{ "kf51",  NULL, 0, KEYC_F51 },
	{ "kf52",  NULL, 0, KEYC_F52 },
	{ "kf53",  NULL, 0, KEYC_F53 },
	{ "kf54",  NULL, 0, KEYC_F54 },
	{ "kf55",  NULL, 0, KEYC_F55 },
	{ "kf56",  NULL, 0, KEYC_F56 },
	{ "kf57",  NULL, 0, KEYC_F57 },
	{ "kf58",  NULL, 0, KEYC_F58 },
	{ "kf59",  NULL, 0, KEYC_F59 },
	{ "kf6",   NULL, 0, KEYC_F6 },
	{ "kf60",  NULL, 0, KEYC_F60 },
	{ "kf61",  NULL, 0, KEYC_F61 },
	{ "kf62",  NULL, 0, KEYC_F62 },
	{ "kf63",  NULL, 0, KEYC_F63 },
	{ "kf7",   NULL, 0, KEYC_F7 },
	{ "kf8",   NULL, 0, KEYC_F8 },
	{ "kf9",   NULL, 0, KEYC_F9 },
	{ "kfnd",  NULL, 0, KEYC_FIND },
	{ "khlp",  NULL, 0, KEYC_HELP },
	{ "khome", NULL, 0, KEYC_HOME },
	{ "kich1", NULL, 0, KEYC_IC },
	{ "kil1",  NULL, 0, KEYC_IL },
	{ "kcub1", NULL, 0, KEYC_LEFT },
	{ "kll",   NULL, 0, KEYC_LL },
	{ "kmrk",  NULL, 0, KEYC_MARK },
	{ "kmsg",  NULL, 0, KEYC_MESSAGE },
	{ "kmov",  NULL, 0, KEYC_MOVE },
	{ "knxt",  NULL, 0, KEYC_NEXT },
	{ "knp",   NULL, 0, KEYC_NPAGE },
	{ "kopn",  NULL, 0, KEYC_OPEN },
	{ "kopt",  NULL, 0, KEYC_OPTIONS },
	{ "kpp",   NULL, 0, KEYC_PPAGE },
	{ "kprv",  NULL, 0, KEYC_PREVIOUS },
	{ "kprt",  NULL, 0, KEYC_PRINT },
	{ "krdo",  NULL, 0, KEYC_REDO },
	{ "kref",  NULL, 0, KEYC_REFERENCE },
	{ "krfr",  NULL, 0, KEYC_REFRESH },
	{ "krpl",  NULL, 0, KEYC_REPLACE },
	{ "krst",  NULL, 0, KEYC_RESTART },
	{ "kres",  NULL, 0, KEYC_RESUME },
	{ "kcuf1", NULL, 0, KEYC_RIGHT },
	{ "ksav",  NULL, 0, KEYC_SAVE },
	{ "kBEG",  NULL, 0, KEYC_SBEG },
	{ "kCAN",  NULL, 0, KEYC_SCANCEL },
	{ "kCMD",  NULL, 0, KEYC_SCOMMAND },
	{ "kCPY",  NULL, 0, KEYC_SCOPY },
	{ "kCRT",  NULL, 0, KEYC_SCREATE },
	{ "kDC",   NULL, 0, KEYC_SDC },
	{ "kDL",   NULL, 0, KEYC_SDL },
	{ "kslt",  NULL, 0, KEYC_SELECT },
	{ "kEND",  NULL, 0, KEYC_SEND },
	{ "kEOL",  NULL, 0, KEYC_SEOL },
	{ "kEXT",  NULL, 0, KEYC_SEXIT },
	{ "kind",  NULL, 0, KEYC_SF },
	{ "kFND",  NULL, 0, KEYC_SFIND },
	{ "kHLP",  NULL, 0, KEYC_SHELP },
	{ "kHOM",  NULL, 0, KEYC_SHOME },
	{ "kIC",   NULL, 0, KEYC_SIC },
	{ "kLFT",  NULL, 0, KEYC_SLEFT },
	{ "kMSG",  NULL, 0, KEYC_SMESSAGE },
	{ "kMOV",  NULL, 0, KEYC_SMOVE },
	{ "kNXT",  NULL, 0, KEYC_SNEXT },
	{ "kOPT",  NULL, 0, KEYC_SOPTIONS },
	{ "kPRV",  NULL, 0, KEYC_SPREVIOUS },
	{ "kPRT",  NULL, 0, KEYC_SPRINT },
	{ "kri",   NULL, 0, KEYC_SR },
	{ "kRDO",  NULL, 0, KEYC_SREDO },
	{ "kRPL",  NULL, 0, KEYC_SREPLACE },
	{ "kRIT",  NULL, 0, KEYC_SRIGHT },
	{ "kRES",  NULL, 0, KEYC_SRSUME },
	{ "kSAV",  NULL, 0, KEYC_SSAVE },
	{ "kSPD",  NULL, 0, KEYC_SSUSPEND },
	{ "khts",  NULL, 0, KEYC_STAB },
	{ "kUND",  NULL, 0, KEYC_SUNDO },
	{ "kspd",  NULL, 0, KEYC_SUSPEND },
	{ "kund",  NULL, 0, KEYC_UNDO },
	{ "kcuu1", NULL, 0, KEYC_UP },
	{ "pmous", NULL, 0, KEYC_MOUSE },
	{ NULL,    NULL, 0, KEYC_NONE }
};

/* tty file descriptor and local terminal buffers. */
int		 local_fd;
struct buffer	*local_in;
struct buffer	*local_out;
struct termios	 local_tio;

/* Initialise local terminal. */
int
local_init(struct buffer **in, struct buffer **out)
{
	char		 *tty;
	int		  mode;
	struct termios	  tio;
	struct local_key *lk;

	if ((tty = ttyname(STDOUT_FILENO)) == NULL)
		fatal("ttyname failed");
	if ((local_fd = open(tty, O_RDWR)) == -1)
		fatal("open failed");
	if ((mode = fcntl(local_fd, F_GETFL)) == -1)
		fatal("fcntl failed");
	if (fcntl(local_fd, F_SETFL, mode|O_NONBLOCK) == -1)
		fatal("fcntl failed");

	*in = local_in = buffer_create(BUFSIZ);
	*out = local_out = buffer_create(BUFSIZ);

	setupterm(NULL, STDOUT_FILENO, NULL);

	if (tcgetattr(local_fd, &local_tio) != 0)
		fatal("tcgetattr failed");
	memcpy(&tio, &local_tio, sizeof tio);
	tio.c_iflag &= ~(IXON|IXOFF|ICRNL|INLCR);
	tio.c_oflag &= ~(OPOST|ONLCR|OCRNL|ONLRET);
	tio.c_lflag &= ~(IEXTEN|ICANON|ECHO|ECHOE|ECHOKE|ECHOCTL|ISIG);
	if (tcsetattr(local_fd, TCSANOW, &tio) != 0)
		fatal("tcsetattr failed");

	if (enter_ca_mode != NULL)
		local_putp(enter_ca_mode);
	if (keypad_xmit != NULL)
		local_putp(keypad_xmit);
	local_putp(clear_screen);

	for (lk = local_keys; lk->name != NULL; lk++) {
		lk->string = tigetstr(lk->name);
		if (lk->string == (char *) -1 || lk->string == (char *) 0)
			lk->string = NULL;
		else {
			lk->size = strlen(lk->string);
			log_debug("string for %s (%d): \"%s\", length %zu",
			    lk->name, lk->code, lk->string, lk->size);
		}
	}
	qsort(local_keys, sizeof local_keys /
	    sizeof local_keys[0], sizeof local_keys[0], local_cmp);

	return (local_fd);
}

/* Compare keys. */
int
local_cmp(const void *ptr1, const void *ptr2)
{
	const struct local_key *lk1 = ptr1, *lk2 = ptr2;

	if (lk1->string == NULL && lk2->string == NULL)
		return (0);
	if (lk1->string == NULL)
		return (1);
	if (lk2->string == NULL)
		return (-1);
	return (lk2->size - lk1->size);
}

/* Tidy up and reset local terminal. */
void
local_done(void)
{
	xfree(local_in);
	xfree(local_out);

	if (tcsetattr(local_fd, TCSANOW, &local_tio) != 0)
		fatal("tcsetattr failed");
	close(local_fd);

	if (keypad_local != NULL)
		putp(keypad_local);	/* not local_putp */
	if (exit_ca_mode != NULL)
		putp(exit_ca_mode);
	putp(clear_screen);
	if (cursor_normal != NULL)
		putp(cursor_normal);
	if (exit_attribute_mode != NULL)
		putp(exit_attribute_mode);
}

/* Put a character. Used as parameter to tputs. */
int
local_putc(int c)
{
	FILE	*f;
	u_char	ch = c;

	if (c < 0 || c > (int) UCHAR_MAX)
		fatalx("invalid character");

	if (debug_level > 1) {
		f = fopen("tmux-out.log", "a+");
		fprintf(f, "%c", ch);
		fclose(f);
	}

	buffer_write(local_out, &ch, 1);
	return (c);
}

/* Put terminfo string. */
void
local_putp(const char *s)
{
	if (s == NULL)
		fatalx("null pointer");

	tputs(s, 1, local_putc);
}

/* Return waiting keys if any. */
int
local_key(size_t *used)
{
	struct local_key	*lk;
	u_int			 i;
	size_t			 size;

	size = BUFFER_USED(local_in);
	if (size == 0)
		return (KEYC_NONE);

	i = 0;
	lk = local_keys;
	while (lk->string != NULL) {
		if (strncmp(BUFFER_OUT(local_in), lk->string, size) == 0) {
			if (size < lk->size)
				return (KEYC_NONE);
			log_debug("got key: %s %d", lk->name, lk->code);
			buffer_remove(local_in, lk->size);
			if (used != NULL)
				*used = lk->size;
			return (lk->code);
		}

		i++;
		lk = local_keys + i;
	}

	if (used != NULL)
		*used = 1;
	return (input_extract8(local_in));
}

/* Display output data. */
void
local_output(struct buffer *b, size_t size)
{
	u_char		 ch;
	uint16_t	 ua, ub;

	while (size != 0) {
		if (size < 1)
			break;
		size--;
		ch = input_extract8(b);
		if (ch != '\e') {
			switch (ch) {
			case '\n':	/* LF */
				if (cursor_down != NULL) {
					local_putp(cursor_down);
					break;
				}
				log_warnx("cursor_down not supported");
				break;
			case '\r':	/* CR */
				if (carriage_return != NULL) {
					local_putp(carriage_return);
					break;
				}
				log_warnx("carriage_return not supported");
				break;
			case '\010':	/* BS */
				if (cursor_left != NULL) {
					local_putp(cursor_left);
					break;
				}
				log_warnx("cursor_left not supported");
				break;
			default:
				local_putc(ch);
				break;
			}
			continue;
		}

		if (size < 1)
			fatalx("underflow");
		size--;
		ch = input_extract8(b);

		log_debug("received code %hhu", ch);
		switch (ch) {
		case CODE_CURSORUP:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_up_cursor == NULL) {
				log_warnx("parm_up_cursor not supported");
				break;
			}
			local_putp(tparm(parm_up_cursor, ua));
			break;
		case CODE_CURSORDOWN:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_down_cursor == NULL) {
				log_warnx("parm_down_cursor not supported");
				break;
			}
			local_putp(tparm(parm_down_cursor, ua));
			break;
		case CODE_CURSORRIGHT:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_right_cursor == NULL) {
				log_warnx("parm_right_cursor not supported");
				break;
			}
			local_putp(tparm(parm_right_cursor, ua));
			break;
		case CODE_CURSORLEFT:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_left_cursor == NULL) {
				log_warnx("parm_left_cursor not supported");
				break;
			}
			local_putp(tparm(parm_left_cursor, ua));
			break;
		case CODE_CURSORMOVE:
			if (size < 4)
				fatalx("underflow");
			size -= 4;
			ua = input_extract16(b);
			ub = input_extract16(b);
			if (cursor_address == NULL) {
				log_warnx("cursor_address not supported");
				break;
			}
			local_putp(tparm(cursor_address, ua - 1, ub - 1));
			break;
		case CODE_CLEARENDOFSCREEN:
			if (clr_eos == NULL) {
				log_warnx("clr_eos not supported");
				break;
			}
			local_putp(clr_eos);
			break;
		case CODE_CLEARSCREEN:
			if (clear_screen == NULL) {
				log_warnx("clear_screen not supported");
				break;
			}
			local_putp(clear_screen);
			break;
		case CODE_CLEARENDOFLINE:
			if (clr_eol == NULL) {
				log_warnx("clr_eol not supported");
				break;
			}
			local_putp(clr_eol);
			break;
		case CODE_CLEARSTARTOFLINE:
			if (clr_bol == NULL) {
				log_warnx("clr_bol not supported");
				break;
			}
			local_putp(clr_bol);
			break;
		case CODE_CLEARLINE:
			if (clr_eol == NULL) {
				log_warnx("clr_eol not suppported");
				break;
			}
			local_putp(clr_eol);	/* XXX */
			break;
		case CODE_INSERTLINE:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_insert_line == NULL) {
				log_warnx("parm_insert_line not supported");
				break;
			}
			local_putp(tparm(parm_insert_line, ua));
			break;
		case CODE_DELETELINE:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_delete_line == NULL) {
				log_warnx("parm_delete not supported");
				break;
			}
			local_putp(tparm(parm_delete_line, ua));
			break;
		case CODE_INSERTCHARACTER:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_ich == NULL) {
				log_warnx("parm_ich not supported");
				break;
			}
			local_putp(tparm(parm_ich, ua));
			break;
		case CODE_DELETECHARACTER:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);
			if (parm_dch == NULL) {
				log_warnx("parm_dch not supported");
				break;
			}
			local_putp(tparm(parm_dch, ua));
			break;
		case CODE_CURSORON:
			if (cursor_normal == NULL) {
				log_warnx("cursor_normal not supported");
				break;
			}
 			local_putp(cursor_normal);
			break;
		case CODE_CURSOROFF:
			if (cursor_invisible == NULL) {
				log_warnx("cursor_invisible not supported");
				break;
			}
			local_putp(cursor_invisible);
			break;
		case CODE_REVERSEINDEX:
			if (scroll_reverse == NULL) {
				log_warnx("scroll_reverse not supported");
				break;
			}
			local_putp(scroll_reverse);
			break;
		case CODE_SCROLLREGION:
			if (size < 4)
				fatalx("underflow");
			size -= 4;
			ua = input_extract16(b);
			ub = input_extract16(b);
			if (change_scroll_region == NULL) {
				log_warnx("change_scroll_region not supported");
				break;
			}
			local_putp(tparm(change_scroll_region, ua - 1, ub - 1));
			break;
		case CODE_INSERTON:
			if (enter_insert_mode == NULL) {
				log_warnx("enter_insert_mode not supported");
				break;
			}
			local_putp(enter_insert_mode);
			break;
		case CODE_INSERTOFF:
			if (exit_insert_mode == NULL) {
				log_warnx("exit_insert_mode not supported");
				break;
			}
			local_putp(exit_insert_mode);
			break;
		case CODE_KCURSOROFF:
			/*
			t = tigetstr("CE");
			if (t != (char *) 0 && t != (char *) -1)
				local_putp(t);
			*/
			break;
		case CODE_KCURSORON:
			/*
			t = tigetstr("CS");
			if (t != (char *) 0 && t != (char *) -1)
				local_putp(t);
			*/
			break;
		case CODE_KKEYPADOFF:
			/*
			if (keypad_local == NULL) {
				log_warnx("keypad_local not supported");
				break;
			}
			local_putp(keypad_local);
			*/
			break;
		case CODE_KKEYPADON:
			/*
			if (keypad_xmit == NULL) {
				log_warnx("keypad_xmit not supported");
				break;
			}
			local_putp(keypad_xmit);
			*/
			break;
		case CODE_TITLE:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);

			if (size < ua)
				fatalx("underflow");
			size -= ua;
			buffer_remove(b, ua);
			break;
		case CODE_ATTRIBUTES:
			if (size < 2)
				fatalx("underflow");
			size -= 2;
			ua = input_extract16(b);

			if (exit_attribute_mode == NULL) {
				log_warnx("exit_attribute_mode not supported");
				break;
			}
			if (ua == 0) {
				local_putp(exit_attribute_mode);
				break;
			}

			while (ua-- != 0) {
				if (size < 2)
					fatalx("underflow");
				size -= 2;
				ub = input_extract16(b);

				switch (ub) {
				case 0:
				case 10:
					if (exit_attribute_mode)
						local_putp(exit_attribute_mode);
					break;
				case 1:
					if (enter_bold_mode)
						local_putp(enter_bold_mode);
					break;
				case 2:
					if (enter_dim_mode)
						local_putp(enter_dim_mode);
					break;
				case 3:
					if (enter_standout_mode)
						local_putp(enter_standout_mode);
					break;
				case 4:
					if (enter_underline_mode)
						local_putp(enter_underline_mode);
					break;
				case 5:
					if (enter_blink_mode)
						local_putp(enter_blink_mode);
					break;
				case 7:
					if (enter_reverse_mode)
						local_putp(enter_reverse_mode);
					break;
				case 8:
					if (enter_secure_mode)
						local_putp(enter_secure_mode);
					break;
				case 23:
					if (exit_standout_mode)
						local_putp(exit_standout_mode);
					break;
				case 24:
					if (exit_underline_mode)
						local_putp(exit_underline_mode);
					break;
				case 30:
				case 31:
				case 32:
				case 33:
				case 34:
				case 35:
				case 36:
				case 37:
					if (set_a_foreground == NULL)
						break;
					local_putp(
					    tparm(set_a_foreground, ub - 30));
					break;
				case 39:
					if (set_a_foreground == NULL)
						break;
					if (tigetflag("AX") == TRUE) {
						local_putp("\e[39m");
						break;
					}
					local_putp(tparm(set_a_foreground, 7));
					break;
				case 40:
				case 41:
				case 42:
				case 43:
				case 44:
				case 45:
				case 46:
				case 47:
					if (set_a_background == NULL)
						break;
					local_putp(
					    tparm(set_a_background, ub - 40));
					break;
				case 49:
					if (set_a_background == NULL)
						break;
					if (tigetflag("AX") == TRUE) {
						local_putp("\e[49m");
						break;
					}
					local_putp(tparm(set_a_background, 0));
					break;
				}
			}
			break;
		}
	}
}
