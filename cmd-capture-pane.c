/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Jonathan Alvarado <radobobo@users.sourceforge.net>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Write the entire contents of a pane to a buffer or stdout.
 */

static enum cmd_retval	cmd_capture_pane_exec(struct cmd *, struct cmdq_item *);

static char	*cmd_capture_pane_append(char *, size_t *, char *, size_t);
static char	*cmd_capture_pane_pending(struct args *, struct window_pane *,
		     size_t *);
static char	*cmd_capture_pane_history(struct args *, struct cmdq_item *,
		     struct window_pane *, size_t *);

const struct cmd_entry cmd_capture_pane_entry = {
	.name = "capture-pane",
	.alias = "capturep",

	.args = { "ab:CeE:JNpPqS:Tt:", 0, 0, NULL },
	.usage = "[-aCeJNpPqT] " CMD_BUFFER_USAGE " [-E end-line] "
		 "[-S start-line] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_capture_pane_exec
};

const struct cmd_entry cmd_clear_history_entry = {
	.name = "clear-history",
	.alias = "clearhist",

	.args = { "Ht:", 0, 0, NULL },
	.usage = "[-H] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_capture_pane_exec
};

static char *
cmd_capture_pane_append(char *buf, size_t *len, char *line, size_t linelen)
{
	buf = xrealloc(buf, *len + linelen + 1);
	memcpy(buf + *len, line, linelen);
	*len += linelen;
	return (buf);
}

static char *
cmd_capture_pane_pending(struct args *args, struct window_pane *wp,
    size_t *len)
{
	struct evbuffer	*pending;
	char		*buf, *line, tmp[5];
	size_t		 linelen;
	u_int		 i;

	pending = input_pending(wp->ictx);
	if (pending == NULL)
		return (xstrdup(""));

	line = EVBUFFER_DATA(pending);
	linelen = EVBUFFER_LENGTH(pending);

	buf = xstrdup("");
	if (args_has(args, 'C')) {
		for (i = 0; i < linelen; i++) {
			if (line[i] >= ' ' && line[i] != '\\') {
				tmp[0] = line[i];
				tmp[1] = '\0';
			} else
				xsnprintf(tmp, sizeof tmp, "\\%03hho", line[i]);
			buf = cmd_capture_pane_append(buf, len, tmp,
			    strlen(tmp));
		}
	} else
		buf = cmd_capture_pane_append(buf, len, line, linelen);
	return (buf);
}

static char *
cmd_capture_pane_history(struct args *args, struct cmdq_item *item,
    struct window_pane *wp, size_t *len)
{
	struct grid		*gd;
	const struct grid_line	*gl;
	struct grid_cell	*gc = NULL;
	int			 n, join_lines, flags = 0;
	u_int			 i, sx, top, bottom, tmp;
	char			*cause, *buf, *line;
	const char		*Sflag, *Eflag;
	size_t			 linelen;

	sx = screen_size_x(&wp->base);
	if (args_has(args, 'a')) {
		gd = wp->base.saved_grid;
		if (gd == NULL) {
			if (!args_has(args, 'q')) {
				cmdq_error(item, "no alternate screen");
				return (NULL);
			}
			return (xstrdup(""));
		}
	} else
		gd = wp->base.grid;

	Sflag = args_get(args, 'S');
	if (Sflag != NULL && strcmp(Sflag, "-") == 0)
		top = 0;
	else {
		n = args_strtonum_and_expand(args, 'S', INT_MIN, SHRT_MAX,
			item, &cause);
		if (cause != NULL) {
			top = gd->hsize;
			free(cause);
		} else if (n < 0 && (u_int) -n > gd->hsize)
			top = 0;
		else
			top = gd->hsize + n;
		if (top > gd->hsize + gd->sy - 1)
			top = gd->hsize + gd->sy - 1;
	}

	Eflag = args_get(args, 'E');
	if (Eflag != NULL && strcmp(Eflag, "-") == 0)
		bottom = gd->hsize + gd->sy - 1;
	else {
		n = args_strtonum_and_expand(args, 'E', INT_MIN, SHRT_MAX,
			item, &cause);
		if (cause != NULL) {
			bottom = gd->hsize + gd->sy - 1;
			free(cause);
		} else if (n < 0 && (u_int) -n > gd->hsize)
			bottom = 0;
		else
			bottom = gd->hsize + n;
		if (bottom > gd->hsize + gd->sy - 1)
			bottom = gd->hsize + gd->sy - 1;
	}

	if (bottom < top) {
		tmp = bottom;
		bottom = top;
		top = tmp;
	}

	join_lines = args_has(args, 'J');
	if (args_has(args, 'e'))
		flags |= GRID_STRING_WITH_SEQUENCES;
	if (args_has(args, 'C'))
		flags |= GRID_STRING_ESCAPE_SEQUENCES;
	if (!join_lines && !args_has(args, 'T'))
		flags |= GRID_STRING_EMPTY_CELLS;
	if (!join_lines && !args_has(args, 'N'))
		flags |= GRID_STRING_TRIM_SPACES;

	buf = NULL;
	for (i = top; i <= bottom; i++) {
		line = grid_string_cells(gd, 0, i, sx, &gc, flags, wp->screen);
		linelen = strlen(line);

		buf = cmd_capture_pane_append(buf, len, line, linelen);

		gl = grid_peek_line(gd, i);
		if (!join_lines || !(gl->flags & GRID_LINE_WRAPPED))
			buf[(*len)++] = '\n';

		free(line);
	}
	return (buf);
}

static enum cmd_retval
cmd_capture_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*c = cmdq_get_client(item);
	struct window_pane	*wp = cmdq_get_target(item)->wp;
	char			*buf, *cause;
	const char		*bufname;
	size_t			 len;

	if (cmd_get_entry(self) == &cmd_clear_history_entry) {
		window_pane_reset_mode_all(wp);
		grid_clear_history(wp->base.grid);
		if (args_has(args, 'H'))
			screen_reset_hyperlinks(wp->screen);
		return (CMD_RETURN_NORMAL);
	}

	len = 0;
	if (args_has(args, 'P'))
		buf = cmd_capture_pane_pending(args, wp, &len);
	else
		buf = cmd_capture_pane_history(args, item, wp, &len);
	if (buf == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'p')) {
		if (len > 0 && buf[len - 1] == '\n')
			len--;
		if (c->flags & CLIENT_CONTROL)
			control_write(c, "%.*s", (int)len, buf);
		else {
			if (!file_can_print(c)) {
				cmdq_error(item, "can't write to client");
				free(buf);
				return (CMD_RETURN_ERROR);
			}
			file_print_buffer(c, buf, len);
			file_print(c, "\n");
			free(buf);
		}
	} else {
		bufname = NULL;
		if (args_has(args, 'b'))
			bufname = args_get(args, 'b');

		if (paste_set(buf, len, bufname, &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			free(buf);
			return (CMD_RETURN_ERROR);
		}
	}

	return (CMD_RETURN_NORMAL);
}
