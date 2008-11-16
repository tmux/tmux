/* $Id: cmd-set-option.c,v 1.44 2008-11-16 13:28:59 nicm Exp $ */

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

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Set an option.
 */

int	cmd_set_option_parse(struct cmd *, int, char **, char **);
void	cmd_set_option_exec(struct cmd *, struct cmd_ctx *);
void	cmd_set_option_send(struct cmd *, struct buffer *);
void	cmd_set_option_recv(struct cmd *, struct buffer *);
void	cmd_set_option_free(struct cmd *);
void	cmd_set_option_print(struct cmd *, char *, size_t);

struct cmd_set_option_data {
	char	*target;
	int	 flag_global;
	char	*option;
	char	*value;
};

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set",
	"[-t target-session] option value",
	0,
	NULL,
	cmd_set_option_parse,
	cmd_set_option_exec,
	cmd_set_option_send,
	cmd_set_option_recv,
	cmd_set_option_free,
	cmd_set_option_print
};

const char *set_option_bell_action_list[] = {
	"none", "any", "current", NULL
};
const char *set_option_mode_keys_list[] = {
	"emacs", "vi", NULL
};
const struct set_option_entry set_option_table[NSETOPTION] = {
	{ "bell-action", SET_OPTION_CHOICE, 0, 0, set_option_bell_action_list },
	{ "buffer-limit", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "default-command", SET_OPTION_STRING, 0, 0, NULL },
	{ "display-time", SET_OPTION_NUMBER, 1, INT_MAX, NULL },
	{ "history-limit", SET_OPTION_NUMBER, 0, SHRT_MAX, NULL },
	{ "mode-keys", SET_OPTION_CHOICE, 0, 0, set_option_mode_keys_list },
	{ "prefix", SET_OPTION_KEY, 0, 0, NULL },
	{ "remain-by-default", SET_OPTION_FLAG, 0, 0, NULL },
	{ "set-titles", SET_OPTION_FLAG, 0, 0, NULL },
	{ "status", SET_OPTION_FLAG, 0, 0, NULL },
	{ "status-bg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-fg", SET_OPTION_COLOUR, 0, 0, NULL },
	{ "status-interval", SET_OPTION_NUMBER, 0, INT_MAX, NULL },
	{ "status-left", SET_OPTION_STRING, 0, 0, NULL },
	{ "status-right", SET_OPTION_STRING, 0, 0, NULL },
	{ "utf8", SET_OPTION_FLAG, 0, 0, NULL },
};

void	set_option_string(struct cmd_ctx *,
	    struct options *, const struct set_option_entry *, char *);
void	set_option_number(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_key(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_colour(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_flag(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);
void	set_option_choice(struct cmd_ctx *,
    	    struct options *, const struct set_option_entry *, char *);

int
cmd_set_option_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_set_option_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->flag_global = 1;
	data->option = NULL;
	data->value = NULL;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "t:s:")) != EOF) {
		switch (opt) {
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			data->flag_global = 0;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1 && argc != 2)
		goto usage;

	data->option = xstrdup(argv[0]);
	if (argc == 2)
		data->value = xstrdup(argv[1]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_set_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_set_option_data	*data = self->data;
	struct session			*s;
	struct client			*c;
	struct options			*oo;
	const struct set_option_entry   *entry;
	u_int				 i;

	if (data == NULL)
		return;

	if (data->flag_global ||
	    ((s = cmd_find_session(ctx, data->target))) == NULL)
		oo = &global_options;
	else
		oo = &s->options;

	if (*data->option == '\0') {
		ctx->error(ctx, "invalid option");
		return;
	}

	entry = NULL;
	for (i = 0; i < NSETOPTION; i++) {
		if (strncmp(set_option_table[i].name,
		    data->option, strlen(data->option)) != 0)
			continue;
		if (entry != NULL) {
			ctx->error(ctx, "ambiguous option: %s", data->option);
			return;
		}
		entry = &set_option_table[i];

		/* Bail now if an exact match. */
		if (strcmp(entry->name, data->option) == 0)
			break;
	}
	if (entry == NULL) {
		ctx->error(ctx, "unknown option: %s", data->option);
		return;
	}

	switch (entry->type) {
	case SET_OPTION_STRING:
		set_option_string(ctx, oo, entry, data->value);
		break;
	case SET_OPTION_NUMBER:
		set_option_number(ctx, oo, entry, data->value);
		break;
	case SET_OPTION_KEY:
		set_option_key(ctx, oo, entry, data->value);
		break;
	case SET_OPTION_COLOUR:
		set_option_colour(ctx, oo, entry, data->value);
		break;
	case SET_OPTION_FLAG:
		set_option_flag(ctx, oo, entry, data->value);
		break;
	case SET_OPTION_CHOICE:
		set_option_choice(ctx, oo, entry, data->value);
		break;
	}

	recalculate_sizes();
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL)
			server_redraw_client(c);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
set_option_string(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	options_set_string(oo, entry->name, "%s", value);
}

void
set_option_number(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	long long	number;
	const char     *errstr;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	number = strtonum(value, entry->minimum, entry->maximum, &errstr);
	if (errstr != NULL) {
		ctx->error(ctx, "value is %s: %s", errstr, value);
		return;
	}
	options_set_number(oo, entry->name, number);
}

void
set_option_key(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	int	key;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((key = key_string_lookup_string(value)) == KEYC_NONE) {
		ctx->error(ctx, "unknown key: %s", value);
		return;
	}
	options_set_number(oo, entry->name, key);

}

void
set_option_colour(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	u_char	colour;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	if ((colour = colour_fromstring(value)) > 8) {
		ctx->error(ctx, "bad colour: %s", value);
		return;
	}

	options_set_number(oo, entry->name, colour);
}

void
set_option_flag(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	int	flag;

	if (value == NULL || *value == '\0')
		flag = !options_get_number(oo, entry->name);
	else {
		if ((value[0] == '1' && value[1] == '\0') ||
		    strcasecmp(value, "on") == 0 ||
		    strcasecmp(value, "yes") == 0)
			flag = 1;
		else if ((value[0] == '0' && value[1] == '\0') ||
		    strcasecmp(value, "off") == 0 ||
		    strcasecmp(value, "no") == 0)
			flag = 0;
		else {
			ctx->error(ctx, "bad value: %s", value);
			return;
		}
	}

	options_set_number(oo, entry->name, flag);
}

void
set_option_choice(struct cmd_ctx *ctx, struct options *oo,
    const struct set_option_entry *entry, char *value)
{
	const char     **choicep;
	int		 n, choice = -1;

	if (value == NULL) {
		ctx->error(ctx, "empty value");
		return;
	}

	n = 0;
	for (choicep = entry->choices; *choicep != NULL; choicep++) {
		n++;
		if (strncmp(*choicep, value, strlen(value)) != 0)
			continue;

		if (choice != -1) {
			ctx->error(ctx, "ambiguous option: %s", value);
			return;
		}
		choice = n - 1;
	}
	if (choice == -1) {
		ctx->error(ctx, "unknown option: %s", value);
		return;
	}

	options_set_number(oo, entry->name, choice);
}

void
cmd_set_option_send(struct cmd *self, struct buffer *b)
{
	struct cmd_set_option_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->option);
	cmd_send_string(b, data->value);
}

void
cmd_set_option_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_set_option_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->option = cmd_recv_string(b);
	data->value = cmd_recv_string(b);
}

void
cmd_set_option_free(struct cmd *self)
{
	struct cmd_set_option_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->option != NULL)
		xfree(data->option);
	if (data->value != NULL)
		xfree(data->value);
	xfree(data);
}

void
cmd_set_option_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_set_option_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->option != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->option);
	if (off < len && data->value != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->value);
}
