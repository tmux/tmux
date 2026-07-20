/* $OpenBSD: cmd-capture-pane.c,v 1.68 2026/07/20 11:16:33 nicm Exp $ */

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
#include <vis.h>

#include "tmux.h"

/*
 * Write the entire contents of a pane to a buffer or stdout.
 */

static enum cmd_retval	cmd_capture_pane_exec(struct cmd *, struct cmdq_item *);

static char	*cmd_capture_pane_append(char *, size_t *, const char *,
		     size_t);
static char	*cmd_capture_pane_pending(struct args *, struct window_pane *,
		     size_t *);
static char	*cmd_capture_pane_history(struct args *, struct cmdq_item *,
		     struct window_pane *, size_t *);
static char	*cmd_capture_pane_hyperlinks(struct grid *, struct screen *,
		     u_int, u_int *, u_int *, size_t *);

const struct cmd_entry cmd_capture_pane_entry = {
	.name = "capture-pane",
	.alias = "capturep",

	.args = { "ab:CeE:FHJLMNpPqRS:Tt:", 0, 0, NULL },
	.usage = "[-aCeFHJLMNpPqRT] " CMD_BUFFER_USAGE " [-E end-line] "
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
cmd_capture_pane_append(char *buf, size_t *len, const char *line,
    size_t linelen)
{
	buf = xrealloc(buf, *len + linelen + 1);
	memcpy(buf + *len, line, linelen);
	*len += linelen;
	return (buf);
}

static char *
cmd_capture_pane_cell(struct screen *s, u_int xx, u_int yy)
{
	struct grid		*gd = s->grid;
	struct hyperlinks	*hl = s->hyperlinks;
	struct grid_cell	 gc;
	char			*line, *data, *link, *linkid, *f, *b, *u;
	char			 c[UTF8_SIZE + 1];
	const char		*uri, *iid;
	u_int			 flags;

	grid_get_cell(gd, xx, yy, &gc);

	memcpy(c, gc.data.data, gc.data.size);
	c[gc.data.size] = '\0';
	utf8_stravis(&data, c, VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL);

	if (gc.link != 0 && hyperlinks_get(hl, gc.link, &uri, &iid, NULL)) {
		xasprintf(&link, "%s", uri);
		if (iid != NULL && *iid != '\0')
			xasprintf(&linkid, "%s", iid);
		else
			xasprintf(&linkid, "NONE");
	} else {
		xasprintf(&link, "NONE");
		xasprintf(&linkid, "NONE");
	}

	flags = gc.flags;
	if (gc.fg & COLOUR_FLAG_256)
		flags |= GRID_FLAG_FG256;
	if (gc.bg & COLOUR_FLAG_256)
		flags |= GRID_FLAG_BG256;

	xasprintf(&f, "%s[%x]", colour_tostring(gc.fg), gc.fg);
	xasprintf(&b, "%s[%x]", colour_tostring(gc.bg), gc.bg);
	xasprintf(&u, "%s[%x]", colour_tostring(gc.us), gc.us);

	xasprintf(&line, "\t\tC %u,%u data=(%u,%u,%s) flags=%s[%x] "
	    "attr=%s[%x] fg=%s bg=%s us=%s link=%s linkid=%s\n",
	    yy, xx, gc.data.width, gc.data.size, data,
	    grid_cell_flags_string(flags), flags,
	    grid_cell_attr_string(gc.attr), gc.attr, f, b, u, link, linkid);

	free(f);
	free(b);
	free(u);
	free(link);
	free(linkid);
	free(data);
	return (line);
}

static char *
cmd_capture_pane_grid(struct window_pane *wp, size_t *len)
{
	struct screen		*s = &wp->base;
	struct grid		*gd = s->grid;
	struct grid_line	*gl;
	struct osc133_data	*od;
	char			*buf = xstrdup(""), *line;
	char			 p[11];
	u_int			 yy, xx, total = gd->hsize + gd->sy;

	xasprintf(&line, "G %ux%u (%u/%u)\n", gd->sx, gd->sy, gd->hsize,
	    gd->hlimit);
	buf = cmd_capture_pane_append(buf, len, line, strlen(line));
	free(line);

	for (yy = 0; yy < total; yy++) {
		gl = grid_get_line(gd, yy);
		if (yy < gd->hsize)
			snprintf(p, sizeof p, "-");
		else
			snprintf(p, sizeof p, "%u", yy - gd->hsize);
		od = &gl->osc133_data;
		if (gl->flags & GRID_LINE_OSC133_FLAGS) {
			xasprintf(&line, "\tL %u (%s) flags=%s[%x] "
			    "%u/%u osc133=%u,%u,%u,%u,%u\n", yy, p,
			    grid_line_flags_string(gl->flags), gl->flags,
			    gl->cellused, gl->cellsize, od->prompt_col,
			    od->cmd_col, od->out_start_col,
			    od->out_end_col, od->exit_status);
		} else {
			xasprintf(&line, "\tL %u (%s) flags=%s[%x] "
			    "%u/%u\n", yy, p,
			    grid_line_flags_string(gl->flags), gl->flags,
			    gl->cellused, gl->cellsize);
		}
		buf = cmd_capture_pane_append(buf, len, line, strlen(line));
		free(line);

		for (xx = 0; xx < gd->sx; xx++) {
			line = cmd_capture_pane_cell(s, xx, yy);
			buf = cmd_capture_pane_append(buf, len, line,
			    strlen(line));
			free(line);
		}
	}
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
cmd_capture_pane_hyperlinks(struct grid *gd, struct screen *s, u_int py,
    u_int *links, u_int *nlinks, size_t *len)
{
	const struct grid_line	*gl = grid_peek_line(gd, py);
	struct grid_cell	 gc;
	const char		*uri;
	char			*line = xstrdup("");
	u_int			 i, j;

	*len = 0;

	if (s->hyperlinks == NULL || (~gl->flags & GRID_LINE_HYPERLINK))
		return (line);

	for (i = 0; i < gl->cellused; i++) {
		grid_get_cell(gd, i, py, &gc);
		if (gc.link == 0)
			continue;
		for (j = 0; j < *nlinks; j++) {
			if (links[j] == gc.link)
				break;
		}
		if (j != *nlinks)
			continue;

		if (!hyperlinks_get(s->hyperlinks, gc.link, &uri, NULL, NULL))
			continue;

		if (*nlinks == gd->sx)
			break;
		links[(*nlinks)++] = gc.link;

		if (*len != 0)
			line = cmd_capture_pane_append(line, len, " ", 1);
		line = cmd_capture_pane_append(line, len, uri, strlen(uri));
	}
	return (line);
}

static char *
cmd_capture_pane_history(struct args *args, struct cmdq_item *item,
    struct window_pane *wp, size_t *len)
{
	struct grid			*gd;
	const struct grid_line		*gl;
	struct screen			*s;
	struct grid_cell		*gc = NULL;
	struct window_mode_entry	*wme;
	int				 n, join_lines, number_lines, flags = 0;
	int				 show_flags, hyperlinks;
	u_int				*links = NULL, nlinks = 0;
	u_int				 i, sx, top, bottom, tmp;
	char				*cause, *buf = NULL, *line, b[64], *cp;
	const char			*Sflag, *Eflag;
	size_t				 linelen;

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
		s = &wp->base;
	} else if (args_has(args, 'M')) {
		wme = TAILQ_FIRST(&wp->modes);
		if (wme != NULL && wme->mode->get_screen != NULL) {
			s = wme->mode->get_screen (wme);
			gd = s->grid;
		} else {
			s = &wp->base;
			gd = wp->base.grid;
		}
	} else {
		s = &wp->base;
		gd = wp->base.grid;
	}

	Sflag = args_get(args, 'S');
	if (Sflag != NULL && strcmp(Sflag, "-") == 0)
		top = 0;
	else {
		n = args_strtonum_and_expand(args, 'S', INT_MIN, SHRT_MAX,
			item, &cause);
		if (cause != NULL) {
			top = gd->hsize;
			free(cause);
		} else if (n < 0 && (u_int)-n > gd->hsize)
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
		} else if (n < 0 && (u_int)-n > gd->hsize)
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
	number_lines = args_has(args, 'L');
	show_flags = args_has(args, 'F');
	hyperlinks = args_has(args, 'H');
	if (hyperlinks)
		links = xreallocarray(NULL, gd->sx, sizeof *links);

	for (i = top; i <= bottom; i++) {
		if (hyperlinks) {
			line = cmd_capture_pane_hyperlinks(gd, s, i, links,
			    &nlinks, &linelen);
		} else {
			line = grid_string_cells(gd, 0, i, sx, &gc, flags, s);
			linelen = strlen(line);
		}
		if (hyperlinks && linelen == 0) {
			free(line);
			continue;
		}

		if (number_lines) {
			if (i >= gd->hsize)
				n = i - gd->hsize;
			else
				n = (int)i - (int)gd->hsize;
			n = snprintf(b, sizeof b, "%d ", n);
			if (n >= 0)
				buf = cmd_capture_pane_append(buf, len, b, n);
		}
		if (show_flags) {
			cp = b;
			*cp = '\0';

			gl = grid_peek_line(gd, i);
			if (gl->flags & GRID_LINE_DEAD)
				*cp++ = 'D';
			if (gl->flags & GRID_LINE_HYPERLINK)
				*cp++ = 'H';
			if (gl->flags & GRID_LINE_START_OUTPUT)
				*cp++ = 'O';
			if (gl->flags & GRID_LINE_START_PROMPT)
				*cp++ = 'P';
			if (gl->flags & GRID_LINE_WRAPPED)
				*cp++ = 'W';
			if (gl->flags & GRID_LINE_EXTENDED)
				*cp++ = 'X';
			if (b == cp)
				*cp++ = '-';
			*cp++ = ' ';
			*cp = '\0';
			buf = cmd_capture_pane_append(buf, len, b, strlen (b));
		}
		buf = cmd_capture_pane_append(buf, len, line, linelen);

		gl = grid_peek_line(gd, i);
		if (!join_lines || !(gl->flags & GRID_LINE_WRAPPED))
			buf[(*len)++] = '\n';

		free(line);
	}
	free(links);
	if (buf == NULL)
		buf = xstrdup("");
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
		server_redraw_window(wp->window);
		return (CMD_RETURN_NORMAL);
	}

	len = 0;
	if (args_has(args, 'R'))
		buf = cmd_capture_pane_grid(wp, &len);
	else if (args_has(args, 'P') && !args_has(args, 'H'))
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
		}
		free(buf);
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
