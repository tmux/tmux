/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Show environment.
 */

static enum cmd_retval	cmd_show_environment_exec(struct cmd *,
			    struct cmdq_item *);

static char	*cmd_show_environment_escape(struct environ_entry *);
static void	 cmd_show_environment_print(struct cmd *, struct cmdq_item *,
		     struct environ_entry *);

const struct cmd_entry cmd_show_environment_entry = {
	.name = "show-environment",
	.alias = "showenv",

	.args = { "hgst:", 0, 1, NULL },
	.usage = "[-hgs] " CMD_TARGET_SESSION_USAGE " [name]",

	.target = { 't', CMD_FIND_SESSION, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_environment_exec
};

static char *
cmd_show_environment_escape(struct environ_entry *envent)
{
	const char	*value = envent->value;
	char		 c, *out, *ret;

	out = ret = xmalloc(strlen(value) * 2 + 1); /* at most twice the size */
	while ((c = *value++) != '\0') {
		/* POSIX interprets $ ` " and \ in double quotes. */
		if (c == '$' || c == '`' || c == '"' || c == '\\')
			*out++ = '\\';
		*out++ = c;
	}
	*out = '\0';

	return (ret);
}

static void
cmd_show_environment_print(struct cmd *self, struct cmdq_item *item,
    struct environ_entry *envent)
{
	struct args	*args = cmd_get_args(self);
	char		*escaped;

	if (!args_has(args, 'h') && (envent->flags & ENVIRON_HIDDEN))
		return;
	if (args_has(args, 'h') && (~envent->flags & ENVIRON_HIDDEN))
		return;

	if (!args_has(args, 's')) {
		if (envent->value != NULL)
			cmdq_print(item, "%s=%s", envent->name, envent->value);
		else
			cmdq_print(item, "-%s", envent->name);
		return;
	}

	if (envent->value != NULL) {
		escaped = cmd_show_environment_escape(envent);
		cmdq_print(item, "%s=\"%s\"; export %s;", envent->name, escaped,
		    envent->name);
		free(escaped);
	} else
		cmdq_print(item, "unset %s;", envent->name);
}

static enum cmd_retval
cmd_show_environment_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct environ		*env;
	struct environ_entry	*envent;
	const char		*tflag, *name = args_string(args, 0);

	if ((tflag = args_get(args, 't')) != NULL) {
		if (target->s == NULL) {
			cmdq_error(item, "no such session: %s", tflag);
			return (CMD_RETURN_ERROR);
		}
	}

	if (args_has(args, 'g'))
		env = global_environ;
	else {
		if (target->s == NULL) {
			tflag = args_get(args, 't');
			if (tflag != NULL)
				cmdq_error(item, "no such session: %s", tflag);
			else
				cmdq_error(item, "no current session");
			return (CMD_RETURN_ERROR);
		}
		env = target->s->environ;
	}

	if (name != NULL) {
		envent = environ_find(env, name);
		if (envent == NULL) {
			cmdq_error(item, "unknown variable: %s", name);
			return (CMD_RETURN_ERROR);
		}
		cmd_show_environment_print(self, item, envent);
		return (CMD_RETURN_NORMAL);
	}

	envent = environ_first(env);
	while (envent != NULL) {
		cmd_show_environment_print(self, item, envent);
		envent = environ_next(envent);
	}
	return (CMD_RETURN_NORMAL);
}
