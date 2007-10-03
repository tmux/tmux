/* $Id: cmd.c,v 1.3 2007-10-03 12:34:16 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

int	cmd_prefix = META;

void	cmd_fn_create(struct client *, struct cmd *);
void	cmd_fn_detach(struct client *, struct cmd *);
void	cmd_fn_last(struct client *, struct cmd *);
void	cmd_fn_meta(struct client *, struct cmd *);
void	cmd_fn_next(struct client *, struct cmd *);
void	cmd_fn_previous(struct client *, struct cmd *);
void	cmd_fn_refresh(struct client *, struct cmd *);
void	cmd_fn_select(struct client *, struct cmd *);
void	cmd_fn_windowinfo(struct client *, struct cmd *);

const struct cmd cmd_default[] = {
	{ '0', cmd_fn_select, 0, NULL },
	{ '1', cmd_fn_select, 1, NULL },
	{ '2', cmd_fn_select, 2, NULL },
	{ '3', cmd_fn_select, 3, NULL },
	{ '4', cmd_fn_select, 4, NULL },
	{ '5', cmd_fn_select, 5, NULL },
	{ '6', cmd_fn_select, 6, NULL },
	{ '7', cmd_fn_select, 7, NULL },
	{ '8', cmd_fn_select, 8, NULL },
	{ '9', cmd_fn_select, 9, NULL },
	{ 'C', cmd_fn_create, 0, NULL },
	{ 'c', cmd_fn_create, 0, NULL },
	{ 'D', cmd_fn_detach, 0, NULL },
	{ 'd', cmd_fn_detach, 0, NULL },
	{ 'N', cmd_fn_next, 0, NULL },
	{ 'n', cmd_fn_next, 0, NULL },
	{ 'P', cmd_fn_previous, 0, NULL },
	{ 'p', cmd_fn_previous, 0, NULL },
	{ 'R', cmd_fn_refresh, 0, NULL },
	{ 'r', cmd_fn_refresh, 0, NULL },
	{ 'L', cmd_fn_last, 0, NULL },
	{ 'l', cmd_fn_last, 0, NULL },
	{ 'I', cmd_fn_windowinfo, 0, NULL },
	{ 'i', cmd_fn_windowinfo, 0, NULL },
	{ META, cmd_fn_meta, 0, NULL },
};
u_int	cmd_count = (sizeof cmd_default / sizeof cmd_default[0]);
struct cmd *cmd_table;

const struct bind cmd_bind_table[] = {
	{ "select", 	cmd_fn_select, BIND_NUMBER|BIND_USER },
	{ "create", 	cmd_fn_create, BIND_STRING|BIND_USER },
	{ "detach", 	cmd_fn_detach, 0 },
	{ "next",	cmd_fn_next, 0 },
	{ "previous", 	cmd_fn_previous, 0 },
	{ "refresh",	cmd_fn_refresh, 0 },
	{ "last",	cmd_fn_last, 0 },
	{ "window-info",cmd_fn_windowinfo, 0 },
	{ "meta",	cmd_fn_meta, 0 }
};
#define NCMDBIND (sizeof cmd_bind_table / sizeof cmd_bind_table[0])

const struct bind *
cmd_lookup_bind(const char *name)
{
	const struct bind	*bind;
	u_int		         i;

	for (i = 0; i < NCMDBIND; i++) {
		bind = cmd_bind_table + i;
		if (strcmp(bind->name, name) == 0)
			return (bind);
	}
	return (NULL);
}

void
cmd_add_bind(int key, u_int num, char *str, const struct bind *bind)
{
	struct cmd	*cmd = NULL;
	u_int		 i;

	for (i = 0; i < cmd_count; i++) {
		cmd = cmd_table + i;
		if (cmd->key == key)
			break;
	}
	if (i == cmd_count) {
		for (i = 0; i < cmd_count; i++) {
			cmd = cmd_table + i;
			if (cmd->key == KEYC_NONE)
				break;
		}
		if (i == cmd_count) {
			cmd_count++;
			cmd_table = xrealloc(cmd_table,
			    cmd_count, sizeof cmd_table[0]);
			cmd = cmd_table + cmd_count - 1;
		}
	}

	cmd->key = key;
	cmd->fn = bind->fn;
	if (bind->flags & BIND_USER) {
		if (bind->flags & BIND_STRING)
			cmd->str = xstrdup(str);
		if (bind->flags & BIND_NUMBER)
			cmd->num = num;
	}
}

void
cmd_remove_bind(int key)
{
	struct cmd	*cmd;
	u_int		 i;

	for (i = 0; i < cmd_count; i++) {
		cmd = cmd_table + i;
		if (cmd->key == key) {
			cmd->key = KEYC_NONE;
			break;
		}
	}
}

void
cmd_init(void)
{
	cmd_table = xmalloc(sizeof cmd_default);
	memcpy(cmd_table, cmd_default, sizeof cmd_default);
}

void
cmd_free(void)
{
	/* XXX free strings */
	xfree(cmd_table);
}

void
cmd_dispatch(struct client *c, int key)
{
	struct cmd	*cmd;
	u_int		 i;

	for (i = 0; i < cmd_count; i++) {
		cmd = cmd_table + i;
		if (cmd->key != KEYC_NONE && cmd->key == key)
			cmd->fn(c, cmd);
	}
}

void
cmd_fn_create(struct client *c, struct cmd *cmd)
{
	char	*s;

	s = cmd->str;
	if (s == NULL)
		s = default_command;
	if (session_new(c->session, s, c->sx, c->sy) != 0)
		server_write_message(c, "%s failed", s); /* XXX */
	else
		server_draw_client(c, 0, c->sy - 1);
}

void
cmd_fn_detach(struct client *c, unused struct cmd *cmd)
{
	server_write_client(c, MSG_DETACH, NULL, 0);
}

void
cmd_fn_last(struct client *c, unused struct cmd *cmd)
{
	if (session_last(c->session) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "No last window"); 
}

void
cmd_fn_meta(struct client *c, unused struct cmd *cmd)
{
	window_key(c->session->window, cmd_prefix);
}

void
cmd_fn_next(struct client *c, unused struct cmd *cmd)
{
	if (session_next(c->session) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "No next window"); 
}

void
cmd_fn_previous(struct client *c, unused struct cmd *cmd)
{
	if (session_previous(c->session) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "No previous window"); 
}

void
cmd_fn_refresh(struct client *c, unused struct cmd *cmd)
{
	server_draw_client(c, 0, c->sy - 1);
}

void
cmd_fn_select(struct client *c, struct cmd *cmd)
{
	if (session_select(c->session, cmd->num) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "Window %u not present", cmd->num);
}

void
cmd_fn_windowinfo(struct client *c, unused struct cmd *cmd)
{
	struct window	*w;
	char 		*buf;
	size_t		 len;
	u_int		 i;

	len = c->sx + 1;
	buf = xmalloc(len);

	w = c->session->window;
	window_index(&c->session->windows, w, &i);
	xsnprintf(buf, len, "%u:%s \"%s\" (size %u,%u) (cursor %u,%u) "
	    "(region %u,%u)", i, w->name, w->screen.title, w->screen.sx,
	    w->screen.sy, w->screen.cx, w->screen.cy, w->screen.ry_upper,
	    w->screen.ry_lower);

	server_write_message(c, "%s", buf);
	xfree(buf);
}
