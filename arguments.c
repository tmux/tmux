/* $OpenBSD$ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Manipulate command arguments.
 */

/* List of argument values. */
TAILQ_HEAD(args_values, args_value);

/* Single arguments flag. */
struct args_entry {
	u_char			 flag;
	struct args_values	 values;
	u_int			 count;
	RB_ENTRY(args_entry)	 entry;
};

/* Parsed argument flags and values. */
struct args {
	struct args_tree	 tree;
	u_int			 count;
	struct args_value	*values;
};

/* Prepared command state. */
struct args_command_state {
	struct cmd_list		*cmdlist;
	char			*cmd;
	struct cmd_parse_input	 pi;
};

static struct args_entry	*args_find(struct args *, u_char);

static int	args_cmp(struct args_entry *, struct args_entry *);
RB_GENERATE_STATIC(args_tree, args_entry, entry, args_cmp);

/* Arguments tree comparison function. */
static int
args_cmp(struct args_entry *a1, struct args_entry *a2)
{
	return (a1->flag - a2->flag);
}

/* Find a flag in the arguments tree. */
static struct args_entry *
args_find(struct args *args, u_char flag)
{
	struct args_entry	entry;

	entry.flag = flag;
	return (RB_FIND(args_tree, &args->tree, &entry));
}

/* Copy value. */
static void
args_copy_value(struct args_value *to, struct args_value *from)
{
	to->type = from->type;
	switch (from->type) {
	case ARGS_NONE:
		break;
	case ARGS_COMMANDS:
		to->cmdlist = from->cmdlist;
		to->cmdlist->references++;
		break;
	case ARGS_STRING:
		to->string = xstrdup(from->string);
		break;
	}
}

/* Get value as string. */
static const char *
args_value_as_string(struct args_value *value)
{
	switch (value->type) {
	case ARGS_NONE:
		return ("");
	case ARGS_COMMANDS:
		if (value->cached == NULL)
			value->cached = cmd_list_print(value->cmdlist, 0);
		return (value->cached);
	case ARGS_STRING:
		return (value->string);
	}
	fatalx("unexpected argument type");
}

/* Create an empty arguments set. */
struct args *
args_create(void)
{
	struct args	 *args;

	args = xcalloc(1, sizeof *args);
	RB_INIT(&args->tree);
	return (args);
}

/* Parse arguments into a new argument set. */
struct args *
args_parse(const struct args_parse *parse, struct args_value *values,
    u_int count, char **cause)
{
	struct args		*args;
	u_int			 i;
	enum args_parse_type	 type;
	struct args_value	*value, *new;
	u_char			 flag;
	const char		*found, *string, *s;
	int			 optional_argument;

	if (count == 0)
		return (args_create());

	args = args_create();
	for (i = 1; i < count; /* nothing */) {
		value = &values[i];
		if (value->type != ARGS_STRING)
			break;

		string = value->string;
		if (*string++ != '-' || *string == '\0')
			break;
		i++;
		if (string[0] == '-' && string[1] == '\0')
			break;

		for (;;) {
			flag = *string++;
			if (flag == '\0')
				break;
			if (flag == '?') {
				args_free(args);
				return (NULL);
			}
			if (!isalnum(flag)) {
				xasprintf(cause, "invalid flag -%c", flag);
				args_free(args);
				return (NULL);
			}
			found = strchr(parse->template, flag);
			if (found == NULL) {
				xasprintf(cause, "unknown flag -%c", flag);
				args_free(args);
				return (NULL);
			}
			if (*++found != ':') {
				log_debug("%s: -%c", __func__, flag);
				args_set(args, flag, NULL);
				continue;
			}
			if (*found == ':') {
				optional_argument = 1;
				found++;
			}
			new = xcalloc(1, sizeof *new);
			if (*string != '\0') {
				new->type = ARGS_STRING;
				new->string = xstrdup(string);
			} else {
				if (i == count) {
					if (optional_argument) {
						log_debug("%s: -%c", __func__,
						    flag);
						args_set(args, flag, NULL);
						continue;
					}
					xasprintf(cause,
					    "-%c expects an argument",
					    flag);
					args_free(args);
					return (NULL);
				}
				if (values[i].type != ARGS_STRING) {
					xasprintf(cause,
					    "-%c argument must be a string",
					    flag);
					args_free(args);
					return (NULL);
				}
				args_copy_value(new, &values[i++]);
			}
			s = args_value_as_string(new);
			log_debug("%s: -%c = %s", __func__, flag, s);
			args_set(args, flag, new);
			break;
		}
	}
	log_debug("%s: flags end at %u of %u", __func__, i, count);
	if (i != count) {
		for (/* nothing */; i < count; i++) {
			value = &values[i];

			s = args_value_as_string(value);
			log_debug("%s: %u = %s (type %d)", __func__, i, s,
			    value->type);

			if (parse->cb != NULL) {
				type = parse->cb(args, args->count, cause);
				if (type == ARGS_PARSE_INVALID) {
					args_free(args);
					return (NULL);
				}
			} else
				type = ARGS_PARSE_STRING;

			args->values = xrecallocarray(args->values,
			    args->count, args->count + 1, sizeof *args->values);
			new = &args->values[args->count++];

			switch (type) {
			case ARGS_PARSE_INVALID:
				fatalx("unexpected argument type");
			case ARGS_PARSE_STRING:
				if (value->type != ARGS_STRING) {
					xasprintf(cause,
					    "argument %u must be \"string\"",
					    args->count);
					args_free(args);
					return (NULL);
				}
				args_copy_value(new, value);
				break;
			case ARGS_PARSE_COMMANDS_OR_STRING:
				args_copy_value(new, value);
				break;
			case ARGS_PARSE_COMMANDS:
				if (value->type != ARGS_COMMANDS) {
					xasprintf(cause,
					    "argument %u must be { commands }",
					    args->count);
					args_free(args);
					return (NULL);
				}
				args_copy_value(new, value);
				break;
			}
		}
	}

	if (parse->lower != -1 && args->count < (u_int)parse->lower) {
		xasprintf(cause,
		    "too few arguments (need at least %u)",
		    parse->lower);
		args_free(args);
		return (NULL);
	}
	if (parse->upper != -1 && args->count > (u_int)parse->upper) {
		xasprintf(cause,
		    "too many arguments (need at most %u)",
		    parse->upper);
		args_free(args);
		return (NULL);
	}
	return (args);
}

/* Copy and expand a value. */
static void
args_copy_copy_value(struct args_value *to, struct args_value *from, int argc,
    char **argv)
{
	char	*s, *expanded;
	int	 i;

	to->type = from->type;
	switch (from->type) {
	case ARGS_NONE:
		break;
	case ARGS_STRING:
		expanded = xstrdup(from->string);
		for (i = 0; i < argc; i++) {
			s = cmd_template_replace(expanded, argv[i], i + 1);
			free(expanded);
			expanded = s;
		}
		to->string = expanded;
		break;
	case ARGS_COMMANDS:
		to->cmdlist = cmd_list_copy(from->cmdlist, argc, argv);
		break;
	}
}

/* Copy an arguments set. */
struct args *
args_copy(struct args *args, int argc, char **argv)
{
	struct args		*new_args;
	struct args_entry	*entry;
	struct args_value	*value, *new_value;
	u_int			 i;

	cmd_log_argv(argc, argv, "%s", __func__);

	new_args = args_create();
	RB_FOREACH(entry, args_tree, &args->tree) {
		if (TAILQ_EMPTY(&entry->values)) {
			for (i = 0; i < entry->count; i++)
				args_set(new_args, entry->flag, NULL);
			continue;
		}
		TAILQ_FOREACH(value, &entry->values, entry) {
			new_value = xcalloc(1, sizeof *new_value);
			args_copy_copy_value(new_value, value, argc, argv);
			args_set(new_args, entry->flag, new_value);
		}
	}
	if (args->count == 0)
		return (new_args);
	new_args->count = args->count;
	new_args->values = xcalloc(args->count, sizeof *new_args->values);
	for (i = 0; i < args->count; i++) {
		new_value = &new_args->values[i];
		args_copy_copy_value(new_value, &args->values[i], argc, argv);
	}
	return (new_args);
}

/* Free a value. */
void
args_free_value(struct args_value *value)
{
	switch (value->type) {
	case ARGS_NONE:
		break;
	case ARGS_STRING:
		free(value->string);
		break;
	case ARGS_COMMANDS:
		cmd_list_free(value->cmdlist);
		break;
	}
	free(value->cached);
}

/* Free values. */
void
args_free_values(struct args_value *values, u_int count)
{
	u_int	i;

	for (i = 0; i < count; i++)
		args_free_value(&values[i]);
}

/* Free an arguments set. */
void
args_free(struct args *args)
{
	struct args_entry	*entry;
	struct args_entry	*entry1;
	struct args_value	*value;
	struct args_value	*value1;

	args_free_values(args->values, args->count);
	free(args->values);

	RB_FOREACH_SAFE(entry, args_tree, &args->tree, entry1) {
		RB_REMOVE(args_tree, &args->tree, entry);
		TAILQ_FOREACH_SAFE(value, &entry->values, entry, value1) {
			TAILQ_REMOVE(&entry->values, value, entry);
			args_free_value(value);
			free(value);
		}
		free(entry);
	}

	free(args);
}

/* Convert arguments to vector. */
void
args_to_vector(struct args *args, int *argc, char ***argv)
{
	char	*s;
	u_int	 i;

	*argc = 0;
	*argv = NULL;

	for (i = 0; i < args->count; i++) {
		switch (args->values[i].type) {
		case ARGS_NONE:
			break;
		case ARGS_STRING:
			cmd_append_argv(argc, argv, args->values[i].string);
			break;
		case ARGS_COMMANDS:
			s = cmd_list_print(args->values[i].cmdlist, 0);
			cmd_append_argv(argc, argv, s);
			free(s);
			break;
		}
	}
}

/* Convert arguments from vector. */
struct args_value *
args_from_vector(int argc, char **argv)
{
	struct args_value	*values;
	int			 i;

	values = xcalloc(argc, sizeof *values);
	for (i = 0; i < argc; i++) {
		values[i].type = ARGS_STRING;
		values[i].string = xstrdup(argv[i]);
	}
	return (values);
}

/* Add to string. */
static void printflike(3, 4)
args_print_add(char **buf, size_t *len, const char *fmt, ...)
{
	va_list	 ap;
	char	*s;
	size_t	 slen;

	va_start(ap, fmt);
	slen = xvasprintf(&s, fmt, ap);
	va_end(ap);

	*len += slen;
	*buf = xrealloc(*buf, *len);

	strlcat(*buf, s, *len);
	free(s);
}

/* Add value to string. */
static void
args_print_add_value(char **buf, size_t *len, struct args_value *value)
{
	char	*expanded = NULL;

	if (**buf != '\0')
		args_print_add(buf, len, " ");

	switch (value->type) {
	case ARGS_NONE:
		break;
	case ARGS_COMMANDS:
		expanded = cmd_list_print(value->cmdlist, 0);
		args_print_add(buf, len, "{ %s }", expanded);
		break;
	case ARGS_STRING:
		expanded = args_escape(value->string);
		args_print_add(buf, len, "%s", expanded);
		break;
	}
	free(expanded);
}

/* Print a set of arguments. */
char *
args_print(struct args *args)
{
	size_t			 len;
	char			*buf;
	u_int			 i, j;
	struct args_entry	*entry;
	struct args_value	*value;

	len = 1;
	buf = xcalloc(1, len);

	/* Process the flags first. */
	RB_FOREACH(entry, args_tree, &args->tree) {
		if (!TAILQ_EMPTY(&entry->values))
			continue;

		if (*buf == '\0')
			args_print_add(&buf, &len, "-");
		for (j = 0; j < entry->count; j++)
			args_print_add(&buf, &len, "%c", entry->flag);
	}

	/* Then the flags with arguments. */
	RB_FOREACH(entry, args_tree, &args->tree) {
		TAILQ_FOREACH(value, &entry->values, entry) {
			if (*buf != '\0')
				args_print_add(&buf, &len, " -%c", entry->flag);
			else
				args_print_add(&buf, &len, "-%c", entry->flag);
			args_print_add_value(&buf, &len, value);
		}
	}

	/* And finally the argument vector. */
	for (i = 0; i < args->count; i++)
		args_print_add_value(&buf, &len, &args->values[i]);

	return (buf);
}

/* Escape an argument. */
char *
args_escape(const char *s)
{
	static const char	 dquoted[] = " #';${}%";
	static const char	 squoted[] = " \"";
	char			*escaped, *result;
	int			 flags, quotes = 0;

	if (*s == '\0') {
		xasprintf(&result, "''");
		return (result);
	}
	if (s[strcspn(s, dquoted)] != '\0')
		quotes = '"';
	else if (s[strcspn(s, squoted)] != '\0')
		quotes = '\'';

	if (s[0] != ' ' &&
	    s[1] == '\0' &&
	    (quotes != 0 || s[0] == '~')) {
		xasprintf(&escaped, "\\%c", s[0]);
		return (escaped);
	}

	flags = VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL;
	if (quotes == '"')
		flags |= VIS_DQ;
	utf8_stravis(&escaped, s, flags);

	if (quotes == '\'')
		xasprintf(&result, "'%s'", escaped);
	else if (quotes == '"') {
		if (*escaped == '~')
			xasprintf(&result, "\"\\%s\"", escaped);
		else
			xasprintf(&result, "\"%s\"", escaped);
	} else {
		if (*escaped == '~')
			xasprintf(&result, "\\%s", escaped);
		else
			result = xstrdup(escaped);
	}
	free(escaped);
	return (result);
}

/* Return if an argument is present. */
int
args_has(struct args *args, u_char flag)
{
	struct args_entry	*entry;

	entry = args_find(args, flag);
	if (entry == NULL)
		return (0);
	return (entry->count);
}

/* Set argument value in the arguments tree. */
void
args_set(struct args *args, u_char flag, struct args_value *value)
{
	struct args_entry	*entry;

	entry = args_find(args, flag);
	if (entry == NULL) {
		entry = xcalloc(1, sizeof *entry);
		entry->flag = flag;
		entry->count = 1;
		TAILQ_INIT(&entry->values);
		RB_INSERT(args_tree, &args->tree, entry);
	} else
		entry->count++;
	if (value != NULL && value->type != ARGS_NONE)
		TAILQ_INSERT_TAIL(&entry->values, value, entry);
}

/* Get argument value. Will be NULL if it isn't present. */
const char *
args_get(struct args *args, u_char flag)
{
	struct args_entry	*entry;

	if ((entry = args_find(args, flag)) == NULL)
		return (NULL);
	if (TAILQ_EMPTY(&entry->values))
		return (NULL);
	return (TAILQ_LAST(&entry->values, args_values)->string);
}

/* Get first argument. */
u_char
args_first(struct args *args, struct args_entry **entry)
{
	*entry = RB_MIN(args_tree, &args->tree);
	if (*entry == NULL)
		return (0);
	return ((*entry)->flag);
}

/* Get next argument. */
u_char
args_next(struct args_entry **entry)
{
	*entry = RB_NEXT(args_tree, &args->tree, *entry);
	if (*entry == NULL)
		return (0);
	return ((*entry)->flag);
}

/* Get argument count. */
u_int
args_count(struct args *args)
{
	return (args->count);
}

/* Get argument values. */
struct args_value *
args_values(struct args *args)
{
	return (args->values);
}

/* Get argument value. */
struct args_value *
args_value(struct args *args, u_int idx)
{
	if (idx >= args->count)
		return (NULL);
	return (&args->values[idx]);
}

/* Return argument as string. */
const char *
args_string(struct args *args, u_int idx)
{
	if (idx >= args->count)
		return (NULL);
	return (args_value_as_string(&args->values[idx]));
}

/* Make a command now. */
struct cmd_list *
args_make_commands_now(struct cmd *self, struct cmdq_item *item, u_int idx,
    int expand)
{
	struct args_command_state	*state;
	char				*error;
	struct cmd_list			*cmdlist;

	state = args_make_commands_prepare(self, item, idx, NULL, 0, expand);
	cmdlist = args_make_commands(state, 0, NULL, &error);
	if (cmdlist == NULL) {
		cmdq_error(item, "%s", error);
		free(error);
	}
	else
		cmdlist->references++;
	args_make_commands_free(state);
	return (cmdlist);
}

/* Save bits to make a command later. */
struct args_command_state *
args_make_commands_prepare(struct cmd *self, struct cmdq_item *item, u_int idx,
    const char *default_command, int wait, int expand)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct client			*tc = cmdq_get_target_client(item);
	struct args_value		*value;
	struct args_command_state	*state;
	const char			*cmd;

	state = xcalloc(1, sizeof *state);

	if (idx < args->count) {
		value = &args->values[idx];
		if (value->type == ARGS_COMMANDS) {
			state->cmdlist = value->cmdlist;
			state->cmdlist->references++;
			return (state);
		}
		cmd = value->string;
	} else {
		if (default_command == NULL)
			fatalx("argument out of range");
		cmd = default_command;
	}


	if (expand)
		state->cmd = format_single_from_target(item, cmd);
	else
		state->cmd = xstrdup(cmd);
	log_debug("%s: %s", __func__, state->cmd);

	if (wait)
		state->pi.item = item;
	cmd_get_source(self, &state->pi.file, &state->pi.line);
	state->pi.c = tc;
	if (state->pi.c != NULL)
		state->pi.c->references++;
	cmd_find_copy_state(&state->pi.fs, target);

	return (state);
}

/* Return argument as command. */
struct cmd_list *
args_make_commands(struct args_command_state *state, int argc, char **argv,
    char **error)
{
	struct cmd_parse_result	*pr;
	char			*cmd, *new_cmd;
	int			 i;

	if (state->cmdlist != NULL) {
		if (argc == 0)
			return (state->cmdlist);
		return (cmd_list_copy(state->cmdlist, argc, argv));
	}

	cmd = xstrdup(state->cmd);
	for (i = 0; i < argc; i++) {
		new_cmd = cmd_template_replace(cmd, argv[i], i + 1);
		log_debug("%s: %%%u %s: %s", __func__, i + 1, argv[i], new_cmd);
		free(cmd);
		cmd = new_cmd;
	}
	log_debug("%s: %s", __func__, cmd);

	pr = cmd_parse_from_string(cmd, &state->pi);
	free(cmd);
	switch (pr->status) {
	case CMD_PARSE_ERROR:
		*error = pr->error;
		return (NULL);
	case CMD_PARSE_SUCCESS:
		return (pr->cmdlist);
	}
	fatalx("invalid parse return state");
}

/* Free commands state. */
void
args_make_commands_free(struct args_command_state *state)
{
	if (state->cmdlist != NULL)
		cmd_list_free(state->cmdlist);
	if (state->pi.c != NULL)
		server_client_unref(state->pi.c);
	free(state->cmd);
	free(state);
}

/* Get prepared command. */
char *
args_make_commands_get_command(struct args_command_state *state)
{
	struct cmd	*first;
	int		 n;
	char		*s;

	if (state->cmdlist != NULL) {
		first = cmd_list_first(state->cmdlist);
		if (first == NULL)
			return (xstrdup(""));
		return (xstrdup(cmd_get_entry(first)->name));
	}
	n = strcspn(state->cmd, " ,");
	xasprintf(&s, "%.*s", n, state->cmd);
	return (s);
}

/* Get first value in argument. */
struct args_value *
args_first_value(struct args *args, u_char flag)
{
	struct args_entry	*entry;

	if ((entry = args_find(args, flag)) == NULL)
		return (NULL);
	return (TAILQ_FIRST(&entry->values));
}

/* Get next value in argument. */
struct args_value *
args_next_value(struct args_value *value)
{
	return (TAILQ_NEXT(value, entry));
}

/* Convert an argument value to a number. */
long long
args_strtonum(struct args *args, u_char flag, long long minval,
    long long maxval, char **cause)
{
	const char		*errstr;
	long long		 ll;
	struct args_entry	*entry;
	struct args_value	*value;

	if ((entry = args_find(args, flag)) == NULL) {
		*cause = xstrdup("missing");
		return (0);
	}
	value = TAILQ_LAST(&entry->values, args_values);
	if (value == NULL ||
	    value->type != ARGS_STRING ||
	    value->string == NULL) {
		*cause = xstrdup("missing");
		return (0);
	}

	ll = strtonum(value->string, minval, maxval, &errstr);
	if (errstr != NULL) {
		*cause = xstrdup(errstr);
		return (0);
	}

	*cause = NULL;
	return (ll);
}

/* Convert an argument to a number which may be a percentage. */
long long
args_percentage(struct args *args, u_char flag, long long minval,
    long long maxval, long long curval, char **cause)
{
	const char		*value;
	struct args_entry	*entry;

	if ((entry = args_find(args, flag)) == NULL) {
		*cause = xstrdup("missing");
		return (0);
	}
	value = TAILQ_LAST(&entry->values, args_values)->string;
	return (args_string_percentage(value, minval, maxval, curval, cause));
}

/* Convert a string to a number which may be a percentage. */
long long
args_string_percentage(const char *value, long long minval, long long maxval,
    long long curval, char **cause)
{
	const char	*errstr;
	long long	 ll;
	size_t		 valuelen = strlen(value);
	char		*copy;

	if (value[valuelen - 1] == '%') {
		copy = xstrdup(value);
		copy[valuelen - 1] = '\0';

		ll = strtonum(copy, 0, 100, &errstr);
		free(copy);
		if (errstr != NULL) {
			*cause = xstrdup(errstr);
			return (0);
		}
		ll = (curval * ll) / 100;
		if (ll < minval) {
			*cause = xstrdup("too small");
			return (0);
		}
		if (ll > maxval) {
			*cause = xstrdup("too large");
			return (0);
		}
	} else {
		ll = strtonum(value, minval, maxval, &errstr);
		if (errstr != NULL) {
			*cause = xstrdup(errstr);
			return (0);
		}
	}

	*cause = NULL;
	return (ll);
}
