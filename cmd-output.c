/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael R
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

struct cmd_output_row {
	char			**cells;
	enum cmd_output_style	 *styles;
};

struct cmd_output_table {
	struct cmdq_item		*item;
	char			*title;
	u_int			 columns;
	char			**headers;
	struct cmd_output_row	*rows;
	u_int			 nrows;
	u_int			 rowspace;
};

static char	*cmd_output_table_chunk(const char *, u_int, size_t *);

static const char *
cmd_output_colour(struct cmdq_item *item, enum cmd_output_style style)
{
	if (!cmd_output_has_colour(item))
		return ("");

	switch (style) {
	case CMD_OUTPUT_HEADING:
		return ("\033[1m");
	case CMD_OUTPUT_IDENTIFIER:
		return ("\033[36m");
	case CMD_OUTPUT_SUCCESS:
		return ("\033[32m");
	case CMD_OUTPUT_WARNING:
		return ("\033[33m");
	case CMD_OUTPUT_ERROR:
		return ("\033[31m");
	case CMD_OUTPUT_DIM:
		return ("\033[2m");
	case CMD_OUTPUT_DEFAULT:
		return ("");
	}
	return ("");
}

static const char *
cmd_output_reset(struct cmdq_item *item, enum cmd_output_style style)
{
	if (style == CMD_OUTPUT_DEFAULT || !cmd_output_has_colour(item))
		return ("");
	return ("\033[0m");
}

int
cmd_output_is_human(struct cmdq_item *item)
{
	struct client	*c = cmdq_get_client(item);

	/*
	 * Readable output is only for interactive clients. Control mode and
	 * redirected output must retain their existing machine-readable form.
	 */
	if (c == NULL || (c->flags & CLIENT_CONTROL))
		return (0);
	if (c->session != NULL)
		return (1);
	return (c->stdout_tty);
}

int
cmd_output_has_colour(struct cmdq_item *item)
{
	struct client		*c = cmdq_get_client(item);
	struct environ_entry	*envent;

	if (!cmd_output_is_human(item) || c->session != NULL ||
	    !c->stdout_tty)
		return (0);
	if (environ_find(c->environ, "NO_COLOR") != NULL)
		return (0);
	envent = environ_find(c->environ, "TERM");
	if (envent != NULL && envent->value != NULL &&
	    strcmp(envent->value, "dumb") == 0)
		return (0);
	return (1);
}

u_int
cmd_output_width(struct cmdq_item *item)
{
	struct client	*c = cmdq_get_client(item);

	if (c == NULL)
		return (80);
	if (c->session != NULL && c->tty.sx != 0)
		return (c->tty.sx);
	if (c->stdout_width != 0)
		return (c->stdout_width);
	return (80);
}

void
cmd_output_print(struct cmdq_item *item, enum cmd_output_style style,
    const char *fmt, ...)
{
	va_list	 ap;
	char	*text;

	va_start(ap, fmt);
	xvasprintf(&text, fmt, ap);
	va_end(ap);

	cmdq_print(item, "%s%s%s", cmd_output_colour(item, style), text,
	    cmd_output_reset(item, style));
	free(text);
}

struct cmd_output_table *
cmd_output_table_create(struct cmdq_item *item, const char *title,
    u_int columns, const char **headers)
{
	struct cmd_output_table	*table;
	u_int			 i;

	table = xcalloc(1, sizeof *table);
	table->item = item;
	table->title = xstrdup(title == NULL ? "" : title);
	table->columns = columns;
	table->headers = xcalloc(columns, sizeof *table->headers);
	for (i = 0; i < columns; i++)
		table->headers[i] = xstrdup(headers[i] == NULL ? "" : headers[i]);
	return (table);
}

void
cmd_output_table_add(struct cmd_output_table *table, const char **cells,
    const enum cmd_output_style *styles)
{
	struct cmd_output_row	*row;
	u_int			 i;

	if (table->nrows == table->rowspace) {
		table->rowspace += 16;
		table->rows = xreallocarray(table->rows, table->rowspace,
		    sizeof *table->rows);
	}
	row = &table->rows[table->nrows++];
	row->cells = xcalloc(table->columns, sizeof *row->cells);
	row->styles = xcalloc(table->columns, sizeof *row->styles);
	for (i = 0; i < table->columns; i++) {
		row->cells[i] = xstrdup(cells[i] == NULL ? "" : cells[i]);
		if (styles != NULL)
			row->styles[i] = styles[i];
		else
			row->styles[i] = CMD_OUTPUT_DEFAULT;
	}
}

void
cmd_output_table_add_formats(struct cmd_output_table *table,
    struct format_tree *ft, const char **formats,
    const enum cmd_output_style *styles)
{
	char	**cells;
	u_int	  i;

	cells = xcalloc(table->columns, sizeof *cells);
	for (i = 0; i < table->columns; i++)
		cells[i] = format_expand(ft, formats[i]);
	cmd_output_table_add(table, (const char **)cells, styles);
	for (i = 0; i < table->columns; i++)
		free(cells[i]);
	free(cells);
}

static char *
cmd_output_table_prefix(struct cmd_output_table *table,
    struct cmd_output_row *row, u_int *widths)
{
	struct evbuffer	*evb;
	const char	*start, *reset;
	char		*padded, *line, *trimmed, *value;
	size_t		 used;
	u_int		 i;

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");
	for (i = 0; i + 1 < table->columns; i++) {
		start = cmd_output_colour(table->item, row->styles[i]);
		reset = cmd_output_reset(table->item, row->styles[i]);
		if (utf8_cstrwidth(row->cells[i]) > widths[i]) {
			trimmed = cmd_output_table_chunk(row->cells[i],
			    widths[i] - 1, &used);
			xasprintf(&value, "%s>", trimmed);
			free(trimmed);
		} else
			value = xstrdup(row->cells[i]);
		padded = utf8_padcstr(value, widths[i]);
		evbuffer_add_printf(evb, "%s%s%s  ", start, padded, reset);
		free(value);
		free(padded);
	}
	evbuffer_add(evb, "", 1);
	line = xstrdup(EVBUFFER_DATA(evb));
	evbuffer_free(evb);
	return (line);
}

static char *
cmd_output_table_chunk(const char *text, u_int limit, size_t *used)
{
	const char	*cp = text, *end = text, *space = NULL;
	struct utf8_data ud;
	enum utf8_state	 more;
	size_t		 size;
	u_int		 width = 0, character_width;

	while (*cp != '\0') {
		size = 1;
		if ((more = utf8_open(&ud, *cp)) == UTF8_MORE) {
			while (cp[size] != '\0' && more == UTF8_MORE) {
				more = utf8_append(&ud, cp[size]);
				size++;
			}
			if (more == UTF8_DONE)
				character_width = ud.width;
			else {
				size = 1;
				character_width = 1;
			}
		} else if ((u_char)*cp > 0x1f && (u_char)*cp != 0x7f)
			character_width = 1;
		else
			character_width = 0;

		if (width + character_width > limit)
			break;
		if (*cp == ' ')
			space = cp;
		width += character_width;
		cp += size;
		end = cp;
	}

	if (*cp != '\0' && space != NULL && space != text)
		end = space;
	if (end == text && *text != '\0') {
		end = text + 1;
		if ((more = utf8_open(&ud, *text)) == UTF8_MORE) {
			while (*end != '\0' && more == UTF8_MORE) {
				more = utf8_append(&ud, *end);
				end++;
			}
		}
	}
	*used = end - text;
	return (xstrndup(text, *used));
}

void
cmd_output_print_wrapped(struct cmdq_item *item, enum cmd_output_style style,
    u_int indent, const char *text)
{
	const char	*remaining = text;
	char		*prefix, *chunk;
	size_t		 used;
	u_int		 width = cmd_output_width(item);

	prefix = xcalloc(indent + 1, 1);
	memset(prefix, ' ', indent);
	if (width > indent + 16)
		width -= indent;
	else
		width = 16;

	do {
		chunk = cmd_output_table_chunk(remaining, width, &used);
		cmd_output_print(item, style, "%s%s", prefix, chunk);
		remaining += used;
		while (*remaining == ' ')
			remaining++;
		free(chunk);
	} while (*remaining != '\0');
	free(prefix);
}

static void
cmd_output_table_print_row(struct cmd_output_table *table,
    struct cmd_output_row *row, u_int *widths, u_int lastwidth)
{
	const char	*remaining, *start, *reset;
	char		*prefix, *continuation, *chunk;
	size_t		 used;
	u_int		 prefixwidth;

	prefix = cmd_output_table_prefix(table, row, widths);
	prefixwidth = 0;
	if (table->columns > 1) {
		u_int	i;

		for (i = 0; i + 1 < table->columns; i++)
			prefixwidth += widths[i] + 2;
	}
	continuation = xcalloc(prefixwidth + 1, 1);
	memset(continuation, ' ', prefixwidth);

	start = cmd_output_colour(table->item,
	    row->styles[table->columns - 1]);
	reset = cmd_output_reset(table->item,
	    row->styles[table->columns - 1]);
	remaining = row->cells[table->columns - 1];
	do {
		chunk = cmd_output_table_chunk(remaining, lastwidth, &used);
		cmdq_print(table->item, "%s%s%s%s", prefix, start, chunk, reset);
		remaining += used;
		while (*remaining == ' ')
			remaining++;
		free(chunk);
		free(prefix);
		prefix = xstrdup(continuation);
	} while (*remaining != '\0');

	free(prefix);
	free(continuation);
}

void
cmd_output_table_print(struct cmd_output_table *table)
{
	struct cmd_output_row	 header;
	u_int			*widths, i, j, total, available, lastwidth;
	u_int			 prefix, minimum, widest;
	enum cmd_output_style	*headerstyles;

	if (*table->title != '\0')
		cmd_output_print(table->item, CMD_OUTPUT_HEADING, "%s",
		    table->title);

	widths = xcalloc(table->columns, sizeof *widths);
	for (i = 0; i < table->columns; i++) {
		widths[i] = utf8_cstrwidth(table->headers[i]);
		for (j = 0; j < table->nrows; j++) {
			u_int	width = utf8_cstrwidth(table->rows[j].cells[i]);

			if (width > widths[i])
				widths[i] = width;
		}
	}

	total = (table->columns - 1) * 2;
	for (i = 0; i < table->columns; i++)
		total += widths[i];
	available = cmd_output_width(table->item);
	lastwidth = widths[table->columns - 1];
	if (total > available) {
		/*
		 * Preserve a useful final value column. Earlier columns are
		 * clipped only when wrapping the final column is not enough.
		 */
		prefix = total - lastwidth;
		minimum = (available >= 40 ? 16 : 8);
		while (prefix + minimum > available) {
			widest = table->columns;
			for (i = 0; i + 1 < table->columns; i++) {
				if (widths[i] <= 3)
					continue;
				if (widest == table->columns ||
				    widths[i] > widths[widest])
					widest = i;
			}
			if (widest == table->columns)
				break;
			widths[widest]--;
			prefix--;
		}
		if (available > prefix)
			lastwidth = available - prefix;
		else
			lastwidth = 1;
	}
	if (lastwidth == 0)
		lastwidth = 1;

	header.cells = table->headers;
	headerstyles = xcalloc(table->columns, sizeof *headerstyles);
	for (i = 0; i < table->columns; i++)
		headerstyles[i] = CMD_OUTPUT_DIM;
	header.styles = headerstyles;
	cmd_output_table_print_row(table, &header, widths, lastwidth);
	free(headerstyles);

	for (i = 0; i < table->nrows; i++)
		cmd_output_table_print_row(table, &table->rows[i], widths,
		    lastwidth);
	cmdq_print(table->item, "%s", "");
	free(widths);
}

void
cmd_output_table_free(struct cmd_output_table *table)
{
	u_int	i, j;

	for (i = 0; i < table->nrows; i++) {
		for (j = 0; j < table->columns; j++)
			free(table->rows[i].cells[j]);
		free(table->rows[i].cells);
		free(table->rows[i].styles);
	}
	for (i = 0; i < table->columns; i++)
		free(table->headers[i]);
	free(table->headers);
	free(table->rows);
	free(table->title);
	free(table);
}
