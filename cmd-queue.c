/* $OpenBSD$ */

/*
 * Copyright (c) 2013 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/* Command queue flags. */
#define CMDQ_FIRED 0x1
#define CMDQ_WAITING 0x2

/* Command queue item type. */
enum cmdq_type {
	CMDQ_COMMAND,
	CMDQ_CALLBACK,
};

/* Command queue item. */
struct cmdq_item {
	char			*name;
	struct cmdq_list	*queue;
	struct cmdq_item	*next;

	struct client		*client;
	struct client		*target_client;

	enum cmdq_type		 type;
	u_int			 group;

	u_int			 number;
	time_t			 time;

	int			 flags;

	struct cmdq_state	*state;
	struct cmd_find_state	 source;
	struct cmd_find_state	 target;

	struct cmd_list		*cmdlist;
	struct cmd		*cmd;

	cmdq_cb			 cb;
	void			*data;

	TAILQ_ENTRY(cmdq_item)	 entry;
};
TAILQ_HEAD(cmdq_item_list, cmdq_item);

/*
 * Command queue state. This is the context for commands on the command queue.
 * It holds information about how the commands were fired (the key and flags),
 * any additional formats for the commands, and the current default target.
 * Multiple commands can share the same state and a command may update the
 * default target.
 */
struct cmdq_state {
	int			 references;
	int			 flags;

	struct format_tree	*formats;

	struct key_event	 event;
	struct cmd_find_state	 current;
};

/* Command queue. */
struct cmdq_list {
	struct cmdq_item	*item;
	struct cmdq_item_list	 list;
};

/* Get command queue name. */
static const char *
cmdq_name(struct client *c)
{
	static char	s[256];

	if (c == NULL)
		return ("<global>");
	if (c->name != NULL)
		xsnprintf(s, sizeof s, "<%s>", c->name);
	else
		xsnprintf(s, sizeof s, "<%p>", c);
	return (s);
}

/* Get command queue from client. */
static struct cmdq_list *
cmdq_get(struct client *c)
{
	static struct cmdq_list *global_queue;

	if (c == NULL) {
		if (global_queue == NULL)
			global_queue = cmdq_new();
		return (global_queue);
	}
	return (c->queue);
}

/* Create a queue. */
struct cmdq_list *
cmdq_new(void)
{
	struct cmdq_list	*queue;

	queue = xcalloc(1, sizeof *queue);
	TAILQ_INIT (&queue->list);
	return (queue);
}

/* Free a queue. */
void
cmdq_free(struct cmdq_list *queue)
{
	if (!TAILQ_EMPTY(&queue->list))
		fatalx("queue not empty");
	free(queue);
}

/* Get item name. */
const char *
cmdq_get_name(struct cmdq_item *item)
{
	return (item->name);
}

/* Get item client. */
struct client *
cmdq_get_client(struct cmdq_item *item)
{
	return (item->client);
}

/* Get item target client. */
struct client *
cmdq_get_target_client(struct cmdq_item *item)
{
	return (item->target_client);
}

/* Get item state. */
struct cmdq_state *
cmdq_get_state(struct cmdq_item *item)
{
	return (item->state);
}

/* Get item target. */
struct cmd_find_state *
cmdq_get_target(struct cmdq_item *item)
{
	return (&item->target);
}

/* Get item source. */
struct cmd_find_state *
cmdq_get_source(struct cmdq_item *item)
{
	return (&item->source);
}

/* Get state event. */
struct key_event *
cmdq_get_event(struct cmdq_item *item)
{
	return (&item->state->event);
}

/* Get state current target. */
struct cmd_find_state *
cmdq_get_current(struct cmdq_item *item)
{
	return (&item->state->current);
}

/* Get state flags. */
int
cmdq_get_flags(struct cmdq_item *item)
{
	return (item->state->flags);
}

/* Create a new state. */
struct cmdq_state *
cmdq_new_state(struct cmd_find_state *current, struct key_event *event,
    int flags)
{
	struct cmdq_state	*state;

	state = xcalloc(1, sizeof *state);
	state->references = 1;
	state->flags = flags;

	if (event != NULL)
		memcpy(&state->event, event, sizeof state->event);
	else
		state->event.key = KEYC_NONE;
	if (current != NULL && cmd_find_valid_state(current))
		cmd_find_copy_state(&state->current, current);
	else
		cmd_find_clear_state(&state->current, 0);

	return (state);
}

/* Add a reference to a state. */
struct cmdq_state *
cmdq_link_state(struct cmdq_state *state)
{
	state->references++;
	return (state);
}

/* Make a copy of a state. */
struct cmdq_state *
cmdq_copy_state(struct cmdq_state *state, struct cmd_find_state *current)
{
	if (current != NULL)
		return (cmdq_new_state(current, &state->event, state->flags));
	return (cmdq_new_state(&state->current, &state->event, state->flags));
}

/* Free a state. */
void
cmdq_free_state(struct cmdq_state *state)
{
	if (--state->references != 0)
		return;

	if (state->formats != NULL)
		format_free(state->formats);
	free(state);
}

/* Add a format to command queue. */
void
cmdq_add_format(struct cmdq_state *state, const char *key, const char *fmt, ...)
{
	va_list	 ap;
	char	*value;

	va_start(ap, fmt);
	xvasprintf(&value, fmt, ap);
	va_end(ap);

	if (state->formats == NULL)
		state->formats = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_add(state->formats, key, "%s", value);

	free(value);
}

/* Add formats to command queue. */
void
cmdq_add_formats(struct cmdq_state *state, struct format_tree *ft)
{
	if (state->formats == NULL)
		state->formats = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_merge(state->formats, ft);
}

/* Merge formats from item. */
void
cmdq_merge_formats(struct cmdq_item *item, struct format_tree *ft)
{
	const struct cmd_entry	*entry;

	if (item->cmd != NULL) {
		entry = cmd_get_entry(item->cmd);
		format_add(ft, "command", "%s", entry->name);
	}
	if (item->state->formats != NULL)
		format_merge(ft, item->state->formats);
}

/* Append an item. */
struct cmdq_item *
cmdq_append(struct client *c, struct cmdq_item *item)
{
	struct cmdq_list	*queue = cmdq_get(c);
	struct cmdq_item	*next;

	do {
		next = item->next;
		item->next = NULL;

		if (c != NULL)
			c->references++;
		item->client = c;

		item->queue = queue;
		TAILQ_INSERT_TAIL(&queue->list, item, entry);
		log_debug("%s %s: %s", __func__, cmdq_name(c), item->name);

		item = next;
	} while (item != NULL);
	return (TAILQ_LAST(&queue->list, cmdq_item_list));
}

/* Insert an item. */
struct cmdq_item *
cmdq_insert_after(struct cmdq_item *after, struct cmdq_item *item)
{
	struct client		*c = after->client;
	struct cmdq_list	*queue = after->queue;
	struct cmdq_item	*next;

	do {
		next = item->next;
		item->next = after->next;
		after->next = item;

		if (c != NULL)
			c->references++;
		item->client = c;

		item->queue = queue;
		TAILQ_INSERT_AFTER(&queue->list, after, item, entry);
		log_debug("%s %s: %s after %s", __func__, cmdq_name(c),
		    item->name, after->name);

		after = item;
		item = next;
	} while (item != NULL);
	return (after);
}

/* Insert a hook. */
void
cmdq_insert_hook(struct session *s, struct cmdq_item *item,
    struct cmd_find_state *current, const char *fmt, ...)
{
	struct cmdq_state		*state = item->state;
	struct cmd			*cmd = item->cmd;
	struct args			*args = cmd_get_args(cmd);
	struct args_entry		*ae;
	struct args_value		*av;
	struct options			*oo;
	va_list				 ap;
	char				*name, tmp[32], flag, *arguments;
	u_int				 i;
	const char			*value;
	struct cmdq_item		*new_item;
	struct cmdq_state		*new_state;
	struct options_entry		*o;
	struct options_array_item	*a;
	struct cmd_list			*cmdlist;

	if (item->state->flags & CMDQ_STATE_NOHOOKS)
		return;
	if (s == NULL)
		oo = global_s_options;
	else
		oo = s->options;

	va_start(ap, fmt);
	xvasprintf(&name, fmt, ap);
	va_end(ap);

	o = options_get(oo, name);
	if (o == NULL) {
		free(name);
		return;
	}
	log_debug("running hook %s (parent %p)", name, item);

	/*
	 * The hooks get a new state because they should not update the current
	 * target or formats for any subsequent commands.
	 */
	new_state = cmdq_new_state(current, &state->event, CMDQ_STATE_NOHOOKS);
	cmdq_add_format(new_state, "hook", "%s", name);

	arguments = args_print(args);
	cmdq_add_format(new_state, "hook_arguments", "%s", arguments);
	free(arguments);

	for (i = 0; i < args_count(args); i++) {
		xsnprintf(tmp, sizeof tmp, "hook_argument_%d", i);
		cmdq_add_format(new_state, tmp, "%s", args_string(args, i));
	}
	flag = args_first(args, &ae);
	while (flag != 0) {
		value = args_get(args, flag);
		if (value == NULL) {
			xsnprintf(tmp, sizeof tmp, "hook_flag_%c", flag);
			cmdq_add_format(new_state, tmp, "1");
		} else {
			xsnprintf(tmp, sizeof tmp, "hook_flag_%c", flag);
			cmdq_add_format(new_state, tmp, "%s", value);
		}

		i = 0;
		av = args_first_value(args, flag);
		while (av != NULL) {
			xsnprintf(tmp, sizeof tmp, "hook_flag_%c_%d", flag, i);
			cmdq_add_format(new_state, tmp, "%s", av->string);
			i++;
			av = args_next_value(av);
		}

		flag = args_next(&ae);
	}

	a = options_array_first(o);
	while (a != NULL) {
		cmdlist = options_array_item_value(a)->cmdlist;
		if (cmdlist != NULL) {
			new_item = cmdq_get_command(cmdlist, new_state);
			if (item != NULL)
				item = cmdq_insert_after(item, new_item);
			else
				item = cmdq_append(NULL, new_item);
		}
		a = options_array_next(a);
	}

	cmdq_free_state(new_state);
	free(name);
}

/* Continue processing command queue. */
void
cmdq_continue(struct cmdq_item *item)
{
	item->flags &= ~CMDQ_WAITING;
}

/* Remove an item. */
static void
cmdq_remove(struct cmdq_item *item)
{
	if (item->client != NULL)
		server_client_unref(item->client);
	if (item->cmdlist != NULL)
		cmd_list_free(item->cmdlist);
	cmdq_free_state(item->state);

	TAILQ_REMOVE(&item->queue->list, item, entry);

	free(item->name);
	free(item);
}

/* Remove all subsequent items that match this item's group. */
static void
cmdq_remove_group(struct cmdq_item *item)
{
	struct cmdq_item	*this, *next;

	if (item->group == 0)
		return;
	this = TAILQ_NEXT(item, entry);
	while (this != NULL) {
		next = TAILQ_NEXT(this, entry);
		if (this->group == item->group)
			cmdq_remove(this);
		this = next;
	}
}

/* Empty command callback. */
static enum cmd_retval
cmdq_empty_command(__unused struct cmdq_item *item, __unused void *data)
{
	return (CMD_RETURN_NORMAL);
}

/* Get a command for the command queue. */
struct cmdq_item *
cmdq_get_command(struct cmd_list *cmdlist, struct cmdq_state *state)
{
	struct cmdq_item	*item, *first = NULL, *last = NULL;
	struct cmd		*cmd;
	const struct cmd_entry	*entry;
	int			 created = 0;

	if ((cmd = cmd_list_first(cmdlist)) == NULL)
		return (cmdq_get_callback(cmdq_empty_command, NULL));

	if (state == NULL) {
		state = cmdq_new_state(NULL, NULL, 0);
		created = 1;
	}

	while (cmd != NULL) {
		entry = cmd_get_entry(cmd);

		item = xcalloc(1, sizeof *item);
		xasprintf(&item->name, "[%s/%p]", entry->name, item);
		item->type = CMDQ_COMMAND;

		item->group = cmd_get_group(cmd);
		item->state = cmdq_link_state(state);

		item->cmdlist = cmdlist;
		item->cmd = cmd;

		cmdlist->references++;
		log_debug("%s: %s group %u", __func__, item->name, item->group);

		if (first == NULL)
			first = item;
		if (last != NULL)
			last->next = item;
		last = item;

		cmd = cmd_list_next(cmd);
	}

	if (created)
		cmdq_free_state(state);
	return (first);
}

/* Fill in flag for a command. */
static enum cmd_retval
cmdq_find_flag(struct cmdq_item *item, struct cmd_find_state *fs,
    const struct cmd_entry_flag *flag)
{
	const char	*value;

	if (flag->flag == 0) {
		cmd_find_from_client(fs, item->target_client, 0);
		return (CMD_RETURN_NORMAL);
	}

	value = args_get(cmd_get_args(item->cmd), flag->flag);
	if (cmd_find_target(fs, item, value, flag->type, flag->flags) != 0) {
		cmd_find_clear_state(fs, 0);
		return (CMD_RETURN_ERROR);
	}
	return (CMD_RETURN_NORMAL);
}

/* Add message with command. */
static void
cmdq_add_message(struct cmdq_item *item)
{
	struct client		*c = item->client;
	struct cmdq_state	*state = item->state;
	const char		*key;
	char			*tmp;
	uid_t                    uid;
	struct passwd		*pw;
	char                    *user = NULL;

	tmp = cmd_print(item->cmd);
	if (c != NULL) {
		uid = proc_get_peer_uid(c->peer);
		if (uid != (uid_t)-1 && uid != getuid()) {
			if ((pw = getpwuid(uid)) != NULL)
				xasprintf(&user, "[%s]", pw->pw_name);
			else
				user = xstrdup("[unknown]");
		} else
			user = xstrdup("");
		if (c->session != NULL && state->event.key != KEYC_NONE) {
			key = key_string_lookup_key(state->event.key, 0);
			server_add_message("%s%s key %s: %s", c->name, user,
			    key, tmp);
		} else {
			server_add_message("%s%s command: %s", c->name, user,
			    tmp);
		}
		free(user);
	} else
		server_add_message("command: %s", tmp);
	free(tmp);
}

/* Fire command on command queue. */
static enum cmd_retval
cmdq_fire_command(struct cmdq_item *item)
{
	const char		*name = cmdq_name(item->client);
	struct cmdq_state	*state = item->state;
	struct cmd		*cmd = item->cmd;
	struct args		*args = cmd_get_args(cmd);
	const struct cmd_entry	*entry = cmd_get_entry(cmd);
	struct client		*tc, *saved = item->client;
	enum cmd_retval		 retval;
	struct cmd_find_state	*fsp, fs;
	int			 flags, quiet = 0;
	char			*tmp;

	if (cfg_finished)
		cmdq_add_message(item);
	if (log_get_level() > 1) {
		tmp = cmd_print(cmd);
		log_debug("%s %s: (%u) %s", __func__, name, item->group, tmp);
		free(tmp);
	}

	flags = !!(state->flags & CMDQ_STATE_CONTROL);
	cmdq_guard(item, "begin", flags);

	if (item->client == NULL)
		item->client = cmd_find_client(item, NULL, 1);

	if (entry->flags & CMD_CLIENT_CANFAIL)
		quiet = 1;
	if (entry->flags & CMD_CLIENT_CFLAG) {
		tc = cmd_find_client(item, args_get(args, 'c'), quiet);
		if (tc == NULL && !quiet) {
			retval = CMD_RETURN_ERROR;
			goto out;
		}
	} else if (entry->flags & CMD_CLIENT_TFLAG) {
		tc = cmd_find_client(item, args_get(args, 't'), quiet);
		if (tc == NULL && !quiet) {
			retval = CMD_RETURN_ERROR;
			goto out;
		}
	} else
		tc = cmd_find_client(item, NULL, 1);
	item->target_client = tc;

	retval = cmdq_find_flag(item, &item->source, &entry->source);
	if (retval == CMD_RETURN_ERROR)
		goto out;
	retval = cmdq_find_flag(item, &item->target, &entry->target);
	if (retval == CMD_RETURN_ERROR)
		goto out;

	retval = entry->exec(cmd, item);
	if (retval == CMD_RETURN_ERROR)
		goto out;

	if (entry->flags & CMD_AFTERHOOK) {
		if (cmd_find_valid_state(&item->target))
			fsp = &item->target;
		else if (cmd_find_valid_state(&item->state->current))
			fsp = &item->state->current;
		else if (cmd_find_from_client(&fs, item->client, 0) == 0)
			fsp = &fs;
		else
			goto out;
		cmdq_insert_hook(fsp->s, item, fsp, "after-%s", entry->name);
	}

out:
	item->client = saved;
	if (retval == CMD_RETURN_ERROR) {
		fsp = NULL;
		if (cmd_find_valid_state(&item->target))
			fsp = &item->target;
		else if (cmd_find_valid_state(&item->state->current))
			fsp = &item->state->current;
		else if (cmd_find_from_client(&fs, item->client, 0) == 0)
			fsp = &fs;
		cmdq_insert_hook(fsp != NULL ? fsp->s : NULL, item, fsp,
		    "command-error");
		cmdq_guard(item, "error", flags);
	} else
		cmdq_guard(item, "end", flags);
	return (retval);
}

/* Get a callback for the command queue. */
struct cmdq_item *
cmdq_get_callback1(const char *name, cmdq_cb cb, void *data)
{
	struct cmdq_item	*item;

	item = xcalloc(1, sizeof *item);
	xasprintf(&item->name, "[%s/%p]", name, item);
	item->type = CMDQ_CALLBACK;

	item->group = 0;
	item->state = cmdq_new_state(NULL, NULL, 0);

	item->cb = cb;
	item->data = data;

	return (item);
}

/* Generic error callback. */
static enum cmd_retval
cmdq_error_callback(struct cmdq_item *item, void *data)
{
	char	*error = data;

	cmdq_error(item, "%s", error);
	free(error);

	return (CMD_RETURN_NORMAL);
}

/* Get an error callback for the command queue. */
struct cmdq_item *
cmdq_get_error(const char *error)
{
	return (cmdq_get_callback(cmdq_error_callback, xstrdup(error)));
}

/* Fire callback on callback queue. */
static enum cmd_retval
cmdq_fire_callback(struct cmdq_item *item)
{
	return (item->cb(item, item->data));
}

/* Process next item on command queue. */
u_int
cmdq_next(struct client *c)
{
	struct cmdq_list	*queue = cmdq_get(c);
	const char		*name = cmdq_name(c);
	struct cmdq_item	*item;
	enum cmd_retval		 retval;
	u_int			 items = 0;
	static u_int		 number;

	if (TAILQ_EMPTY(&queue->list)) {
		log_debug("%s %s: empty", __func__, name);
		return (0);
	}
	if (TAILQ_FIRST(&queue->list)->flags & CMDQ_WAITING) {
		log_debug("%s %s: waiting", __func__, name);
		return (0);
	}

	log_debug("%s %s: enter", __func__, name);
	for (;;) {
		item = queue->item = TAILQ_FIRST(&queue->list);
		if (item == NULL)
			break;
		log_debug("%s %s: %s (%d), flags %x", __func__, name,
		    item->name, item->type, item->flags);

		/*
		 * Any item with the waiting flag set waits until an external
		 * event clears the flag (for example, a job - look at
		 * run-shell).
		 */
		if (item->flags & CMDQ_WAITING)
			goto waiting;

		/*
		 * Items are only fired once, once the fired flag is set, a
		 * waiting flag can only be cleared by an external event.
		 */
		if (~item->flags & CMDQ_FIRED) {
			item->time = time(NULL);
			item->number = ++number;

			switch (item->type) {
			case CMDQ_COMMAND:
				retval = cmdq_fire_command(item);

				/*
				 * If a command returns an error, remove any
				 * subsequent commands in the same group.
				 */
				if (retval == CMD_RETURN_ERROR)
					cmdq_remove_group(item);
				break;
			case CMDQ_CALLBACK:
				retval = cmdq_fire_callback(item);
				break;
			default:
				retval = CMD_RETURN_ERROR;
				break;
			}
			item->flags |= CMDQ_FIRED;

			if (retval == CMD_RETURN_WAIT) {
				item->flags |= CMDQ_WAITING;
				goto waiting;
			}
			items++;
		}
		cmdq_remove(item);
	}
	queue->item = NULL;

	log_debug("%s %s: exit (empty)", __func__, name);
	return (items);

waiting:
	log_debug("%s %s: exit (wait)", __func__, name);
	return (items);
}

/* Get running item if any. */
struct cmdq_item *
cmdq_running(struct client *c)
{
	struct cmdq_list	*queue = cmdq_get(c);

	if (queue->item == NULL)
		return (NULL);
	if (queue->item->flags & CMDQ_WAITING)
		return (NULL);
	return (queue->item);
}

/* Print a guard line. */
void
cmdq_guard(struct cmdq_item *item, const char *guard, int flags)
{
	struct client	*c = item->client;
	long		 t = item->time;
	u_int		 number = item->number;

	if (c != NULL && (c->flags & CLIENT_CONTROL))
		control_write(c, "%%%s %ld %u %d", guard, t, number, flags);
}

/* Show message from command. */
void
cmdq_print_data(struct cmdq_item *item, struct evbuffer *evb)
{
	server_client_print(item->client, 1, evb);
}

/* Show message from command. */
void
cmdq_print(struct cmdq_item *item, const char *fmt, ...)
{
	va_list		 ap;
	struct evbuffer	*evb;

	evb = evbuffer_new();
	if (evb == NULL)
		fatalx("out of memory");

	va_start(ap, fmt);
	evbuffer_add_vprintf(evb, fmt, ap);
	va_end(ap);

	cmdq_print_data(item, evb);
	evbuffer_free(evb);
}

/* Show error from command. */
void
cmdq_error(struct cmdq_item *item, const char *fmt, ...)
{
	struct client	*c = item->client;
	struct cmd	*cmd = item->cmd;
	va_list		 ap;
	char		*msg, *tmp;
	const char	*file;
	u_int		 line;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	log_debug("%s: %s", __func__, msg);

	if (c == NULL) {
		cmd_get_source(cmd, &file, &line);
		cfg_add_cause("%s:%u: %s", file, line, msg);
	} else if (c->session == NULL || (c->flags & CLIENT_CONTROL)) {
		server_add_message("%s message: %s", c->name, msg);
		if (~c->flags & CLIENT_UTF8) {
			tmp = msg;
			msg = utf8_sanitize(tmp);
			free(tmp);
		}
		if (c->flags & CLIENT_CONTROL)
			control_write(c, "%s", msg);
		else
			file_error(c, "%s\n", msg);
		c->retval = 1;
	} else {
		*msg = toupper((u_char) *msg);
		status_message_set(c, -1, 1, 0, "%s", msg);
	}

	free(msg);
}
